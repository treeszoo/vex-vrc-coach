#pragma once

#include "lemlib/api.hpp"
#include "pneumatic_actuator.hpp"
#include "pros/distance.hpp"
#include "pros/optical.hpp"

extern pros::Controller controller;
extern pros::Motor intakeMotor;
extern pros::Motor outtakeMotor;

// motor groups
extern pros::MotorGroup leftMotors;
extern pros::MotorGroup rightMotors;

extern pros::Optical optical;

extern pros::Rotation verticalEnc;
extern pros::Rotation horizontalEnc;
extern pros::Imu imu;

extern PneumaticActuator loaderActuator;
extern PneumaticActuator middleGoalActuator;
extern PneumaticActuator descorerActuator;
extern PneumaticActuator wheelupActuator;

extern pros::MotorGroup Rope;

// lemlib defs

extern lemlib::Drivetrain drivetrain;
extern lemlib::Chassis chassis;

// distance sensor (front)
extern pros::Distance distance_front;

// >> V5 part
#include "pros/distance.hpp"

#include "pros/rtos.hpp"
#include "lemlib/api.hpp"


extern pros::Controller controller;
extern pros::MotorGroup leftMotors;
extern pros::MotorGroup rightMotors;

extern pros::MotorGroup Rope;

extern pros::Motor Lefttoggle;
extern pros::Motor Righttoggle;

extern pros::adi::DigitalOut claw;
extern pros::adi::DigitalOut toggle;
extern pros::adi::DigitalOut touchtoggle;

extern pros::Imu imu;
extern pros::Rotation verticalEnc;
extern pros::Rotation horizontalEnc;



//lemlib defs
extern lemlib::TrackingWheel vertical;
extern lemlib::TrackingWheel horizontal;
extern lemlib::Drivetrain drivetrain;
extern lemlib::ControllerSettings linearController;
extern lemlib::ControllerSettings angularController;
extern lemlib::OdomSensors sensors;
extern lemlib::ExpoDriveCurve throttleCurve;
extern lemlib::ExpoDriveCurve steerCurve;
extern lemlib::Chassis chassis;



