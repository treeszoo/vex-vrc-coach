#pragma once

/**
 * @file config_ui.h
 * @brief Configuration UI module declarations
 *
 * Touchscreen interface for configuring robot settings before a match.
 * All functions are in the config_ui namespace for clean isolation.
 */

namespace config_ui {

/**
 * Initialize the configuration UI
 * Loads existing config and draws the initial UI
 */
void initialize();

/**
 * Disabled state handler (no-op for config UI)
 */
void disabled();

/**
 * Competition initialize handler (no-op for config UI)
 */
void competition_initialize();

/**
 * Autonomous handler - displays message (no auton for config UI)
 */
void autonomous();

/**
 * Main loop - handles touch input for configuration
 */
void opcontrol();

} // namespace config_ui
