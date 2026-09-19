#include "debug_output.h"
#include "api.h"
#include "color_filter.h"
#include "movement_utils.h"
#include "robot_config.h"
#include <cstdarg>

using namespace pros;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Convert AntiJamMode enum to string for display
 */
const char *antiJamModeToString(AntiJamMode mode) {
  switch (mode) {
  case AntiJamMode::NORMAL:
    return "NORMAL";
  case AntiJamMode::JAM_DETECTED:
    return "JAM_DETECT";
  case AntiJamMode::REVERSING:
    return "REVERSING";
  default:
    return "UNKNOWN";
  }
}

/**
 * Convert RollerOperation enum to string for display
 */
const char *rollerOperationToString(RollerOperation op) {
  switch (op) {
  case RollerOperation::NONE:
    return "NONE";
  case RollerOperation::HIGH_GOAL:
    return "HIGH_GOAL";
  case RollerOperation::MID_GOAL:
    return "MID_GOAL";
  case RollerOperation::INTAKE_ONLY:
    return "INTAKE";
  case RollerOperation::REVERSE:
    return "REVERSE";
  default:
    return "UNKNOWN";
  }
}

// ============================================================================
// Controller Screen Debug Functions (3 lines, 18 chars each, compact)
// ============================================================================

/**
 * Helper function to print a formatted line to controller with padding
 * Handles variable-width font by padding to 35 characters
 */
static void printControllerLine(int row, const char *format, ...) {
  char line[40];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  controller.print(row, 0, "%-35s", line);
  delay(50);
}

/**
 * Print position debug information to controller screen
 */
void printPositionDebugController() {
  printControllerLine(0, "XY:%.1f,%.1f", chassis.getPose().x,
                      chassis.getPose().y);
  printControllerLine(1, "%.0f,%.1f", chassis.getPose().theta,
                      imu.get_rotation());
}

/**
 * Print optical sensor debug information to controller screen
 */
void printOpticalDebugController() {
  int proximity = optical.get_proximity();
  double hue = optical.get_hue();
  printControllerLine(0, "Prx:%d Hue:%.0f", proximity, hue);

  bool red = is_red();
  bool blue = is_blue();
  const char *colorStatus = red ? "RED" : (blue ? "BLUE" : "NONE");
  printControllerLine(1, "Color:%s", colorStatus);

  printControllerLine(2, "XY:%.1f,%.1f", chassis.getPose().x,
                      chassis.getPose().y);
}

/**
 * Print intake motor and anti-jam debug information to controller screen
 */
void printIntakeMotorDebugController(const AntiJamState &state,
                                     RollerOperation currentOp) {
  // Line 0: Actual vs Commanded velocity
  double actualVel = intakeMotor.get_actual_velocity();
  printControllerLine(0, "A:%.0f C:%d", actualVel,
                      state.commandedIntakeVelocity);

  // Line 1: State and threshold
  printControllerLine(1, "%s T:%d", antiJamModeToString(state.mode),
                      state.config.velocityThreshold);

  // Line 2: Timing (debounce or reversal)
  uint32_t now = millis();
  if (state.mode == AntiJamMode::REVERSING) {
    uint32_t reversalElapsed = 0;
    if (state.reversalStartTime > 0) {
      reversalElapsed = now - state.reversalStartTime;
    }
    printControllerLine(2, "Rv:%d/%dms", reversalElapsed,
                        state.config.reversalDurationMs);
  } else {
    uint32_t debounceTime = 0;
    if (state.debounceStartTime > 0 && state.mode == AntiJamMode::NORMAL) {
      debounceTime = now - state.debounceStartTime;
    }
    printControllerLine(2, "Db:%d/%dms", debounceTime,
                        state.config.debounceTimeMs);
  }
}

/**
 * Print configuration information to controller screen
 */
void printConfigDebugController(AutonomousMode autonMode,
                                const AutonConfig &config) {
  // Line 0: Autonomous mode + aggressive flag
  printControllerLine(0, "%s%s", getAutonName(autonMode),
                      config.aggressive ? " [AGGR]" : "");

  // Line 1: Team color + color sensor
  printControllerLine(1, "%s Snsr:%s", getColorName(config.teamColor),
                      config.useColorSensor ? "ON" : "OFF");

  // Line 2: Push alliance
  printControllerLine(2, "Push:%s", config.pushAlliance ? "ON" : "OFF");
}

/**
 * Print distance sensor readings to controller screen (mm)
 */
void printDistanceDebugController() {
  int front = distance_front.get();
  if (front == PROS_ERR)
    front = -1;
  int sz = distance_front.get_object_size();
  if (sz == PROS_ERR)
    sz = -1;
  printControllerLine(0, "Front: %d mm", front);
  printControllerLine(1, "sz:%d", sz);
  printControllerLine(2, "(9999 = no object)");
}

// ============================================================================
// Brain LCD Debug Functions (5 lines, detailed)
// ============================================================================

/**
 * Print position debug information to brain LCD
 */
void printPositionDebugLCD() {
  lcd::print(1, "Position: X=%.1f, Y=%.1f", chassis.getPose().x,
             chassis.getPose().y);
  lcd::print(2, "Heading: %.1f deg", chassis.getPose().theta);
  lcd::print(3, "IMU: %.1f deg", imu.get_rotation());
  lcd::print(5, "");
}

/**
 * Print optical sensor debug information to brain LCD
 */
void printOpticalDebugLCD() {
  // Line 1: Optical sensor proximity
  int proximity = optical.get_proximity();
  lcd::print(1, "Proximity: %d", proximity);

  // Line 2: Optical sensor hue
  double hue = optical.get_hue();
  lcd::print(2, "Hue: %.1f", hue);

  // Line 3: Color detection
  bool red = is_red();
  bool blue = is_blue();
  const char *colorStatus = red ? "RED" : (blue ? "BLUE" : "NONE");
  lcd::print(3, "Detected: %s", colorStatus);

  // Line 4: Blank separator
  lcd::print(4, "");

  // Line 5: Robot position
  lcd::print(5, "X: %.1f  Y: %.1f", chassis.getPose().x, chassis.getPose().y);
}

/**
 * Print intake motor and anti-jam debug information to brain LCD
 */
void printIntakeMotorDebugLCD(const AntiJamState &state,
                              RollerOperation currentOp) {
  uint32_t now = millis();

  // Line 1: Actual vs Commanded velocity
  double actualVel = intakeMotor.get_actual_velocity();
  lcd::print(1, "Intake: A=%.0f C=%d", actualVel,
             state.commandedIntakeVelocity);

  // Line 2: Anti-jam state and threshold
  lcd::print(2, "State: %s (T=%d)", antiJamModeToString(state.mode),
             state.config.velocityThreshold);

  // Line 3: Timing information based on current state
  if (state.mode == AntiJamMode::REVERSING) {
    // Show reversal progress
    uint32_t reversalElapsed = 0;
    if (state.reversalStartTime > 0) {
      reversalElapsed = now - state.reversalStartTime;
    }
    lcd::print(3, "Reverse: %dms/%dms", reversalElapsed,
               state.config.reversalDurationMs);
  } else {
    // Show debounce progress
    uint32_t debounceTime = 0;
    if (state.debounceStartTime > 0 && state.mode == AntiJamMode::NORMAL) {
      debounceTime = now - state.debounceStartTime;
    }
    lcd::print(3, "Debounce: %dms/%dms", debounceTime,
               state.config.debounceTimeMs);
  }

  // Line 4: Current roller operation
  lcd::print(4, "Op: %s", rollerOperationToString(currentOp));

  // Line 5: Timestamps for debugging timing issues
  lcd::print(5, "Now:%d Db:%d Rv:%d", now, state.debounceStartTime,
             state.reversalStartTime);
}

/**
 * Print distance sensor readings to brain LCD (mm)
 */
void printDistanceDebugLCD() {
  int front = distance_front.get();
  if (front == PROS_ERR)
    front = -1;
  int sz = distance_front.get_object_size();
  if (sz == PROS_ERR)
    sz = -1;
  lcd::print(1, "Front: %d mm  sz:%d", front, sz);
  lcd::print(2, "(9999 = no object)");
  lcd::print(3, "");
  lcd::print(4, "");
  lcd::print(5, "");
}

/**
 * Print configuration information to brain LCD (auton, team, sensor)
 */
void printConfigDebugLCD(AutonomousMode autonMode, const AutonConfig &config) {
  lcd::print(0, "Auton: %s%s", getAutonName(autonMode),
             config.aggressive ? " [AGGR]" : "");
  lcd::print(1, "Team: %s  Sensor: %s", getColorName(config.teamColor),
             config.useColorSensor ? "ON" : "OFF");
  lcd::print(2, "Push Alliance: %s", config.pushAlliance ? "ON" : "OFF");
}

// ============================================================================
// Screen Control Tasks
// ============================================================================

/**
 * Controller screen control task
 */
void controllerScreenControl(const DebugMode *currentMode,
                             const AntiJamState *antiJamState,
                             const RollerOperation *currentRollerOp,
                             const AutonomousMode *autonMode,
                             const AutonConfig *config) {
  DebugMode previousMode = DebugMode::POSITION;

  while (true) {
    // Dereference pointers to get current values in each iteration
    DebugMode mode =
        (currentMode != nullptr) ? *currentMode : DebugMode::DISABLED;
    RollerOperation rollerOp =
        (currentRollerOp != nullptr) ? *currentRollerOp : RollerOperation::NONE;

    // Display debug info based on current mode (same as LCD)
    switch (mode) {
    case DebugMode::DISABLED:
      // Show configuration info (team color, auto mode, etc.)
      if (autonMode != nullptr && config != nullptr) {
        printConfigDebugController(*autonMode, *config);
      } else {
        printControllerLine(0, "Config");
        printControllerLine(1, "loading...");
      }
      break;

    case DebugMode::POSITION:
      printPositionDebugController();
      break;

    case DebugMode::OPTICAL_SENSOR:
      printOpticalDebugController();
      break;

    case DebugMode::INTAKE_MOTOR:
      if (antiJamState != nullptr) {
        printIntakeMotorDebugController(*antiJamState, rollerOp);
      } else {
        printControllerLine(0, "Intake debug");
        printControllerLine(1, "waiting...");
      }
      break;

    case DebugMode::DISTANCE_SENSOR:
      printDistanceDebugController();
      break;
    }

    delay(150); // Update at ~6Hz (total cycle time with print delays)
  }
}

/**
 * Brain LCD screen control task
 */
void lcdScreenControl(const DebugMode *currentMode,
                      const AntiJamState *antiJamState,
                      const RollerOperation *currentRollerOp,
                      const AutonomousMode *autonMode,
                      const AutonConfig *config) {
  while (true) {
    // Front distance sensor reading for status line / distance mode
    int frontMm = distance_front.get();
    if (frontMm == PROS_ERR)
      frontMm = -1;

    // Dereference pointers to get current values in each iteration
    DebugMode mode =
        (currentMode != nullptr) ? *currentMode : DebugMode::DISABLED;
    RollerOperation rollerOp =
        (currentRollerOp != nullptr) ? *currentRollerOp : RollerOperation::NONE;

    // Display debug info based on selected mode (same as controller)
    switch (mode) {
    case DebugMode::DISABLED:
      // Restore config display (auton, team, sensor) and add distance reading
      if (autonMode != nullptr && config != nullptr) {
        printConfigDebugLCD(*autonMode, *config);
      }
      lcd::print(3, "Front: %d mm  sz:%d", frontMm,
                 distance_front.get_object_size());
      lcd::print(4, "");
      lcd::print(5, "");
      break;

    case DebugMode::POSITION:
      lcd::print(0, "Front dist: %d mm", frontMm);
      printPositionDebugLCD();
      break;

    case DebugMode::OPTICAL_SENSOR:
      lcd::print(0, "Front dist: %d mm", frontMm);
      printOpticalDebugLCD();
      break;

    case DebugMode::INTAKE_MOTOR:
      lcd::print(0, "Front dist: %d mm", frontMm);
      if (antiJamState != nullptr) {
        printIntakeMotorDebugLCD(*antiJamState, rollerOp);
      } else {
        lcd::print(1, "Intake debug");
        lcd::print(2, "waiting...");
      }
      break;

    case DebugMode::DISTANCE_SENSOR:
      lcd::print(0, "Front dist: %d mm", frontMm);
      printDistanceDebugLCD();
      break;
    }

    delay(50); // Update at 20Hz
  }
}
