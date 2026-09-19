#include "async_utils.h"
#include "auton/common.h"
#include "auton/skill.h"
#include "movement_utils.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"
#include "roller_control.h"

using namespace pros;

// ============================================================================
// Skills Constants
// ============================================================================

const float FORWARD_OFFSET_FROM_GOAL = 17.5;
const float DISTANCE_BETWEEN_LOADERS = 94;
const float DISTANCE_FROM_GOAL_TO_LOADER = 33.8;
const float DISTANCE_FROM_GOAL_TO_WALL = 26;
const bool DEBUG = false;

// ============================================================================
// Skills Phase 1: Loaders 1 & 2
// ============================================================================

static void loaderOneAndTwo() {
  chassis.setPose(-13.476, 1.68, 0);

  // load 1
  chassis.moveToPose(-13.48, -27.2, 0, 900,
                     {.forwards = false,
                      .lead = 0,
                      .maxSpeed = 100,
                      .minSpeed = 30,
                      .earlyExitRange = 0});

  chassis.turnToHeading(90, 600, {.minSpeed = 20, .earlyExitRange = 0.1});
  intake_only();
  loaderActuator.deploy();
  chassis.waitUntilDone();
  clear_loader(-1.5, -29.5, 90, 50, 900);

  // go to other side to score
  runAsync(
      [&]() {
        loaderActuator.retract();
        intake_only();
      },
      900);
  chassis.moveToPose(-28.5, -42, 65, 1000,
                     {.forwards = false, .minSpeed = 80, .earlyExitRange = 3});
  chassis.moveToPose(-105, -44, 90, 1200,
                     {.forwards = false, .minSpeed = 80, .earlyExitRange = 2});
  chassis.moveToPoint(
      -105, -30.3, 1500,
      {.forwards = false, .maxSpeed = 80, .minSpeed = 60, .earlyExitRange = 2});
  chassis.waitUntilDone();
  delay(200);

  chassis.turnToHeading(
      270, 900, {.maxSpeed = 70, .minSpeed = 20, .earlyExitRange = 0.1});
  goAndScoreHighGoal(-83, -30.3, 270, 80, 800, 2800);

  // load 2
  loaderActuator.deploy();
  intake_only();
  clear_loader(-139, -28.7, 270, 50, 1500);

  // score 2
  runAsync(
      [&]() {
        hold_intake();
        loaderActuator.retract();
      },
      300);
  goAndScoreHighGoal(-83, -30.3, 271, 70, 1100, 2800);

  pushLastBlockInForBonusPoints();
  stop_rollers();
}

// ============================================================================
// Skills Phase 2: Middle Goal & Loader 3
// ============================================================================

static void scoreMiddleGoalAndClearLoaderThree() {
  chassis.setPose(19.5, -31, -90);
  //reset_pose();
  runAsync([&]() { intake_only(); }, 900);
  intake_only();
  
  chassis.turnToHeading(12, 700,{.maxSpeed = 95,.earlyExitRange=8});

  //intake 3+3 blocks

  chassis.moveToPoint(27.6, 9.4, 800,{.maxSpeed = 110, .earlyExitRange = 2});

  //also use loader arm to stop the 3 blocks
  runAsync([&]() { loaderActuator.deploy(); }, 1450);
  chassis.moveToPose(25,36.5,344,1000,{.lead=0.01, .maxSpeed=90, .minSpeed=20, .earlyExitRange=3});
  
  //score middle goal
  chassis.turnToHeading(315,330,{.earlyExitRange=5});

  chassis.moveToPoint(37.5,26.7,1000,{.forwards=false, .maxSpeed=70},false);
  middleGoalActuator.deploy();
  delay(100);
  // move forward a bit to avoid pushing blocks out of the loader
  runAsync(
      [&]() {
        chassis.arcade(20, 0);
        delay(110);
        chassis.arcade(0, 0);
        delay(3500);
        loaderActuator.retract();
      },
      50);
  controll_rollers_auton(200, 300, 4000, true);

  runAsync(
      [&]() {
        middleGoalActuator.retract();
        // clear all blocks to avoid messing up with the bonus points in the
        // next loader (600 RPM intake, 600 RPM outtake)
        controll_rollers_auton(600, 600, 300, true);
        loaderActuator.deploy();
        delay(1000);
        intake_only();
      },
      300);

  chassis.setPose(59.4,-14.6,39);

  chassis.moveToPose(DISTANCE_BETWEEN_LOADERS , 24.59, 39, 600,
                     {.lead = 0.01}); // will time out
  chassis.moveToPose(
      DISTANCE_BETWEEN_LOADERS , 24.59, 39, 1500,
      {.lead = 0.01, .maxSpeed = 50, .minSpeed = 30, .earlyExitRange = 1},false);
  chassis.turnToHeading(0, 200, {},false);
  clear_loader(DISTANCE_BETWEEN_LOADERS , DISTANCE_FROM_GOAL_TO_LOADER + 3, 0,
               50, 700, 200); // loader 3
}

static void test(){
  chassis.setPose(19.5, -31, -90);
  
}
// ============================================================================
// Skills Phase 3: Score Loader 3
// ============================================================================

static void scoreLoaderThree() {
  // loader 3 pose: (95.7, 30.4, 0)
  runAsync(
      [&]() {
        loaderActuator.retract();
        delay(1200);
        hold_intake();
      },
      1000);
  // back out of the loader
  chassis.moveToPoint(DISTANCE_BETWEEN_LOADERS,
                      DISTANCE_FROM_GOAL_TO_LOADER - 8, 800,
                      {.forwards = false, .minSpeed = 80, .earlyExitRange = 2});

  // back and turn to the wall
  chassis.moveToPose(DISTANCE_BETWEEN_LOADERS + DISTANCE_FROM_GOAL_TO_WALL,
                     FORWARD_OFFSET_FROM_GOAL, -90, 1600,
                     {.forwards = false,
                      .lead = 0.5,
                      .maxSpeed = 50,
                      .minSpeed = 30,
                      .earlyExitRange = .5},
                     false);
  delay(200);

  // starting against the wall
  reset_pose();

  // go to other side to score
  chassis.swingToHeading(65, lemlib::DriveSide::RIGHT, 600, {.maxSpeed = 70});
  chassis.moveToPose(-12, 3.4, 65, 500,
                     {.forwards = false, .minSpeed = 80, .earlyExitRange = 3});
  chassis.moveToPose(-59, 1, 90, 1200,
                     {.forwards = false, .minSpeed = 100, .earlyExitRange = 2});
  chassis.moveToPose(-75, 15.5, 225, 1200,
                     {.forwards = false,
                      .lead = 0.4,
                      .maxSpeed = 100,
                      .minSpeed = 60,
                      .earlyExitRange = 2});

  // score 3
  chassis.turnToHeading(269, 700, {});
  runAsync([&]() { loaderActuator.deploy(); }, 3000);
  goAndScoreHighGoal(-62, 18.3, 270, 75, 1500, 2850);
}

// ============================================================================
// Skills Phase 4: Loader 4
// ============================================================================

static void clearAndScoreLoaderFour() {
  alignToLongGoal_fast();
  // loader 4
  chassis.setPose(0, 0, 0);
  loaderActuator.deploy();
  intake_only();
  chassis.moveToPose(
      -1, 35, 0, 500,
      {.lead = 0.1, .maxSpeed = 80, .minSpeed = 40, .earlyExitRange = 2});
  clear_loader(-1, 35, 0, 50, 1400);

  // score 4
  runAsync([&]() { loaderActuator.retract(); }, 600);
  goAndScoreHighGoal(-1, -2, 0, 80, 1200, 2850);

  // push last block in for bonus points
  pushLastBlockInForBonusPoints();
  chassis.arcade(0, 0);
}

// ============================================================================
// Skills Phase 5: Park
// ============================================================================

static void park() {
  chassis.setPose(0, 0, 0);
  intake_only();
  chassis.moveToPoint(9.5, 27, 900, {.maxSpeed = 70});
  chassis.turnToHeading(77, 500, {}, false);
  runAsync([&]() { loaderActuator.deploy(); }, 450);
  chassis.moveToPoint(30.5, 37, 1200, {.maxSpeed = 80}, false);
  wheelupActuator.deploy();

  chassis.tank(70, 72);
  delay(1050);
  loaderActuator.retract();
  delay(150);
  chassis.tank(0, 0);
}

// ============================================================================
// Skills Main Routine
// ============================================================================
void skill_safe(const AutonConfig &config) {
  (void)config; // TODO: use config.teamColor and config.useColorSensor
  // retract the wheelup actuator to deploy the tracking wheel
  wheelupActuator.retract();
  chassis.setBrakeMode(pros::E_MOTOR_BRAKE_BRAKE);

  loaderOneAndTwo();
  alignToLongGoal_fast();
  scoreMiddleGoalAndClearLoaderThree();
  //test();
  scoreLoaderThree();
  clearAndScoreLoaderFour();
  park();
}
