# File Logger — Design Document

## Overview

Asynchronous, thread-safe file logger that writes log data to the SD card (`/usd/`) during autonomous and driver control. Replaces ephemeral screen-only debugging with persistent CSV files for post-match analysis.

### Goals

- **Non-blocking**: Callers never wait on SD card I/O
- **Thread-safe**: Multiple `pros::Task` threads can log concurrently
- **Simple API**: printf-style low-level logging + one-call pose logging
- **Crash-safe**: Every public function is wrapped so that no exception or error can propagate to the caller — all failures are silent no-ops
- **Competition-ready**: Silent failure if no SD card; never interferes with robot operation

## Architecture

Lock-free producer-consumer pattern with a **double-buffer** and a single background writer task. No mutex is used anywhere in the producer (caller) path.

```
 Caller threads                         Background writer task
 ┌──────────┐                          ┌──────────────────────┐
 │ auton    │──┐                       │                      │
 │ opcontrol│──┤── fetch_add ──> [ Active Buffer ] ←──swap──→ [ Flush Buffer ] ──> writer ──> .csv
 │ debug    │──┘  (lock-free)          │                (50ms loop)              │
 └──────────┘                          └──────────────────────┘
```

- **Callers** do a single `std::atomic<uint32_t>::fetch_add(1)` to claim a slot in the active buffer. This is one CPU instruction — **zero blocking, zero contention, no mutex**.
- **Writer task** periodically swaps which buffer is active (via `compare_exchange`), then drains the now-inactive buffer to the `FILE*`. Producers and the writer never touch the same buffer simultaneously.
- Messages are only dropped when the active buffer is truly full (64 entries accumulated between writer cycles). Mutex contention can never cause drops.

## Data Structures

All state is file-scoped (`static`) in `file_logger.cpp`. Nothing leaks into the header.

### LogEntry

```cpp
struct LogEntry {
    char message[128];           // Pre-formatted log line including newline
    uint8_t length;              // Actual string length (avoids strlen in writer)
    std::atomic<bool> ready{false};  // Set to true after producer finishes writing
};
```

128 bytes is sufficient for pose lines like `083.456,SCORE,123.4,-56.7,270.3\n`. Messages exceeding 127 characters are truncated by `vsnprintf`.

The `ready` flag solves the in-flight write problem: a producer that has claimed a slot but hasn't finished writing will have `ready == false`. The writer waits for it before reading.

### Double-Buffer

```cpp
static constexpr int BUFFER_SIZE = 64;

static LogEntry bufferA[BUFFER_SIZE];
static LogEntry bufferB[BUFFER_SIZE];
static LogEntry *buffers[2] = {bufferA, bufferB};
```

Two identically-sized flat arrays. At any moment, one is the "active" buffer (producers write to it) and the other is the "flush" buffer (writer drains it). They swap roles every writer cycle.

Total memory: 2 × 64 × ~130 bytes ≈ **16 KB** for the buffers.

### Packed Atomic State

```cpp
// Bit layout: [bit 31: active buffer index (0 or 1)] [bits 30..0: write count]
static std::atomic<uint32_t> bufferState{0};

static constexpr uint32_t INDEX_BIT = 0x80000000;  // Bit 31
static constexpr uint32_t COUNT_MASK = 0x7FFFFFFF;  // Bits 30..0
```

This is the key to lock-free operation. A single `fetch_add(1)` on `bufferState` atomically:
1. Claims the next slot (increments the count)
2. Reads which buffer is active (bit 31)

Both happen in **one indivisible CPU instruction** (ARM `LDREX`/`STREX`). No mutex, no contention.

**When full:** If `slot >= BUFFER_SIZE`, the message is dropped. The count is allowed to exceed `BUFFER_SIZE` (the writer uses `min(count, BUFFER_SIZE)` to know the real count and the overflow to track drops). This avoids a `fetch_sub` which could race with a buffer swap.

### Logger State

```cpp
static FILE *logFile = nullptr;
static pros::Task *writerTask = nullptr;
static volatile bool loggerActive = false;
static char logFilePath[64];
static uint32_t initMicros = 0;     // pros::micros() captured at init (for elapsed timestamps)
static volatile int dropCount = 0;  // Total messages dropped (buffer full)
```

Note: **no `pros::Mutex`** anywhere in the logger state. Thread safety is achieved entirely through atomics.

## Public API

Header file: `include/file_logger.h`

### Initialization & Shutdown

```cpp
/**
 * Initialize the file logger for the given autonomous mode.
 * Creates /usd/logs/ directory if needed, opens the log file,
 * writes a CSV header, and starts the background writer task.
 *
 * @param mode    The autonomous mode (used for file naming)
 * @param config  Pointer to auton config (logged in header metadata)
 * @return true if logger started successfully, false if SD card unavailable
 */
bool fileLoggerInit(AutonomousMode mode, const AutonConfig *config = nullptr);

/**
 * Flush remaining buffered messages and close the log file.
 * Blocks briefly until the buffer is drained (up to 500ms timeout).
 * Safe to call multiple times or if logger was never initialized.
 */
void fileLoggerShutdown();
```

### Low-Level Logging

```cpp
/**
 * Log a formatted message (printf-style).
 * Non-blocking: formats the message and enqueues it.
 * Automatically appends a newline if not present.
 * Silently drops the message if the buffer is full.
 *
 * @param fmt  printf format string
 * @param ...  format arguments
 */
void fileLog(const char *fmt, ...);
```

### High-Level Convenience Logging

```cpp
/**
 * Log the current robot pose (x, y, heading) with a label and timestamp.
 * Output format: <elapsed_seconds.milliseconds>,<label>,<x>,<y>,<heading>
 *
 * @param label  Short descriptive label (e.g., "SCORE", "LOADER1", "TURN")
 */
void fileLogPose(const char *label);
```

### API Design Rationale

- **Prefix `fileLog`/`fileLogger`**: Avoids collisions with LemLib's internal logger or future PROS log functions.
- **Free functions, not a class**: Matches existing codebase style (`debug_output.h`, `movement_utils.h`, `roller_control.h`). A singleton class would be unusual here and adds no benefit for a single-instance logger.

## File Naming

### Path Convention

`/usd/logs/<lowercase_mode_name>_<MMDD>_<HHMM>.csv`

The `getAutonName()` return value is converted: lowercase, spaces become underscores. A timestamp (month-day, hour-minute) from the V5 brain's clock is appended so that each run produces a unique file and previous logs are never overwritten.

| AutonomousMode | getAutonName() | Example Log File Path |
|---|---|---|
| `ALLIANCE_LEFT_SAFE` | `"LEFT SAFE"` | `/usd/logs/left_safe_0304_1430.csv` |
| `ALLIANCE_LEFT_AGGRESSIVE` | `"LEFT AGGR"` | `/usd/logs/left_aggr_0304_1431.csv` |
| `ALLIANCE_RIGHT` | `"RIGHT"` | `/usd/logs/right_0304_1505.csv` |
| `ALLIANCE_SOLO` | `"SOLO AWP"` | `/usd/logs/solo_awp_0304_1510.csv` |
| `SKILLS` | `"SKILLS"` | `/usd/logs/skills_0304_1520.csv` |

The timestamp is generated using `time()` + `localtime()` at initialization. If the brain's clock is not set, the timestamp defaults to `0101_0000`.

### No Overwrite — One File Per Run

Each run creates a **new file** with its own timestamp. Previous log files are preserved on the SD card. This allows comparing runs across a competition day. Old files can be cleaned up manually by deleting the `/usd/logs/` folder contents between events.

> **Note:** Ensure the V5 brain's date/time is set for meaningful filenames. The clock can be set via the brain's settings menu.

## Log File Format

CSV format, directly openable in Excel/Google Sheets/Python:

```csv
# mode=SKILLS color=RED sensor=OFF
elapsed,label,x,y,heading
000.000,START,0.0,0.0,0.0
000.152,APPROACH_LOADER,-13.5,1.7,0.0
001.043,AT_LOADER,-13.5,-27.8,0.1
001.044,loader_deploy
002.500,LOADERS_DONE,-1.5,-29.5,90.0
# end=058.234 dropped=0
```

- **Header comment** (`#`): Records metadata — auton mode, team color, color sensor setting.
- **CSV header**: `elapsed,label,x,y,heading`
- **Pose lines**: `<SSS.mmm>,<label>,<x>,<y>,<heading>` — from `fileLogPose()` calls. Elapsed seconds (3-digit) with milliseconds (3-digit) since `fileLoggerInit()`.
- **Free-form lines**: Raw text from `fileLog()` calls, interspersed with CSV data.
- **Footer comment**: Total elapsed time and number of dropped messages.

### Timestamp Computation

Elapsed time since `fileLoggerInit()`, formatted as `SSS.mmm` (3-digit seconds, dot, 3-digit milliseconds). Max auton is 120 seconds so 3 digits is sufficient. Computed from a single value captured at init:
- `initMicros` — `pros::micros()` value (monotonic microsecond counter)

At each log call:
```
elapsed_us  = pros::micros() - initMicros
total_sec   = elapsed_us / 1_000_000
frac_ms     = (elapsed_us % 1_000_000) / 1_000
```

Simple, precise, and no dependency on the brain's wall clock being set correctly.

## Implementation Details

### Enqueue (Lock-Free Producer)

Callers **never block and never contend with the writer**. A single `fetch_add` claims a slot and reads the active buffer index atomically.

```cpp
static bool enqueueEntry(const char *msg, int len) {
    uint32_t prev = bufferState.fetch_add(1, std::memory_order_relaxed);
    int bufIdx = (prev & INDEX_BIT) ? 1 : 0;
    int slot = prev & COUNT_MASK;

    if (slot >= BUFFER_SIZE) {
        // Buffer full — don't undo (would race with swap). Writer handles overflow.
        dropCount++;
        return false;
    }

    memcpy(buffers[bufIdx][slot].message, msg, len);
    buffers[bufIdx][slot].length = (uint8_t)len;
    buffers[bufIdx][slot].ready.store(true, std::memory_order_release);  // Signal: entry complete
    return true;
}
```

**Why no `fetch_sub` on overflow?** After the writer swaps buffers, `bufferState` points to the new buffer. A `fetch_sub` from a producer that read the OLD buffer index would decrement the NEW buffer's count — corrupting it. Instead, we let the count exceed `BUFFER_SIZE` and the writer computes `dropped = count - BUFFER_SIZE`.

### Background Writer Task

The writer atomically swaps the active buffer via `compare_exchange`, then drains the now-inactive buffer at its own pace. No producer can touch the inactive buffer.

```cpp
static void writerTaskFn() {
    while (loggerActive) {
        // 1. Read current state
        uint32_t snapshot = bufferState.load(std::memory_order_relaxed);
        int activeIdx = (snapshot & INDEX_BIT) ? 1 : 0;
        int count = snapshot & COUNT_MASK;

        if (count > 0) {
            // 2. Swap to the other buffer (new producers will write there)
            int newIdx = 1 - activeIdx;
            uint32_t newState = newIdx ? INDEX_BIT : 0;  // Other buffer, count = 0

            while (!bufferState.compare_exchange_weak(
                       snapshot, newState, std::memory_order_acq_rel)) {
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
                // Spin-wait for in-flight writes (producer claimed slot but hasn't set ready)
                while (!buffers[activeIdx][i].ready.load(std::memory_order_acquire)) {}

                if (logFile) {
                    fwrite(buffers[activeIdx][i].message, 1,
                           buffers[activeIdx][i].length, logFile);
                }
                buffers[activeIdx][i].ready.store(false, std::memory_order_relaxed);
            }

            // 4. Track drops from overflow
            if (count > BUFFER_SIZE) {
                dropCount += (count - BUFFER_SIZE);
            }
        }

        if (logFile) fflush(logFile);
        pros::delay(50);
    }
}
```

**Why this is safe:**
- After the CAS swap, `bufferState` points to the NEW buffer. All new `fetch_add` calls go to the new buffer.
- A producer that called `fetch_add` BEFORE the swap got the OLD buffer index from its return value. It writes to the old buffer, sets `ready = true`. The writer's spin-wait picks it up.
- The producer's `fetch_add` and the writer's `compare_exchange` are both atomic operations on the same `bufferState` — the CPU guarantees they are serialized.
- The CAS retry loop only runs when a producer's `fetch_add` races with the swap. This resolves in 1-2 retries at most.
- The `ready` flag spin-wait handles the window between a producer's `fetch_add` and its `memcpy + ready = true`. On single-core (V5 brain), this resolves within one RTOS tick.

### Timestamp Helper

Computes elapsed `SSS.mmm` since logger init:

```cpp
static int formatTimestamp(char *buf, int bufSize) {
    uint32_t elapsed_us = pros::micros() - initMicros;
    uint32_t total_sec = elapsed_us / 1000000;
    uint32_t frac_ms = (elapsed_us % 1000000) / 1000;

    return snprintf(buf, bufSize, "%03lu.%03lu",
                    (unsigned long)total_sec, (unsigned long)frac_ms);
}
```

### fileLog Implementation

Every public function is wrapped so no exception or error can crash the caller:

```cpp
void fileLog(const char *fmt, ...) {
    try {
        if (!loggerActive || !fmt) return;

        char buf[128];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if (len < 0) return;
        if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;

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
```

### fileLogPose Implementation

```cpp
void fileLogPose(const char *label) {
    try {
        if (!loggerActive || !label) return;

        lemlib::Pose pose = chassis.getPose();

        char ts[24];
        formatTimestamp(ts, sizeof(ts));

        char buf[128];
        int len = snprintf(buf, sizeof(buf), "%s,%s,%.1f,%.1f,%.1f\n",
                           ts, label, pose.x, pose.y, pose.theta);

        if (len > 0 && len < (int)sizeof(buf)) {
            enqueueEntry(buf, len);
        }
    } catch (...) {
        // Never crash the caller
    }
}
```

## Initialization Flow

1. Check if logger is already active (idempotent — return true)
2. Reset both buffers (all `ready` flags to false), reset `bufferState` to 0, reset `dropCount` to 0
3. Capture `initMicros = pros::micros()` (elapsed time anchor for log entries)
4. Build filename: `getAutonName(mode)` → lowercase, spaces→underscores, append `_MMDD_HHMM` from `time()` + `localtime()` → `/usd/logs/<name>_0304_1430.csv`
5. Open file with `fopen(path, "w")`
6. If fopen fails, return false (no SD card)
7. Write header: `# mode=... color=... sensor=...\n` and `elapsed,label,x,y,heading\n`
8. `fflush(logFile)`
9. Set `loggerActive = true`
10. Start writer task: `new pros::Task(writerTaskFn, ...)`
11. Return true

The entire init function is wrapped in `try/catch(...)` — if anything fails unexpectedly, it returns false and the logger stays inactive.

## Shutdown Flow

1. If `!loggerActive`, return (safe no-op)
2. Set `loggerActive = false`
3. Wait for writer task to exit (up to 500ms)
4. Delete writer task, set to nullptr
5. Drain any remaining entries directly
6. Write footer: `# end=<SSS.mmm> dropped=<dropCount>\n`
7. `fflush()` and `fclose()`

The entire shutdown function is wrapped in `try/catch(...)` — always safe to call.

## Integration Points

### `src/competition.cpp`

```cpp
#include "file_logger.h"

void autonomous() {
    descorerActuator.deploy();
    wheelupActuator.retract();
    fileLoggerInit(selectedAutonMode, &autonConfig);  // ADD
    // ... existing switch on selectedAutonMode ...
}

void disabled() {
    fileLoggerShutdown();  // ADD (safe even if never initialized)
}
```

### Auton files (optional, incremental)

```cpp
// In skill_aggr.cpp or any auton file:
fileLogPose("START");
// ... movement code ...
fileLogPose("LOADERS_DONE");
// ... more movement ...
fileLogPose("ALIGNED");
```

## Error Handling & Crash Safety

**Core guarantee:** No public function in this module can ever crash the application. Every public function (`fileLoggerInit`, `fileLoggerShutdown`, `fileLog`, `fileLogPose`) is wrapped in `try { ... } catch (...) {}` as a last-resort safety net, in addition to explicit null/bounds checks.

| Scenario | Behavior |
|---|---|
| No SD card / fopen fails | `fileLoggerInit` returns false; all log calls silently no-op |
| Buffer full (64 entries between cycles) | Message dropped, `dropCount` incremented, caller never blocked |
| SD card removed mid-match | `fwrite`/`fflush` fail silently; file may be truncated |
| Called before init / after shutdown | No-op (checks `loggerActive` flag) |
| `fileLoggerShutdown` called twice | Second call is a no-op |
| Long format string (>127 chars) | Truncated by `vsnprintf` |
| nullptr passed as fmt or label | Returns immediately (null check before use) |
| Brain clock not set | Filename uses `0101_0000`; elapsed timestamps unaffected (use `pros::micros()`) |
| Any unexpected exception | Caught by `catch(...)`, function returns silently |

No `errno` checks, no retry logic, no assertions. Deliberate for competition robotics — logging must never interfere with robot operation.

## Memory Budget

| Component | Size |
|---|---|
| Double-buffer (2 × 64 × ~130 bytes) | ~16 KB |
| Logger state (FILE*, atomics, flags, path) | ~100 bytes |
| Writer task stack (TASK_STACK_DEPTH_DEFAULT) | 32 KB |
| **Total** | **~48 KB** |

Acceptable within the V5 brain's 512KB user memory. The double-buffer costs ~8KB more than a single buffer, but eliminates all mutex contention. If memory is tight, reduce `BUFFER_SIZE` to 32 (~8KB savings).

## Files Summary

| File | Action |
|---|---|
| `include/file_logger.h` | **New** — public API declarations |
| `src/file_logger.cpp` | **New** — all implementation |
| `src/competition.cpp` | **Modify** — add init in `autonomous()`, shutdown in `disabled()` |
| `src/auton/*.cpp` | **Modify (optional)** — add `fileLogPose()` calls at waypoints |

No build system changes needed — the Makefile auto-discovers `.cpp` files in `src/`.
