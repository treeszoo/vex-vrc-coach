#include "async_utils.h"
#include "auton/common.h"
#include "auton/skill.h"
#include "movement_utils.h"
#include "distance_movement.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"
#include "roller_control.h"

using namespace pros;

// ============================================================================
// Skills Constants
// ============================================================================

const float FORWARD_OFFSET_FROM_GOAL = 17.5;
const float DISTANCE_BETWEEN_LOADERS = 95;
const float DISTANCE_FROM_GOAL_TO_LOADER = 33.8;
const float DISTANCE_FROM_GOAL_TO_WALL = 26;
const bool DEBUG = false;

// ============================================================================
// Skills Phase 1: Loaders 1 & 2
// ============================================================================

static void loaderOneAndTwo() {
  chassis.setPose(-13.476, 1.68, 0);

  // load 1
  chassis.moveToPose(-13.48, -27.8, 0, 900,
                     {.forwards = false,
                      .lead = 0,
                      .maxSpeed = 100,
                      .minSpeed = 30,
                      .earlyExitRange = 0});

  chassis.turnToHeading(90, 600, {.minSpeed = 20, .earlyExitRange = 0.1});
  intake_only();
  loaderActuator.deploy();
  chassis.waitUntilDone();
  clear_loader(-1.5, -29.5, 90, 50, 900, 400);

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
      -105, -32, 1500,
      {.forwards = false, .maxSpeed = 80, .minSpeed = 60, .earlyExitRange = 2});
  chassis.waitUntilDone();
  delay(200);

  chassis.turnToHeading(
      270, 900, {.maxSpeed = 70, .minSpeed = 20, .earlyExitRange = 0.1});
  goAndScoreHighGoal(-83, -31, 270, 80, 800, 2800);

  // load 2
  loaderActuator.deploy();
  intake_only();
  clear_loader(-139, -30.5, 267, 50, 1500);

  // score 2
  runAsync(
      [&]() {
        hold_intake();
        loaderActuator.retract();
      },
      300);
  goAndScoreHighGoal(-83, -30.5, 271, 70, 1100, 2800);

  //pushLastBlockInForBonusPoints();
  stop_rollers();
}

// ============================================================================
// Skills Phase 2: Middle Goal & Loader 3


// ============================================================================

//test
static void testy() {
  reset_pose();
} 


static void clearParkingZone() {
  reset_pose();

  //go in front of the parking zone
  runAsync([&]() { intake_only(); }, 900);
  chassis.moveToPoint(9.5, 27, 900,
     {.maxSpeed = 70});
  chassis.turnToHeading(90, 500, {}, false);
  chassis.moveToPoint(30.5, 37, 1200, {.maxSpeed = 80}, false);

  wheelupActuator.deploy();
  delay(30);

  //charge into parking + intake 6 blocks
  
  chassis.arcade(75, 0);
  delay(1400);
  chassis.arcade(0,0);
  delay(50);
  chassis.turnToHeading(90, 100);
  
  move_to_distance(distance_front, true, 1000, 65);
  wheelupActuator.retract();
  delay(30);
  chassis.turnToHeading(90,100,{});
}



static void scoreMiddleGoalAndClearLoaderThree() {

  chassis.setPose(72.5,25.2,90);

  delay(500);

  //get a red ball
  chassis.swingToHeading(204, lemlib::DriveSide::RIGHT, 900,
    {.minSpeed = 70, .earlyExitRange = 0.2});
  chassis.moveToPose(73.8,-5.6,208,900,{.lead=0.12});
  
  

  // score center goal
  
  chassis.moveToPose(
      71.9, -18.3, 180, 1100,
      {.lead = 0.1,.maxSpeed=70, .minSpeed = 40},
      false);
  chassis.turnToHeading(45,1000,{.maxSpeed=50,.earlyExitRange=0.2});
  //chassis.moveToPoint(60.9,-27.2,1300,{.forwards=false},false);
  chassis.moveToPose(
    61, -27.2, 45, 1200,
    {.forwards = false, .lead = 0.01, .minSpeed = 40, .earlyExitRange = 3},
    false);
  middleGoalActuator.deploy();
  delay(100);
  // move forward a bit to avoid pushing blocks out of the loader
  runAsync(
      [&]() {
        chassis.arcade(20, 0);
        delay(80);
        chassis.arcade(0, 0);
      },
      50);
  controll_rollers_auton(200, 300, 2000, false);
  controll_rollers_auton(300,300,2000,false);

  chassis.setPose(59.4,-14.6,39);
      //L
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

  chassis.turnToHeading(45, 500);

  chassis.moveToPose(DISTANCE_BETWEEN_LOADERS - 3, 24.59, 39, 600,
                     {.lead = 0.01}, false); // will time out
  chassis.moveToPose(
      DISTANCE_BETWEEN_LOADERS - 3, 24.59, 39, 1000,
      {.lead = 0.01, .maxSpeed = 50, .minSpeed = 30, .earlyExitRange = 1});
  chassis.turnToHeading(0, 200, {});
  //95,36.8
  clear_loader(DISTANCE_BETWEEN_LOADERS-3, DISTANCE_FROM_GOAL_TO_LOADER + 3, 0,
               50, 700, 200); // loader 3
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
  chassis.moveToPose(-75, 16.8, 225, 1200,
                     {.forwards = false,
                      .lead = 0.4,
                      .maxSpeed = 100,
                      .minSpeed = 60,
                      .earlyExitRange = 2});

  // score 3
  chassis.turnToHeading(269, 700, {});
  runAsync([&]() { loaderActuator.deploy(); }, 3000);
  goAndScoreHighGoal(-62, 18.3, 270, 65, 1000, 2850);
}

// ============================================================================
// Skills Phase 4: Loader 4
// ============================================================================

static void clearAndScoreLoaderFour() {
  // loader 4
  chassis.setPose(0, 0, 0);
  loaderActuator.deploy();
  intake_only();
  chassis.moveToPose(
      1, 35, 0, 500,
      {.lead = 0.1, .maxSpeed = 80, .minSpeed = 40, .earlyExitRange = 2});
  clear_loader(1, 35, 0, 50, 1400);
    
  // score 4
  runAsync([&]() { loaderActuator.retract(); }, 600);
  goAndScoreHighGoal(0, -2, 0, 80, 1200, 2850);

  // push last block in for bonus points
  //pushLastBlockInForBonusPoints();
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
  runAsync([&](){ loaderActuator.deploy();},450);
  chassis.moveToPoint(30.5, 37, 1200, {.maxSpeed = 80}, false);
  wheelupActuator.deploy();

  chassis.tank(70, 70);
  delay(1050);
  loaderActuator.retract();
  delay(150);
  chassis.tank(0, 0);
}

// ============================================================================
// Skills Main Routine
// ============================================================================
void skill_aggr(const AutonConfig &config) {
  (void)config; // TODO: use config.teamColor and config.useColorSensor
  // retract the wheelup actuator to deploy the tracking wheel
  wheelupActuator.retract();
  chassis.setBrakeMode(pros::E_MOTOR_BRAKE_BRAKE);

  loaderOneAndTwo();
  alignToLongGoal_fast();
  //testy();
  clearParkingZone();
  scoreMiddleGoalAndClearLoaderThree();
  scoreLoaderThree();
  clearAndScoreLoaderFour();
  park();
}
