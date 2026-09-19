#include "movement_utils.h"
#include "async_utils.h"
#include "file_logger.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "roller_control.h"

using namespace pros;

// ============================================================================
// Position Tracking
// ============================================================================

// Track absolute position across multiple resets
static lemlib::Pose absolute_offset(0, 0, 0);

void reset_pose() {
  // Get current pose before resetting
  lemlib::Pose current = chassis.getPose();

  // Accumulate the current position into the absolute offset
  absolute_offset.x += current.x;
  absolute_offset.y += current.y;
  absolute_offset.theta += current.theta;

  // Reset the chassis pose to origin
  chassis.setPose(0, 0, 0);
}

// ============================================================================
// Movement Helpers
// ============================================================================

void shakeCustom(int count, int backupSpeed, int backupTimeout,
                 int forwardSpeed, int forwardTimeout) {
  for (int i = 0; i < count; i++) {
    chassis.arcade(-backupSpeed, 0);
    delay(backupTimeout);
    fileLogPose("shake backward");

    chassis.arcade(forwardSpeed, 0);
    delay(forwardTimeout);
    fileLogPose("shake forward");
  }
}

void clear_loader(float x, float y, float theta, float speed, int moveTimeout,
                  int extraTimeout) {
  chassis.moveToPose(x, y, theta, moveTimeout,
                     {.lead = 0.01, .maxSpeed = speed}, false);
  delay(600);
  shakeCustom(2, 30, 80, 50, 500);
  if (extraTimeout > 0) {
    delay(extraTimeout);
  }
}

void goAndScoreHighGoal(float x, float y, float theta, float speed,
                        int moveTimeout, int scoreTimeout) {
  fileLogPose("goAndScoreHighGoal");
  chassis.moveToPose(x, y, theta, moveTimeout,
                     {.forwards = false,
                      .lead = 0.01,
                      .maxSpeed = speed,
                      .minSpeed = 40,
                      .earlyExitRange = 0.3},
                     false);
                  
  fileLogPose("Reached high goal");

  runAsync(
      [&]() {
        fileLogPose("Backing up agst high goal");
        // move back against the goal
        chassis.arcade(-40, 0);
        delay(400);
        chassis.arcade(0, 0);
        fileLogPose("Backed up agst high goal");
      },
      100);
  // Use autonomous anti-jam function
  controll_rollers_auton(INTAKE_HIGH_GOAL_VELOCITY, OUTTAKE_HIGH_GOAL_VELOCITY,
                         scoreTimeout, true);
}
