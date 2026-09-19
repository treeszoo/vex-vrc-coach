#include "auton/right.h"
#include "async_utils.h"
#include "auton/common.h"
#include "color_filter.h"
#include "file_logger.h"
// Tuning Constants
const float SPEED_TO_LOADER = 65;
// increase the pickup time to pick up more blocks
const float RIGHT_LOADER_PICKUP_TIME = 40; // must be over 100 to wait.
const float RIGHT_LOADER_MOVE_TIMEOUT = 920;

#include "movement_utils.h"
#include "pros/motors.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"
#include "roller_control.h"

using namespace pros;

// ============================================================================
// Alliance Right Autonomous
// ============================================================================

void auton_alliance_right(const AutonConfig &config, bool descore) {
  init_auton(config);
  reset_pose();

  fileLogPose("Start Pos");

  // go to the line for the first 3 blocks
  runAsync([&]() { intake_only(); }, 350);
  chassis.moveToPoint(0, 2, 200, {.earlyExitRange = .5});
  chassis.turnToHeading(45, 200, {.earlyExitRange = 3});
  chassis.moveToPoint(6.5, 8, 650, {.earlyExitRange = 0.2});

  // pick up the first 3 blocks
  chassis.turnToHeading(0, 240, {.earlyExitRange = 5}, false);

  fileLogPose("Turned, picking up 3 blocks.");
  chassis.moveToPose(
      7.5, 22.6, 0, 1300,
      {.lead = 0.01, .maxSpeed = 45, .minSpeed = 30, .earlyExitRange = 0.5});
  chassis.waitUntilDone();
  delay(150);
  fileLogPose("First 3 blocks picked up.");

  hold_intake();

  // go to loader line,
  runAsync(
      [&]() {
        loaderActuator.deploy();
        fileLogPose("Loader deployed for goal line.");
        intake_only();
      },
      350);
  chassis.turnToHeading(135, 600, {}, false);

  fileLogPose("Turned, moving to goal line");
  chassis.moveToPose(30.9, 0, 142, 800, {.lead = 0.1}); // will time out
  chassis.moveToPose(30.9, 0, 142, 600,
                     {.lead = 0.1, .maxSpeed = 50, .earlyExitRange = 0.5});
  chassis.turnToHeading(180, 250, {.earlyExitRange = .5});

  // approach loader, pick up only 3 blocks
  //CHANGE THIS AFTER ALL AUTONS <<--
  pick_blocks_from_loader(
      31.9, -8.5, 180, SPEED_TO_LOADER, RIGHT_LOADER_MOVE_TIMEOUT,
      config.useColorSensor ? 2000 : RIGHT_LOADER_PICKUP_TIME, &config);
  loaderActuator.retract();

  // when using color sensor, some blocks might be between the loader and the
  // intake, we need to hold the intake, retract the loader so that these blocks
  // will not be picked up
  if (config.useColorSensor) {
    hold_intake();
    // back up and score long goal
    chassis.moveToPose(32, -2, -180, 500,
                       {
                           .forwards = false,
                           .lead = 0.01,
                           .minSpeed = 40,
                           .earlyExitRange = 0.5,
                       });
    // slow down to let go of the opponent blocks
    stop_rollers();
    chassis.moveToPose(32, 10, -180, 800,
                       {
                           .forwards = false,
                           .lead = 0.01,
                           .maxSpeed = 40,
                           .earlyExitRange = 0.5,
                       });
  }

  goAndScoreHighGoal(32, 21.7, -180, 127, 900, 2000);
  stop_rollers();

  if (descore) {
    performDescore(false);
  }
}
