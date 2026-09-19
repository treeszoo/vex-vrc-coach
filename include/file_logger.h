#pragma once

/**
 * @file file_logger.h
 * @brief Asynchronous, thread-safe file logger for SD card logging
 *
 * Non-blocking CSV logger that writes to /usd/logs/ during autonomous and
 * driver control. Uses a lock-free double-buffer with a background writer task.
 * All public functions are crash-safe — failures are silent no-ops.
 */

#include "robot_config_data.h"

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

/**
 * Log the current robot pose (x, y, heading) with a label and timestamp.
 * Output format: <MM:SS.uuuuuu>,<label>,<x>,<y>,<heading>
 *
 * @param label  Short descriptive label (e.g., "SCORE", "LOADER1", "TURN")
 */
void fileLogPose(const char *label);
