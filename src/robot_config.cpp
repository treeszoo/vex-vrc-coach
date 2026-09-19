#include "robot_config.h"
#include "pneumatic_actuator.hpp"
#include "pros/distance.hpp"
#include <random>

// controller
// pros::Controller controller(pros::E_CONTROLLER_MASTER);

// intake
pros::Motor intakeMotor(10, pros::MotorGearset::blue);

// outtake
pros::Motor outtakeMotor(-17, pros::MotorGearset::blue);

// drivetrain left
// pros::MotorGroup leftMotors({-11, -12, -13}, pros::MotorGearset::blue);
// pros::MotorGroup rightMotors({14, 15, 16}, pros::MotorGearset::blue);

// odometry wheels
// pros::Rotation verticalEnc(-9);
// pros::Rotation horizontalEnc(-18);

// pneumatics
PneumaticActuator loaderActuator(1);
PneumaticActuator middleGoalActuator(2);
PneumaticActuator descorerActuator(3);
PneumaticActuator wheelupActuator(4);

// not used yet
pros::Optical optical(19);

// distance sensor (front)
pros::Distance distance_front(20);

// imu tracks the robot's orientation
//pros::Imu imu(5);



// lemlib defs
/* lemlib::TrackingWheel vertical(&verticalEnc, lemlib::Omniwheel::NEW_275, 0);
lemlib::TrackingWheel horizontal(&horizontalEnc, lemlib::Omniwheel::NEW_275,
                                 -2);*/

/*lemlib::Drivetrain drivetrain(
    &leftMotors,                // left motor group
    &rightMotors,               // right motor group
    11.34,                      // 10 inch track width
    lemlib::Omniwheel::NEW_325, // using new 3.25" omnis
    450,                        // drivetrain rpm is 400
    4 // horizontal drift is 2. If we had traction wheels, it would have been 8
);*/

// lateral motion controller
/*lemlib::ControllerSettings
    linearController(12.5,  // proportional gain (kP)
                     0.001, // integral gain (kI)
                     50,    // derivative gain (kD)
                     3,     // anti windup
                     0,     // small error range, in inches
                     0,     // small error range timeout, in milliseconds
                     0,     // large error range, in inches
                     0,     // large error range timeout, in milliseconds
                     0      // maximum acceleration (slew)
    );

lemlib::ControllerSettings
    angularController(3.5,   // proportional gain (kP)
                      0.001, // integral gain (kI)
                      18,    // derivative gain (kD)
                      5,     // intergral anti windup
                      0,     // small error range, in degrees
                      0,     // small error range timeout, in milliseconds
                      0,     // large error range, in degrees
                      0,     // large error range timeout, in milliseconds
                      0      // maximum acceleration (slew)
}

// sensors for odometry
lemlib::OdomSensors sensors(&vertical, // vertical tracking wheel
                            nullptr,   // vertical tracking wheel 2, set to
                                       // nullptr as we don't have a second one
                            &horizontal, // horizontal tracking wheel
                            nullptr,     // horizontal tracking wheel 2, set to
                                     // nullptr as we don't have a second one
                            &imu // inertial sensor
);*/

// input curve for throttle input during driver control
/*lemlib::ExpoDriveCurve
    throttleCurve(3,    // joystick deadband out of 127
                  7,    // minimum output where drivetrain will move out of 127
                  0.999 // expo curve gain
                        // 1.021
    );

// input curve for steer input during driver control
lemlib::ExpoDriveCurve
    steerCurve(5,    // joystick deadband out of 127
               5,    // minimum output where drivetrain will move out of 127
               1.01L // expo curve gain
                     // 1.021
    );

// create the chassis
lemlib::Chassis chassis(drivetrain, linearController, angularController,
                        sensors, &throttleCurve, &steerCurve

);*/

// >> V5 part

#include "lemlib/api.hpp"
#include "lemlib/chassis/chassis.hpp"
#include "lemlib/chassis/trackingWheel.hpp"
#include "pros/distance.hpp"
#include "pros/rtos.hpp"
#include "main.h"




// controller
pros::Controller controller(pros::E_CONTROLLER_MASTER);

//toggle motors
pros::Motor Lefttoggle(16,pros::MotorGearset::green);
pros::Motor Righttoggle(-17,pros::MotorGearset::green);


//right rope updown
pros::MotorGroup Rope({-5,15},pros::MotorGearset::green);

//drivetrain
pros::MotorGroup leftMotors({-1, 2, -3},pros::MotorGearset::blue);
pros::MotorGroup rightMotors({11, -12, 13}, pros::MotorGearset::blue);



//pneumatics

pros::adi::DigitalOut claw(1);
pros::adi::DigitalOut toggle(2);
pros::adi::DigitalOut touchtoggle(3);


//odometry wheel sensors
pros::Rotation verticalEnc(8);
pros::Rotation horizontalEnc(-9);

//imu
pros::Imu imu(7);

//lemlib defs
lemlib::TrackingWheel vertical(&verticalEnc, 1.98, 0);
lemlib::TrackingWheel horizontal(&horizontalEnc, lemlib::Omniwheel::NEW_275, -2.56);


lemlib::Drivetrain drivetrain(
    &leftMotors, // left motor group
    &rightMotors, // right motor group
    11.34, // 11.34 inch track width
    lemlib::Omniwheel::NEW_275, // using new 3.25" omnis
    450, // drivetrain rpm is 450
    4 // horizontal drift is 2. If we had traction wheels, it would have been 8
);

// lateral motion controller
lemlib::ControllerSettings linearController(
    16, // proportional gain (kP)
    0.001, // integral gain (kI)
    65, // derivative gain (kD)
    3, // anti windup
    1, // small error range, in inches
    100, // small error range timeout, in milliseconds
    0, // large error range, in inchess
    0, // large error range timeout, in milliseconds
    45// maximum acceleration (slew)
);

lemlib::ControllerSettings angularController(
    3.6, // proportional gain (kP)
    0.001, // integral gain (kI)
    30, // derivative gain (kD)
    8, // intergral anti windup
    0, // small error range, in degrees
    0, // small error range timeout, in milliseconds
    0, // large error range, in degrees
    0, // large error range timeout, in milliseconds
    0 // maximum acceleration (slew)
);

// sensors for odometry
lemlib::OdomSensors sensors(&vertical, // vertical tracking wheel
                            nullptr, // vertical tracking wheel 2, set to nullptr as we don't have a second one
                            &horizontal, // horizontal tracking wheel
                            nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
                            &imu // inertial sensor
);

// input curve for throttle input during driver control
lemlib::ExpoDriveCurve throttleCurve(10, // joystick deadband out of 127
                                     8, // minimum output where drivetrain will move out of 127
                                     1.003 // expo curve gain
                                     //1.021
);

// input curve for steer input during driver control
lemlib::ExpoDriveCurve steerCurve(10, // joystick deadband out of 127
                                 5, // minimum output where drivetrain will move out of 127
                                  1 // expo curve gain
                                  //1.021
); 

// create the chassis
lemlib::Chassis chassis(drivetrain, linearController, angularController, sensors, &throttleCurve, &steerCurve

);


