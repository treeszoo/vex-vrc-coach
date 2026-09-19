/**
 * @file competition.cpp
 * @brief Competition mode module implementation
 *
 * Normal robot operation: autonomous routines and operator control.
 * Wrapped in competition namespace for clean isolation from config UI.
 */

#include "competition.h"
#include "auton/left.h"
#include "auton/right.h"
#include "auton/skill.h"
#include "auton/solo.h"

#include "button_helpers.h"
#include "file_logger.h"
#include "button_tracker.hpp"
#include "debug_output.h"
#include "pros/llemu.hpp"
#include "pros/misc.h"
#include "pros/misc.hpp"
#include "pros/motors.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"
#include "roller_control.h"
#include "subsystem/rope.h"
#include "subsystem/toggle.h"
#include <cstdio>
#include <cstring>

namespace competition {

using namespace std;
using namespace pros;

// ============================================================================
// Configuration State (loaded from SD card)
// ============================================================================

static AutonomousMode selectedAutonMode = ConfigDefaults::AUTON_MODE;
static AutonConfig autonConfig = {
    .teamColor = ConfigDefaults::TEAM_COLOR,
    .useColorSensor = ConfigDefaults::USE_COLOR_SENSOR,
};


// >> V5 part
static constexpr float DTkp = 0.85f;
static bool claw_con = false;
static bool toggle_con = false;
static bool touch_con = false;




// ============================================================================
// Debug System
// ============================================================================

// Select debug mode (can be changed at runtime with UP+A button combo)
// Controls both controller screen and brain LCD
// DISABLED mode shows configuration info (team color, auto mode, etc.)
static DebugMode CURRENT_DEBUG_MODE = DebugMode::DISABLED;

// Shared state for debug display (updated by opcontrol, read by
// lcdScreenControl)
static AntiJamState *debugAntiJamState = nullptr;
static RollerOperation debugCurrentRollerOp = RollerOperation::NONE;

// Screen control tasks (must be declared static to persist after initialize())
static pros::Task *controllerScreenTask = nullptr;
static pros::Task *brainScreenTask = nullptr;

// ============================================================================
// Configuration Loading
// ============================================================================

/**
 * Load robot configuration from SD card
 * Uses safe defaults if file is missing or invalid
 */
static bool loadRobotConfig() {
  // Try to open config file - if SD card isn't present or file doesn't exist,
  // fopen will fail and we'll use defaults
  FILE *file = fopen(CONFIG_FILE_PATH, "r");
  if (!file) {
    return false;
  }

  int auton = -1, color = -1, colorSensor = -1, aggr = -1, push = -1;
  char line[64];

  while (fgets(line, sizeof(line), file)) {
    sscanf(line, "auton=%d", &auton);
    sscanf(line, "color=%d", &color);
    sscanf(line, "use_color_sensor=%d", &colorSensor);
    sscanf(line, "aggressive=%d", &aggr);
    sscanf(line, "push_alliance=%d", &push);
  }
  fclose(file);

  if (isValidAutonMode(auton)) {
    selectedAutonMode = static_cast<AutonomousMode>(auton);
  }
  if (isValidTeamColor(color)) {
    autonConfig.teamColor = static_cast<TeamColor>(color);
  }
  if (colorSensor == 0 || colorSensor == 1) {
    autonConfig.useColorSensor = (colorSensor == 1 && optical.is_installed());
  }
  if (aggr == 0 || aggr == 1) {
    autonConfig.aggressive = (aggr == 1);
  }
  if (push == 0 || push == 1) {
    autonConfig.pushAlliance = (push == 1);
  }

  return true;
}

/**
 * Load robot parameters configuration from SD card
 * Uses safe defaults if file is missing or invalid
 */
static bool loadRobotParameterConfig() {
  // Try to open config file - if SD card isn't present or file doesn't exist,
  // fopen will fail and we'll use defaults
  FILE *file = fopen(CONFIG_PARAMETER_FILE_PATH, "r");
  if (!file) {
    return false;
  }

  // add in code to load parameters below

  return true;
}

// ============================================================================
// Public Entry Points (called by main.cpp dispatcher)
// ============================================================================

void initialize() {
  pros::lcd::initialize();

  // Load configuration from SD card
  loadRobotConfig(); 

  //Load pre-configured parameters from SD card
   loadRobotParameterConfig();

  // Display loaded config on LCD (condensed to one line)
  pros::lcd::print(0, "%s%s | %s | Snsr:%s Push:%s",
                   getAutonName(selectedAutonMode),
                   autonConfig.aggressive ? "(A)" : "",
                   getColorName(autonConfig.teamColor),
                   autonConfig.useColorSensor ? "ON" : "OFF",
                   autonConfig.pushAlliance ? "ON" : "OFF");

  // Start screen control tasks (must be stored as static to persist)
  controllerScreenTask = new pros::Task([]() {
    controllerScreenControl(&CURRENT_DEBUG_MODE, debugAntiJamState,
                            &debugCurrentRollerOp, &selectedAutonMode,
                            &autonConfig);
  });
  brainScreenTask = new pros::Task([]() {
    lcdScreenControl(&CURRENT_DEBUG_MODE, debugAntiJamState,
                     &debugCurrentRollerOp, &selectedAutonMode, &autonConfig);
  });
}

void disabled() { 
  fileLogPose("Robot disabled.");
  fileLoggerShutdown(); 
}

void competition_initialize() {
  chassis.calibrate();      // calibrate sensors
  chassis.setPose(0, 0, 0); // set pose
  controller.clear();
  descorerActuator.retract();
}

void autonomous() {
  // Safety: deploy descorer at start of auton
  //change here to change the autons
  descorerActuator.deploy();
  wheelupActuator.retract();
  fileLoggerInit(selectedAutonMode, &autonConfig);
  fileLogPose("Auton started");

  switch (selectedAutonMode) {
  case AutonomousMode::ALLIANCE_LEFT:
    auton_alliance_left(autonConfig, autonConfig.aggressive, true);
    break;
  case AutonomousMode::ALLIANCE_RIGHT:
    auton_alliance_right(autonConfig, true);
    break;
  case AutonomousMode::ALLIANCE_SOLO:
    auton_solo_right(autonConfig,autonConfig.pushAlliance);
    break;
  case AutonomousMode::SKILLS:
    if (autonConfig.aggressive) {
      skill_aggr(autonConfig);
    } else {
      skill_safe(autonConfig);
    }
    break;
  default:
    auton_alliance_left(autonConfig, false, true);
    break;
  }
}

// Check if any controller input is active (buttons or joysticks)
static bool hasAnyControllerInput() {
  constexpr int DEADZONE = 10;

  // Check all analog sticks
  if (abs(controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y)) > DEADZONE ||
      abs(controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_X)) > DEADZONE ||
      abs(controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_Y)) >
          DEADZONE ||
      abs(controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X)) >
          DEADZONE) {
    return true;
  }

  // Check all digital buttons
  if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_A) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_B) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_X) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_Y) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_UP) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_LEFT) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_RIGHT) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_R1) ||
      controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)) {
    return true;
  }

  return false;
}

void opcontrol() {
  chassis.setBrakeMode(E_MOTOR_BRAKE_COAST);
  if (autonConfig.useColorSensor) {
    optical.set_led_pwm(0);
  }

  ButtonTracker buttonA, buttonLeft, buttonWheelup, buttonDebugMode;
  bool isWheelupDeployed = false;

  // Anti-jam state machine for driver control
  AntiJamState antiJamState;

  // Track roller operations to detect changes
  RollerOperation currentRollerOp = RollerOperation::NONE;
  RollerOperation previousRollerOp = RollerOperation::NONE;

  // Make state accessible to debug task
  debugAntiJamState = &antiJamState;
  debugCurrentRollerOp = currentRollerOp;

  while (true) {

    // Deploy wheelup only after user provides any controller input
    if (!isWheelupDeployed && hasAnyControllerInput()) {
      wheelupActuator.deploy();
      isWheelupDeployed = true;
    }

    // Drive control
    int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
    int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
    chassis.arcade(leftY, rightX * 0.85);

    // Determine current roller operation based on button state
    // L1 => outtake high goal (with intake and anti-jam)
    // L2 => outtake mid goal (with intake and anti-jam)
    // R1 => intake only without outtake (no anti-jam)
    // R2 => reverse both intake and outtake rollers (no anti-jam)
    if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1)) {
      currentRollerOp = RollerOperation::HIGH_GOAL;
    } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)) {
      currentRollerOp = RollerOperation::MID_GOAL;
    } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R1)) {
      currentRollerOp = RollerOperation::INTAKE_ONLY;
    } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)) {
      currentRollerOp = RollerOperation::REVERSE;
    } else {
      currentRollerOp = RollerOperation::NONE;
    }

    // Detect operation changes and reset anti-jam state
    if (currentRollerOp != previousRollerOp) {
      resetAntiJamState(antiJamState);
      previousRollerOp = currentRollerOp;
    }

    // Update debug state
    debugCurrentRollerOp = currentRollerOp;

    // Execute roller operation with anti-jam
    executeRollerOperation(selectedAutonMode, currentRollerOp, antiJamState);

    // Pneumatic control
    // Button B toggles loader
    handleButtonToggle(E_CONTROLLER_DIGITAL_B, buttonA,
                       []() { loaderActuator.toggle(); });

    // Button Down toggles descorer
    handleButtonToggle(E_CONTROLLER_DIGITAL_DOWN, buttonLeft,
                       []() { descorerActuator.toggle(); });

    // X & Up toggle wheelup and recalibrate
    handleMultiButtonToggle(
        {pros::E_CONTROLLER_DIGITAL_X, pros::E_CONTROLLER_DIGITAL_UP},
        buttonWheelup, []() {
          wheelupActuator.toggle();
          chassis.calibrate();
          chassis.setPose(0, 0, 0);
        });

    // Up & A toggle debug mode (cycle through modes on both screens)
    handleMultiButtonToggle(
        {pros::E_CONTROLLER_DIGITAL_UP, pros::E_CONTROLLER_DIGITAL_A},
        buttonDebugMode, []() {
          // Cycle through debug modes using modulo arithmetic
          int currentMode = static_cast<int>(CURRENT_DEBUG_MODE);
          int nextMode = (currentMode + 1) % 5; // 5 debug modes total
          CURRENT_DEBUG_MODE = static_cast<DebugMode>(nextMode);

          // Rumble feedback (1-5 pulses based on mode)
          const char *rumblePatterns[] = {".", "..", "...", "....", "....."};
          // controller.rumble(rumblePatterns[nextMode]);
        });

    delay(10);
  }

// >> V5 part
stopRopeTask();
    chassis.setBrakeMode(pros::E_MOTOR_BRAKE_COAST);

    claw_con = false;
    toggle_con = true;
    toggle.set_value(true);
    claw.set_value(true);

    uint32_t start_time = pros::millis();
    pros::Task toggleTask(toggle_task_fn, nullptr, "Toggle Task");

    while (true) {
        // 1. Drivetrain Control
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
        chassis.arcade(leftY, rightX * DTkp);

        // 2. Pneumatics / Toggles
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_RIGHT)) {
            toggle_con = !toggle_con;
            toggle.set_value(toggle_con);
        }

        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_B)) {
            claw_con = !claw_con;
            claw.set_value(claw_con);
        }

        if (pros::millis() - start_time >= 90000) {
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)) {
                touch_con = !touch_con;
                touchtoggle.set_value(touch_con);
            }
        }

        // 3. Arm / Rope Movement
        bool up = controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_UP);
        bool l1 = controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1);
        bool l2 = controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2);
        update_rope_opcontrol(up, l1, l2);

        pros::delay(10);
    }

}

} // namespace competition
