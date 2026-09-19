#pragma once

#include "robot_config_data.h"
#include "roller_control.h"

/**
 * @file debug_output.h
 * @brief Debug output system for LCD display
 *
 * Provides modular debug output functions that can be switched
 * between different modes during driver control.
 */

// ============================================================================
// Debug Mode Selection
// ============================================================================

/**
 * Debug mode selection for both controller and LCD output
 */
enum class DebugMode {
  DISABLED,        // Show configuration info (team color, auto mode, etc.)
  OPTICAL_SENSOR,  // Display optical sensor information (color picking)
  POSITION,        // Robot position and orientation
  INTAKE_MOTOR,    // Display intake motor and anti-jam state
  DISTANCE_SENSOR  // Front distance sensor readings (mm)
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Convert AntiJamMode enum to string for display
 */
const char *antiJamModeToString(AntiJamMode mode);

/**
 * Convert RollerOperation enum to string for display
 */
const char *rollerOperationToString(RollerOperation op);

// ============================================================================
// Controller Screen Debug Functions (3 lines, compact format)
// ============================================================================

/**
 * Print position debug information to controller screen
 */
void printPositionDebugController();

/**
 * Print optical sensor debug information to controller screen
 */
void printOpticalDebugController();

/**
 * Print intake motor and anti-jam debug information to controller screen
 *
 * @param state Anti-jam state structure
 * @param currentOp Current roller operation
 */
void printIntakeMotorDebugController(const AntiJamState &state,
                                     RollerOperation currentOp);

/**
 * Print front distance sensor readings to controller screen
 */
void printDistanceDebugController();

/**
 * Print configuration information to controller screen (team color, auto mode,
 * etc.)
 *
 * @param autonMode Selected autonomous mode
 * @param config Autonomous configuration (team color, sensor usage)
 */
void printConfigDebugController(AutonomousMode autonMode,
                                const AutonConfig &config);

// ============================================================================
// Brain LCD Debug Functions (5 lines, detailed format)
// ============================================================================

/**
 * Print position debug information to brain LCD
 */
void printPositionDebugLCD();

/**
 * Print optical sensor debug information to brain LCD
 */
void printOpticalDebugLCD();

/**
 * Print intake motor and anti-jam debug information to brain LCD
 *
 * @param state Anti-jam state structure
 * @param currentOp Current roller operation
 */
void printIntakeMotorDebugLCD(const AntiJamState &state,
                              RollerOperation currentOp);

/**
 * Print front distance sensor readings to brain LCD
 */
void printDistanceDebugLCD();

/**
 * Print configuration information to brain LCD (auton, team color, sensor)
 */
void printConfigDebugLCD(AutonomousMode autonMode, const AutonConfig &config);

// ============================================================================
// Screen Control Tasks (called by competition.cpp)
// ============================================================================

/**
 * Controller screen control task
 * Displays debug info based on current mode
 *
 * @param currentMode Pointer to current debug mode
 * @param antiJamState Pointer to anti-jam state (can be nullptr)
 * @param currentRollerOp Pointer to current roller operation
 * @param autonMode Pointer to selected autonomous mode
 * @param config Pointer to autonomous configuration
 */
void controllerScreenControl(const DebugMode *currentMode,
                             const AntiJamState *antiJamState,
                             const RollerOperation *currentRollerOp,
                             const AutonomousMode *autonMode,
                             const AutonConfig *config);

/**
 * Brain LCD screen control task
 * Displays debug info based on current mode
 *
 * @param currentMode Pointer to current debug mode
 * @param antiJamState Pointer to anti-jam state (can be nullptr)
 * @param currentRollerOp Pointer to current roller operation
 * @param autonMode Pointer to selected autonomous mode
 * @param config Pointer to autonomous configuration
 */
void lcdScreenControl(const DebugMode *currentMode,
                      const AntiJamState *antiJamState,
                      const RollerOperation *currentRollerOp,
                      const AutonomousMode *autonMode,
                      const AutonConfig *config);
