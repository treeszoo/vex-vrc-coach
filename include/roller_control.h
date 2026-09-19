#pragma once

#include "robot_config_data.h"
#include <cstdint>

/**
 * @file roller_control.h
 * @brief Roller/intake motor control with anti-jam protection.
 *
 * This module provides:
 * - Low-level roller velocity control
 * - Anti-jam state machine for intake protection
 * - Driver control and autonomous roller operations
 */

// ============================================================================
// Velocity Constants
// ============================================================================

extern const int HOLD_INTAKE_VELOCITY;
extern const int INTAKE_HIGH_GOAL_VELOCITY;
extern const int OUTTAKE_HIGH_GOAL_VELOCITY;
extern const int INTAKE_MID_GOAL_VELOCITY;
extern const int OUTTAKE_MID_GOAL_VELOCITY;
extern const int INTAKE_MID_GOAL_MANUAL_VELOCITY;
extern const int OUTTAKE_MID_GOAL_MANUAL_VELOCITY;
extern const int OUTTAKE_IDLE_VELOCITY;

// ============================================================================
// Low-Level Motor Control
// ============================================================================

/**
 * Direct control of both roller motors
 * @param velocity_intake Velocity for intake motor (RPM)
 * @param velocity_outtake Velocity for outtake motor (RPM)
 */
void controll_rollers(int velocity_intake, int velocity_outtake);

/**
 * Run intake only, outtake at idle reverse
 */
void intake_only();

/**
 * Reverse both rollers
 */
void reverse_rollers();

/**
 * Hold intake at low velocity, outtake at idle reverse
 */
void hold_intake();

/**
 * Stop both roller motors
 */
void stop_rollers();

// ============================================================================
// Anti-Jam System
// ============================================================================

/**
 * Anti-jam state machine states
 */
enum class AntiJamMode {
  NORMAL,       // Normal operation, monitoring for jams
  JAM_DETECTED, // Jam detected, preparing to reverse
  REVERSING     // Currently reversing intake motor
};

/**
 * Anti-jam configuration parameters
 */
struct AntiJamConfig {
  int velocityThreshold = 15; // RPM below which motor considered jammed
  int debounceTimeMs = 280;   // Time motor must be stopped before jam detected
  int reversalDurationMs = 180; // How long to reverse motor
  int reversalVelocity = 500;   // Velocity for reversal (positive = backward)
};

/**
 * Anti-jam runtime state
 */
struct AntiJamState {
  AntiJamMode mode = AntiJamMode::NORMAL;
  uint32_t debounceStartTime = 0; // When motor first stopped (for debounce)
  uint32_t reversalStartTime = 0; // When reversal began (for reversal timing)
  uint32_t lastJamTime = 0;       // When last jam was detected
  int commandedIntakeVelocity =
      0;                // What intake velocity we're trying to achieve
  AntiJamConfig config; // Configuration parameters
};

/**
 * Check if intake motor is jammed
 *
 * @param state Anti-jam state structure
 * @param commandedVelocity Velocity we commanded the motor to move at
 * @return true if jam detected, false otherwise
 */
bool detectJam(AntiJamState &state, int commandedVelocity);

/**
 * Update anti-jam state machine and get adjusted intake motor velocity
 *
 * @param state Anti-jam state structure
 * @param commandedIntakeVelocity Velocity we want the intake motor at
 * @return Actual velocity to command to intake motor (may be reversed if jam
 * detected)
 */
int updateAntiJamStateMachine(AntiJamState &state, int commandedIntakeVelocity);

/**
 * Reset anti-jam state machine to initial state
 * Call when starting new operation or switching operations
 */
void resetAntiJamState(AntiJamState &state);

// ============================================================================
// Driver Control Operations
// ============================================================================

/**
 * Roller operation types for driver control
 */
enum class RollerOperation { NONE, HIGH_GOAL, MID_GOAL, INTAKE_ONLY, REVERSE };

/**
 * Execute roller operation with anti-jam support for scoring operations
 *
 * @param operation Type of roller operation to execute
 * @param antiJamState Anti-jam state machine (modified if anti-jam active)
 */
void executeRollerOperation(AutonomousMode autonMode, RollerOperation operation,
                            AntiJamState &antiJamState);

// ============================================================================
// Autonomous Operations
// ============================================================================

/**
 * Control rollers for a specific duration in autonomous mode
 * Includes anti-jam support with blocking implementation
 *
 * @param velocity_intake Desired intake velocity
 * @param velocity_outtake Desired outtake velocity
 * @param duration_ms How long to run (milliseconds)
 * @param enable_anti_jam Enable anti-jam system (default: true)
 */
void controll_rollers_auton(int velocity_intake, int velocity_outtake,
                            int duration_ms, bool enable_anti_jam = true);
