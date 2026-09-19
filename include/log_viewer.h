#pragma once

/**
 * @file log_viewer.h
 * @brief Log Viewer module declarations
 *
 * Touchscreen file browser and log viewer for CSV log files on the SD card.
 * All functions are in the log_viewer namespace for clean isolation.
 */

namespace log_viewer {

/**
 * Initialize the log viewer
 * Scans /usd/logs/ for CSV files and draws the file browser
 */
void initialize();

/**
 * Disabled state handler (no-op for log viewer)
 */
void disabled();

/**
 * Competition initialize handler (no-op for log viewer)
 */
void competition_initialize();

/**
 * Autonomous handler - displays message (no auton for log viewer)
 */
void autonomous();

/**
 * Main loop - handles touch input for file browsing and log viewing
 */
void opcontrol();

} // namespace log_viewer
