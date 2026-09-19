/**
 * @file file_logger.cpp
 * @brief Asynchronous, thread-safe file logger implementation
 *
 * Lock-free producer-consumer pattern with a double-buffer and a single
 * background writer task. No mutex is used anywhere in the producer path.
 * All public functions are wrapped in try/catch for crash safety.
 */

#include "file_logger.h"
#include "robot_config.h"

#include "pros/rtos.hpp"

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

// ============================================================================
// Data Structures
// ============================================================================

struct LogEntry {
  char message[128];
  uint8_t length;
  std::atomic<bool> ready{false};
};

static constexpr int BUFFER_SIZE = 64;

static LogEntry bufferA[BUFFER_SIZE];
static LogEntry bufferB[BUFFER_SIZE];
static LogEntry *buffers[2] = {bufferA, bufferB};

// Packed atomic state: [bit 31: active buffer index] [bits 30..0: write count]
static std::atomic<uint32_t> bufferState{0};

static constexpr uint32_t INDEX_BIT = 0x80000000;
static constexpr uint32_t COUNT_MASK = 0x7FFFFFFF;

// ============================================================================
// Logger State
// ============================================================================

static FILE *logFile = nullptr;
static pros::Task *writerTask = nullptr;
static volatile bool loggerActive = false;
static volatile bool writerDone = false;
static char logFilePath[64];
static uint32_t initMicros = 0;
static std::atomic<int> dropCount{0};

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * Format elapsed time since logger init as SSS.mmm
 */
static int formatTimestamp(char *buf, int bufSize) {
  uint32_t elapsed_us = pros::micros() - initMicros;
  uint32_t total_sec = elapsed_us / 1000000;
  uint32_t frac_ms = (elapsed_us % 1000000) / 1000;

  return snprintf(buf, bufSize, "%03lu.%03lu", (unsigned long)total_sec,
                  (unsigned long)frac_ms);
}

/**
 * Lock-free enqueue: claim a slot via fetch_add, write entry, set ready flag.
 */
static bool enqueueEntry(const char *msg, int len) {
  uint32_t prev = bufferState.fetch_add(1, std::memory_order_relaxed);
  int bufIdx = (prev & INDEX_BIT) ? 1 : 0;
  int slot = prev & COUNT_MASK;

  if (slot >= BUFFER_SIZE) {
    // Buffer full — don't undo (would race with swap). Writer handles overflow.
    dropCount.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  memcpy(buffers[bufIdx][slot].message, msg, len);
  buffers[bufIdx][slot].length = (uint8_t)len;
  buffers[bufIdx][slot].ready.store(true, std::memory_order_release);
  return true;
}

/**
 * Background writer task: periodically swap buffers and drain the inactive one.
 */
static void writerTaskFn() {
  writerDone = false;
  while (loggerActive) {
    // 1. Read current state
    uint32_t snapshot = bufferState.load(std::memory_order_relaxed);
    int activeIdx = (snapshot & INDEX_BIT) ? 1 : 0;
    int count = snapshot & COUNT_MASK;

    if (count > 0) {
      // 2. Swap to the other buffer (new producers will write there)
      int newIdx = 1 - activeIdx;
      uint32_t newState = newIdx ? INDEX_BIT : 0; // Other buffer, count = 0

      while (!bufferState.compare_exchange_weak(snapshot, newState,
                                                std::memory_order_acq_rel)) {
        // CAS failed — a producer snuck in. Retry with updated snapshot.
        activeIdx = (snapshot & INDEX_BIT) ? 1 : 0;
        count = snapshot & COUNT_MASK;
        newIdx = 1 - activeIdx;
        newState = newIdx ? INDEX_BIT : 0;
      }
      // snapshot now holds the final state of the old buffer
      count = snapshot & COUNT_MASK;

      // 3. Drain the old (now inactive) buffer
      int toWrite = (count > BUFFER_SIZE) ? BUFFER_SIZE : count;
      for (int i = 0; i < toWrite; i++) {
        // Spin-wait for in-flight writes
        while (!buffers[activeIdx][i].ready.load(std::memory_order_acquire)) {
        }

        if (logFile) {
          fwrite(buffers[activeIdx][i].message, 1,
                 buffers[activeIdx][i].length, logFile);
        }
        buffers[activeIdx][i].ready.store(false, std::memory_order_relaxed);
      }

      // 4. Track drops from overflow
      if (count > BUFFER_SIZE) {
        dropCount.fetch_add(count - BUFFER_SIZE, std::memory_order_relaxed);
      }
    }

    if (logFile)
      fflush(logFile);
    pros::delay(50);
  }
  writerDone = true;
}

/**
 * Reset all buffer entries and atomic state.
 */
static void resetBuffers() {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    bufferA[i].ready.store(false, std::memory_order_relaxed);
    bufferA[i].length = 0;
    bufferB[i].ready.store(false, std::memory_order_relaxed);
    bufferB[i].length = 0;
  }
  bufferState.store(0, std::memory_order_relaxed);
  dropCount.store(0, std::memory_order_relaxed);
}

// ============================================================================
// Public API
// ============================================================================

bool fileLoggerInit(AutonomousMode mode, const AutonConfig *config) {
  try {
    if (loggerActive)
      return true;

    // 1. Reset buffers
    resetBuffers();

    // 2. Capture init time
    initMicros = pros::micros();

    // 3. Build filename from auton name
    const char *name = getAutonName(mode);
    char safeName[32];
    int ni = 0;
    for (int i = 0; name[i] && ni < (int)sizeof(safeName) - 1; i++) {
      if (name[i] == ' ')
        safeName[ni++] = '_';
      else
        safeName[ni++] = (char)tolower((unsigned char)name[i]);
    }
    safeName[ni] = '\0';

    // 4. Get timestamp from brain clock
    char timeStr[16] = "0101_000000";
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    if (tm) {
      snprintf(timeStr, sizeof(timeStr), "%02d%02d_%02d%02d%02d",
               tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
               tm->tm_sec);
    }

    // 5. Open file in /usd/logs/
    // NOTE: The /usd/logs/ directory must exist on the SD card.
    // PROS does not expose mkdir — create the folder manually on a computer
    // before first use. If the directory doesn't exist, fopen fails silently
    // and the logger stays inactive (no crash, no interference).
    snprintf(logFilePath, sizeof(logFilePath), "/usd/logs/%s_%s.csv", safeName,
             timeStr);

    logFile = fopen(logFilePath, "w");
    if (!logFile)
      return false;

    // 6. Write header
    const char *colorName = config ? getColorName(config->teamColor) : "N/A";
    const char *sensorStr =
        config ? (config->useColorSensor ? "ON" : "OFF") : "N/A";
    const char *aggrStr = config ? (config->aggressive ? "ON" : "OFF") : "N/A";
    const char *pushStr =
        config ? (config->pushAlliance ? "ON" : "OFF") : "N/A";
    fprintf(logFile, "# mode=%s color=%s sensor=%s aggr=%s push=%s\n",
            getAutonName(mode), colorName, sensorStr, aggrStr, pushStr);
    fprintf(logFile, "elapsed,label,x,y,heading\n");
    fflush(logFile);

    // 7. Start writer task
    loggerActive = true;
    writerTask = new pros::Task([]() { writerTaskFn(); });

    return true;
  } catch (...) {
    return false;
  }
}

void fileLoggerShutdown() {
  try {
    if (!loggerActive)
      return;

    // 1. Signal writer to stop
    loggerActive = false;

    // 2. Wait for writer task to exit (up to 500ms)
    if (writerTask) {
      for (int i = 0; i < 50 && !writerDone; i++) {
        pros::delay(10);
      }
      delete writerTask;
      writerTask = nullptr;
    }

    // 3. Drain any remaining entries from BOTH buffers
    //    The writer may not have had CPU time to flush (single-core V5),
    //    or may have swapped buffers before exiting. Check both.
    for (int buf = 0; buf < 2; buf++) {
      for (int i = 0; i < BUFFER_SIZE; i++) {
        if (buffers[buf][i].ready.load(std::memory_order_acquire)) {
          if (logFile) {
            fwrite(buffers[buf][i].message, 1, buffers[buf][i].length,
                   logFile);
          }
          buffers[buf][i].ready.store(false, std::memory_order_relaxed);
        }
      }
    }

    // 4. Write footer
    if (logFile) {
      char ts[24];
      formatTimestamp(ts, sizeof(ts));
      fprintf(logFile, "# end=%s dropped=%d\n", ts,
              dropCount.load(std::memory_order_relaxed));
      fflush(logFile);
      fclose(logFile);
      logFile = nullptr;
    }
  } catch (...) {
    // Never crash the caller
  }
}

void fileLog(const char *fmt, ...) {
  try {
    if (!loggerActive || !fmt)
      return;

    char buf[128];
    char ts[24];
    formatTimestamp(ts, sizeof(ts));
    int off = snprintf(buf, sizeof(buf), "%s,", ts);

    va_list args;
    va_start(args, fmt);
    int len = off + vsnprintf(buf + off, sizeof(buf) - off, fmt, args);
    va_end(args);

    if (len < 0)
      return;
    if (len >= (int)sizeof(buf))
      len = sizeof(buf) - 1;

    // Auto-append newline
    if (len == 0 || buf[len - 1] != '\n') {
      if (len < (int)sizeof(buf) - 1) {
        buf[len++] = '\n';
        buf[len] = '\0';
      }
    }
    enqueueEntry(buf, len);
  } catch (...) {
    // Never crash the caller
  }
}

void fileLogPose(const char *label) {
  try {
    if (!loggerActive || !label)
      return;

    lemlib::Pose pose = chassis.getPose();

    char ts[24];
    formatTimestamp(ts, sizeof(ts));

    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%s,%s,%.1f,%.1f,%.1f\n", ts, label,
                       pose.x, pose.y, pose.theta);

    if (len > 0 && len < (int)sizeof(buf)) {
      enqueueEntry(buf, len);
    }
  } catch (...) {
    // Never crash the caller
  }
}
