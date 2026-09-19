#include "auton/solo.h"
#include "async_utils.h"
#include "auton/common.h"
// Tuning Constants
const float SPEED_TO_LOADER = 60;

#include "movement_utils.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"
#include "roller_control.h"
#include "file_logger.h"
using namespace pros;

// ============================================================================
// Alliance Solo Autonomous
// ============================================================================


void auton_solo_right(const AutonConfig &config,  bool push_alliance) {
  init_auton(config);

  // right start
  chassis.setPose(0, 0, 0);
  fileLogPose("start pose");
  intake_only();

  //push the other robot
  if (push_alliance == true) {
    chassis.arcade(55, 0);
    delay(240);

    // intake the block between bots
    chassis.arcade(20, 0);
    delay(150);
    chassis.arcade(0, 0);
  }
    
  
  //back up to loader
  chassis.moveToPoint(0, -32.3, 1000,
                      {.forwards = false,
                       .maxSpeed = 90,
                       .minSpeed = 30,
                       .earlyExitRange = 3},false);
  delay(200);

  
  runAsync([&]() { loaderActuator.deploy(); }, 100);
  chassis.turnToHeading(-89, 310,
                        {.minSpeed = 50, .earlyExitRange = 10}); // times out
  delay(100);
  
  //pick from first loader
  pick_blocks_from_loader(-14.5, -31.5, -85, SPEED_TO_LOADER, 850,
                          config.useColorSensor ? 2000 : 220, &config);
  fileLogPose("loader 1");
  //score
  chassis.moveToPose(
    19.5, -31, -90, 950,
     {.forwards = false, .lead = 0.1, .minSpeed = 70, .earlyExitRange = 1},
     false);
  runAsync([&]() { loaderActuator.retract(); }, 500);
  fileLogPose("score 4 blocks");
  controll_rollers_auton(600, 600, 1350, true);

  //turn to 3 blocks
  intake_only();
  
  chassis.turnToHeading(12, 700,{.maxSpeed = 95,.earlyExitRange=8});

  //intake 3+3 blocks

  chassis.moveToPoint(27.6, 9.4, 800,{.maxSpeed = 110, .earlyExitRange = 2});

  //also use loader arm to stop the 3 blocks
  runAsync([&]() { loaderActuator.deploy(); }, 1450);
  chassis.moveToPose(25,34.5,344,1000,{.lead=0.01, .maxSpeed=90, .minSpeed=20, .earlyExitRange=3});
  fileLogPose("intaked 6 blocks");
  //score middle goal
  chassis.turnToHeading(315,330,{.earlyExitRange=5});

  chassis.moveToPoint(36.8,24.3,720,{.forwards=false, .maxSpeed=70},false);
  //score middle goal
  middleGoalActuator.deploy();
  delay(30);
  
  controll_rollers_auton(300, 300, 800);
  chassis.turnToHeading(-45, 100);
  stop_rollers();
  fileLogPose("middlegoal");
  
  delay(30);
  
  
  runAsync([&]() { middleGoalActuator.retract(); }, 700);
  
  //move to loader line
  chassis.moveToPoint(3.7, 61.9, 1100,{.maxSpeed=90, .earlyExitRange=11},false);
  intake_only();
  delay(50);
  chassis.turnToHeading(255, 270,{.maxSpeed=60},false);
  delay(70);

  pick_blocks_from_loader(-14.6, 62, -83, SPEED_TO_LOADER, 850, 
    config.useColorSensor ? 2000 : 220,&config);

    chassis.moveToPose(
      21, 62, -90, 950,
       {.forwards = false, .lead = 0.1, .minSpeed = 70, .earlyExitRange = 1},
       false);
    runAsync([&]() { loaderActuator.retract(); }, 500);
  
  controll_rollers_auton(600, 600, 1350, true);
  //stop_rollers();

}
