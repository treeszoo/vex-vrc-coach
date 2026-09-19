/**
 * @file main.cpp
 * @brief PROS entry point dispatcher
 *
 * This file routes PROS callbacks to either the Competition module or
 * the Config UI module based on the compile-time APP_MODE setting.
 *
 * Using `if constexpr` ensures the unused module's code is completely
 * eliminated by the compiler - zero runtime overhead.
 */

#include "main.h"
#include "competition.h"
#include "config_ui.h"
#include "log_viewer.h"
#include "main.h"
#include "robot_config.h"
#include "subsystem/rope.h"
#include "subsystem/toggle.h"
#include "auton/v5_auton.h"

// >> V5 part
static constexpr float DTkp = 0.85f;
static bool claw_con = false;
static bool toggle_con = false;
static bool touch_con = false;

/**
 * Compile-time application mode selection.
 */
enum class AppMode {
  COMPETITION, // Normal competition mode (autonomous + opcontrol)
  CONFIG_UI,   // Touchscreen configuration UI
  LOG_VIEWER   // Post-match log file viewerx
};

/**
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  CHANGE THIS VALUE TO SWITCH MODES BEFORE BUILDING             │
 * │                                                                 │
 * │  AppMode::COMPETITION  - For matches (upload to slot 4)        │
 * │  AppMode::CONFIG_UI    - For pre-match setup (upload to slot 1)│
 * │  AppMode::LOG_VIEWER   - For post-match logs (upload to slot 2)│
 * └─────────────────────────────────────────────────────────────────┘
 */
constexpr AppMode APP_MODE = AppMode::COMPETITION;

/**
 * Runs initialization code. This occurs as soon as the program is started.
 */
void initialize() {
  if constexpr (APP_MODE == AppMode::CONFIG_UI) {
    config_ui::initialize();
  } else if constexpr (APP_MODE == AppMode::LOG_VIEWER) {
    log_viewer::initialize();
  } else {
    competition::initialize();


    // >> V5 part
    Rope.set_brake_mode(pros::MotorBrake::hold);
    Lefttoggle.set_brake_mode(pros::MotorBrake::brake);
    Righttoggle.set_brake_mode(pros::MotorBrake::brake);

    pros::delay(200);
    Rope.tare_position();
    Lefttoggle.tare_position();
    Righttoggle.tare_position();

    pros::c::delay(100);
    chassis.calibrate();
    verticalEnc.set_position(0);
    horizontalEnc.set_position(0);

    pros::delay(100);
    chassis.setPose(0, 0, 0);

    pros::c::screen_erase();
    pros::c::screen_set_pen(pros::c::COLOR_WHITE);
    pros::delay(300);
    claw.set_value(true);
    toggle.set_value(false);
    touchtoggle.set_value(false);

    pros::Task screenTask{[]() {
        while (true) {
            pros::c::screen_print(TEXT_LARGE, 1, "Battery: %.f percent", pros::battery::get_capacity());
            pros::c::screen_print(TEXT_LARGE, 3, "Degree: %.f", Rope.get_position());
            pros::delay(50);
        }
    }};
  }
}

/**
 * Runs while the robot is in the disabled state of Field Management System or
 * the VEX Competition Switch, following either autonomous or opcontrol.
 */
void disabled() {
  if constexpr (APP_MODE == AppMode::CONFIG_UI) {
    config_ui::disabled();
  } else if constexpr (APP_MODE == AppMode::LOG_VIEWER) {
    log_viewer::disabled();
  } else {
    competition::disabled();

    // >> V5 part
    stopRopeTask();
  }
}

/**
 * Runs after initialize(), and before autonomous when connected to the Field
 * Management System or the VEX Competition Switch.
 */
void competition_initialize() {
  if constexpr (APP_MODE == AppMode::CONFIG_UI) {
    config_ui::competition_initialize();
  } else if constexpr (APP_MODE == AppMode::LOG_VIEWER) {
    log_viewer::competition_initialize();
  } else {
    competition::competition_initialize();
  }
}

/**
 * Runs the user autonomous code.
 */
void autonomous() {
  if constexpr (APP_MODE == AppMode::CONFIG_UI) {
    config_ui::autonomous();
  } else if constexpr (APP_MODE == AppMode::LOG_VIEWER) {
    log_viewer::autonomous();
  } else {
    competition::autonomous();


    // >> V5 part
    chassis.setBrakeMode(pros::E_MOTOR_BRAKE_BRAKE);
    near_2();
  }
}

/**
 * Runs the operator control code.
 */
void opcontrol() {
  if constexpr (APP_MODE == AppMode::CONFIG_UI) {
    config_ui::opcontrol();
  } else if constexpr (APP_MODE == AppMode::LOG_VIEWER) {
    log_viewer::opcontrol();
  } else {
    competition::opcontrol();
  }
}
