#include "auton/common.h"
#include "color_filter.h"
#include "file_logger.h"
#include "movement_utils.h"
#include "pros/motors.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "roller_control.h"

#include <algorithm>

using namespace pros;

// ============================================================================
// Initialization
// ============================================================================

void init_auton(const AutonConfig &config) {
  if (config.useColorSensor) {
    optical.set_led_pwm(100);
  }
  chassis.setBrakeMode(E_MOTOR_BRAKE_BRAKE);
  // retract the wheelup, so that the tracking wheel is deployed
  wheelupActuator.retract();
}

// ============================================================================
// Goal Alignment
// ============================================================================

void alignToLongGoal_fast() {
  // Use current heading to determine turn direction
  // In skills, theta <= 270 means right turn
  bool rightTurn = chassis.getPose().theta <= 270;
  // only make a right turn
  chassis.arcade(0, rightTurn ? 100 : -100);
  delay(100);
  int speeds[] = {20, 40, 80, 100, 127};
  int delays[] = {150, 150, 100, 100, 500};
  for (int i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
    chassis.arcade(-speeds[i], 0);
    delay(delays[i]);
  }
  chassis.arcade(0, 0);
  delay(50);
}

void alignToLongGoal_slow() {
  chassis.arcade(0, 80);
  delay(200);
  int speeds[] = {20, 40, 80, 100, 127};
  int delays[] = {400, 200, 100, 100, 500};
  for (int i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
    chassis.arcade(-speeds[i], 0);
    delay(delays[i]);
  }
  chassis.arcade(0, 0);
  delay(20);

  chassis.arcade(0, -80);
  delay(200);
  for (int i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
    chassis.arcade(-speeds[i], 0);
    delay(delays[i]);
  }
  chassis.arcade(0, 0);
  delay(50);
}

// ============================================================================
// Descoring
// ============================================================================

void performDescore(bool quickMode) {
  descorerActuator.retract();
  chassis.setPose(0, 0, -1);
  chassis.moveToPoint(-10, 8, 800, {.maxSpeed = 80});
  chassis.turnToHeading(0, 500);

  chassis.moveToPose(-10.5, -19, -2, quickMode ? 1100 : 3000,
                     {.forwards = false,
                      .lead = 0,
                      .maxSpeed = quickMode ? 60.0f : 70.0f,
                      .earlyExitRange = 0.5});
}

// ============================================================================
// Block Pickup
// ============================================================================

void pick_blocks_from_loader(float x, float y, float theta, float speed,
                             int moveTimeout, int pickupTime,
                             const AutonConfig *config) {
  (void)theta; // Currently unused, kept for API compatibility

  fileLogPose("pick_blocks_from_loader");

  // approach loader
  chassis.moveToPoint(x, y, moveTimeout, {.maxSpeed = speed}, false);
  fileLogPose("reached loader");

  int initialDelay = 100;

  // Use color-filtered delay if config provided
  if (colorFilteredDelay(initialDelay, config)) {
    return; // Opponent block detected - exit immediately
  }

  int remainingTime = std::max(0, pickupTime - initialDelay);
  int interval = 300;
  int count = remainingTime / interval;
  int backupTimeout = 100;

  // Use color-filtered shake if config provided, otherwise regular shake
  if (config && config->useColorSensor) {
    if (shakeCustomWithColorFilter(count, 40, backupTimeout, 50,
                                   interval - backupTimeout, config)) {
      return; // Opponent block detected - exit immediately
    }
  } else {
    shakeCustom(count, 70, backupTimeout, 80, interval - backupTimeout);
  }

  colorFilteredDelay(remainingTime % interval, config);
  // Final delay - if opponent detected here, we're done anyway
}

// ============================================================================
// Movement Helpers
// ============================================================================

void swingForward(int moveDuration, int swingDuration, int speed) {
  chassis.arcade(speed, 0);
  delay(moveDuration);
  int backupTimeout = 60, forwardTimeout = 80;
  shakeCustom(swingDuration / (backupTimeout + forwardTimeout), 70,
              backupTimeout, 80, forwardTimeout);
  delay(swingDuration % (backupTimeout + forwardTimeout));
}

void pushLastBlockInForBonusPoints() {
  chassis.arcade(25, 0);
  delay(300);
  chassis.arcade(-25, 0);
  delay(1000);
  chassis.arcade(0, 0);
}
