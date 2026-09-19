#include "auton/left.h"
#include "async_utils.h"
#include "auton/common.h"
#include "color_filter.h"
// Tuning Constants
const float SPEED_TO_LOADER = 65;
// decrease the pickup time to pick up less blocks
const float LEFT_LOADER_PICKUP_TIME = 80;

#include "movement_utils.h"
#include "pros/motors.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"
#include "roller_control.h"

using namespace pros;

// ============================================================================
// Alliance Left Autonomous
// ============================================================================

void auton_alliance_left(const AutonConfig &config,
                         bool pickBlocksUnderLongGoal, bool descore) {
  init_auton(config);
  // (0, 0) is to the right of the parking zone, against the wall
  chassis.setPose(-3.4, 11.5, -23);

  controll_rollers(600, -50);

  // first 3 blocks
  chassis.moveToPose(
      -6, 34.5, 5, 600,
      {.lead = 0.2, .maxSpeed = 120, .minSpeed = 20, .earlyExitRange = .5});
  // change x to adjust the angle of the robot
  chassis.moveToPoint(-4, 38, 500,
                      {.maxSpeed = 40, .minSpeed = 20, .earlyExitRange = .8},
                      false);

  if (pickBlocksUnderLongGoal) {
    // blocks under the long goal
    chassis.moveToPose(
        -31, 58.3, -87, 1500,
        {.lead = .28, .maxSpeed = 90, .minSpeed = 20, .earlyExitRange = 0.7},
        false);

    // way to the middle goal
    chassis.moveToPose(-3.3, 42, -43, 1500,
                       {.forwards = false,
                        .lead = .4,
                        .maxSpeed = 120,
                        .minSpeed = 20,
                        .earlyExitRange = 0.7},
                       true);
  }

  // score middle goal
  chassis.turnToHeading(-135, 500, {.maxSpeed = 60, .minSpeed = 20});
  chassis.moveToPose(6.3, 53.6, -135, 1200,
                     {.forwards = false,
                      .lead = 0.01,
                      .maxSpeed = 90,
                      .minSpeed = 40,
                      .earlyExitRange = 4},
                     false);
  middleGoalActuator.deploy();
  controll_rollers(-20, -15);
  delay(100);
  // Use autonomous anti-jam for scoring
  controll_rollers_auton(600, 400, 800, true);
  stop_rollers();
  delay(160);
  runAsync(
      [&]() {
        middleGoalActuator.retract();
        delay(100);
        hold_intake();
        loaderActuator.deploy();
        delay(300);
        intake_only();
      },
      200);

  // move to loader line
  chassis.moveToPose(-28.5, 21.3, -131, 850, {.lead = 0.01}); // will time out
  chassis.moveToPose(
      -31.9, 21.3, -150, 650,
      {.lead = 0.01, .maxSpeed = 50, .minSpeed = 40, .earlyExitRange = 2});
  chassis.turnToHeading(-180, 200);
  pick_blocks_from_loader(-33.5, 1.6, -180, SPEED_TO_LOADER,
                          (1000 * 60) / SPEED_TO_LOADER,
                          LEFT_LOADER_PICKUP_TIME, &config);

  // score long goal
  runAsync([&]() { loaderActuator.retract(); }, 200);

  chassis.moveToPose(-31.4, 37.6, -180, 600,
                     {.forwards = false,
                      .lead = 0.01,
                      .maxSpeed = 120,
                      .minSpeed = 40,
                      .earlyExitRange = 2}); // will time out
  goAndScoreHighGoal(-31.4, 37.6, -180, 120, 600, 2000);
  stop_rollers();

  // descore
  if (descore) {
    performDescore(pickBlocksUnderLongGoal);
  }
  delay(9000);
}
