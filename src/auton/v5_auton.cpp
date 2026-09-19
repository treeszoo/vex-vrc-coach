#include "auton/v5_auton.h"
#include "robot_config.h"
#include "subsystem/rope.h"
#include "lemlib/timer.hpp"

void near_2() {
    chassis.setPose(0, 0, 0);
    toggle.set_value(false);

    Lefttoggle.move_relative(180 * 2.5, 127);
    pros::delay(300);
    Lefttoggle.move_relative(180 * 2.5, 127);
    startRopeTask(550, 127, 1300);
    pros::delay(200);
    chassis.swingToHeading(-50, lemlib::DriveSide::RIGHT, 500);
    chassis.waitUntilDone();  

    chassis.arcade(-50, 0);
    pros::c::delay(680);
    chassis.arcade(0, 0);
    pros::c::delay(100);
    startRopeTask(200, 30, 1000);
    pros::c::delay(300);
    claw.set_value(false);
    pros::c::delay(150);

    chassis.arcade(50, 0);
    pros::c::delay(200);
    startRopeTask(0, 127, 500);

    chassis.swingToHeading(18, lemlib::DriveSide::RIGHT, 400, {.maxSpeed = 70});
    chassis.waitUntilDone();
    chassis.tank(-15, -80);

    lemlib::Timer turnT1(1500);
    turnT1.reset();
    while (chassis.getPose().theta < 60 && !turnT1.isDone()) {
        pros::c::delay(10);
    }

    chassis.swingToHeading(110, lemlib::DriveSide::LEFT, 200);
    chassis.waitUntilDone();
    pros::c::delay(150);
    claw.set_value(true);
    pros::delay(200);

    chassis.turnToHeading(25, 650, {.maxSpeed = 60, .earlyExitRange = 1});
    startRopeTask(800, 127, 1500);
    chassis.waitUntilDone();
    chassis.moveToPoint(11, 6, 800, {.maxSpeed = 50});
    chassis.turnToHeading(-30, 500, {.maxSpeed = 90});
    chassis.waitUntilDone();
    chassis.arcade(-60, 0);
    pros::c::delay(800);
    chassis.arcade(0, 0);
    startRopeTask(150, 25, 1000);
    pros::c::delay(200);
    claw.set_value(false);
    pros::c::delay(100);

    chassis.moveToPoint(1.8, 28, 2000, {.maxSpeed = 70});
    startRopeTask(0, 40, 900);
    chassis.turnToHeading(146.8, 700, {.maxSpeed = 90});
    chassis.waitUntilDone();
    chassis.swingToHeading(165, lemlib::DriveSide::LEFT, 200, {.maxSpeed = 70});
    chassis.waitUntilDone();
    chassis.tank(-80, -20);

    lemlib::Timer turnT2(1500);
    turnT2.reset();
    while (chassis.getPose().theta > 100 && !turnT2.isDone()) {
        pros::c::delay(10);
    }

    chassis.swingToHeading(80, lemlib::DriveSide::RIGHT, 500);
    pros::c::delay(250);
    claw.set_value(true);
    pros::delay(300);

    chassis.waitUntilDone();
    chassis.turnToHeading(160, 700, {.maxSpeed = 80});
    startRopeTask(1100, 127, 1500);
    chassis.moveToPoint(8.75, -8.52, 500, {.maxSpeed = 70});
    chassis.turnToHeading(192, 500, {.maxSpeed = 80});
    chassis.waitUntilDone();
    chassis.arcade(-70, 0);
    pros::delay(800);
    startRopeTask(400, 40, 1800);
    pros::c::delay(250);
    claw.set_value(false);
    pros::delay(300);
    chassis.waitUntilDone();
    chassis.arcade(70, 0);
    pros::delay(300);
    chassis.arcade(0, 0);
    chassis.turnToHeading(306.65, 500, {.maxSpeed = 80});
    startRopeTask(0, 90, 700);
    chassis.moveToPoint(1.5, 7.8, 800, {.forwards = false, .maxSpeed = 80});
}

void far_2() {
    chassis.setPose(0, 0, 0);
    toggle.set_value(false);

    Righttoggle.move_relative(180 * 2.5, 127);
    pros::delay(300);
    Righttoggle.move_relative(180 * 2.5, 127);
    startRopeTask(450, 127, 1300);
    pros::delay(200);
    chassis.swingToHeading(50, lemlib::DriveSide::LEFT, 500);
    chassis.waitUntilDone();  

    chassis.arcade(-50, 0);
    pros::c::delay(500);
    chassis.arcade(-30, 0);
    pros::c::delay(250);
    chassis.arcade(0, 0);
    pros::c::delay(100);
    startRopeTask(220, 40, 900);
    pros::c::delay(200);
    claw.set_value(false);
    pros::c::delay(150);

    chassis.arcade(50, 0);
    pros::c::delay(200);
    startRopeTask(0, 127, 500);

    chassis.swingToHeading(-20, lemlib::DriveSide::LEFT, 400, {.maxSpeed = 70});
    chassis.waitUntilDone();
    chassis.tank(-80, -20);    

    lemlib::Timer turnT3(1500);
    turnT3.reset();
    while (chassis.getPose().theta > -60 && !turnT3.isDone()) {
        pros::c::delay(10);
    }

    chassis.swingToHeading(-110, lemlib::DriveSide::RIGHT, 300);
    pros::c::delay(250);
    claw.set_value(true);
    pros::delay(300);

    chassis.turnToHeading(-25, 800, {.maxSpeed = 60, .earlyExitRange = 1});
    startRopeTask(800, 127, 900);
    chassis.waitUntilDone();
    chassis.moveToPoint(-11, 6, 800, {.maxSpeed = 60});
    chassis.turnToHeading(30, 400, {.maxSpeed = 60});
    chassis.waitUntilDone();
    chassis.arcade(-50, 0);
    pros::c::delay(400);
    chassis.arcade(-30, 0);
    pros::c::delay(500);
    chassis.arcade(0, 0);
    startRopeTask(180, 40, 900);
    pros::c::delay(300);
    claw.set_value(false);
    pros::c::delay(100);

    chassis.moveToPoint(-1.5, 29, 2500, {.maxSpeed = 70});
    startRopeTask(0, 40, 900);
    chassis.turnToHeading(-146.8, 700, {.maxSpeed = 90});
    chassis.waitUntilDone();
    chassis.swingToHeading(-165, lemlib::DriveSide::RIGHT, 300, {.maxSpeed = 70});
    chassis.waitUntilDone();

    chassis.tank(-20, -80);

    lemlib::Timer turnT4(1000);
    turnT4.reset();
    while (chassis.getPose().theta < -100 && !turnT4.isDone()) {
        pros::c::delay(10);
    }

    chassis.swingToHeading(-80, lemlib::DriveSide::LEFT, 500);
    pros::c::delay(250);

    chassis.waitUntilDone();
    claw.set_value(true);
    pros::delay(200);
    chassis.turnToHeading(-160, 700, {.maxSpeed = 80});
    startRopeTask(1200, 127, 1500);
    chassis.waitUntilDone();
    chassis.arcade(70, 0);
    pros::delay(500);
    chassis.turnToHeading(-200, 300, {.maxSpeed = 80});
    chassis.waitUntilDone();
    chassis.arcade(-60, 0);
    pros::delay(800);

    chassis.arcade(0, 0);
    startRopeTask(500, 60, 1500);
    pros::c::delay(250);
    claw.set_value(false);
    pros::delay(300);

    chassis.waitUntilDone();
    chassis.arcade(70, 0);
    pros::delay(200);
    chassis.arcade(0, 0);
    startRopeTask(0, 80, 1000);    
    chassis.turnToHeading(-306.65, 500, {.maxSpeed = 80});

    chassis.moveToPoint(-1.5, 7.8, 1000, {.forwards = false, .maxSpeed = 80});
    chassis.waitUntilDone();
}