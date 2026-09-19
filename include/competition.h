#pragma once

/**
 * @file competition.h
 * @brief Competition mode module declarations
 *
 * Normal robot operation: autonomous routines and operator control.
 * All functions are in the competition namespace for clean isolation.
 */

namespace competition {

/**
 * Initialize the robot for competition
 * Sets up LCD, sensors, and actuators
 */
void initialize();

/**
 * Disabled state handler
 * Retracts actuators for safety
 */
void disabled();

/**
 * Competition-specific initialization
 * Calibrates chassis and resets pose
 */
void competition_initialize();

/**
 * Run the selected autonomous routine
 */
void autonomous();

/**
 * Operator control loop
 * Handles driver input for driving and mechanisms
 */
void opcontrol();

} // namespace competition
