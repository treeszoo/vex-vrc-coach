#include "distance_movement.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include <cmath>

// Loop delay in ms — lower = more responsive, higher = less CPU
const int LOOP_DELAY_MS = 1;

int read_filtered_distance(pros::Distance &sensor, int lastValidDistance,
                           double lastSpeed, int minObjectSize) {
  int rawDistance = sensor.get();
  // int objectSize = sensor.get_object_size();

  // // Filter: object too small — likely a passing obstruction
  // if (minObjectSize > 0 && objectSize >= 0 && objectSize < minObjectSize) {
  //   return -1;
  // }

  // // Filter: physically impossible distance jump (noise)
  // // At max speed the robot moves ~30mm per LOOP_DELAY_MS; add margin for
  // // sensor jitter
  // double scale = LOOP_DELAY_MS / 10.0;
  // double maxChange = (std::abs(lastSpeed) / 127.0) * 30.0 * scale + 50.0;
  // if (std::abs(rawDistance - lastValidDistance) > maxChange) {
  //   return -1;
  // }

  return rawDistance;
}

void move_to_distance(pros::Distance &sensor, bool sensorFacesForward,
                      double destination, double maxSpeed, int timeout,
                      int minObjectSize) {
  uint32_t startTime = pros::millis();
  const int TOLERANCE = 10;
  const int OVERSHOOT_COMPENSATION = 40;
  const int MIN_SPEED = 20;
  const double SLOWDOWN_RANGE = 200.0;

  // Ensure maxSpeed is within valid range
  if (maxSpeed > 127)
    maxSpeed = 127;

  // Calculate kP such that we start slowing down at ~200mm error
  // speed = error * kP  =>  kP = speed / error = maxSpeed / 200
  double kP = maxSpeed / SLOWDOWN_RANGE;

  int prevErrorSign = 0;
  int directionChanges = 0;
  const int MAX_DIRECTION_CHANGES = 2;

  int lastValidDistance = sensor.get();
  double lastSpeed = 0;
  int consecutiveRejects = 0;
  const int MAX_CONSECUTIVE_REJECTS = 5;

  while (true) {
    if (timeout != -1 && (pros::millis() - startTime >= (uint32_t)timeout)) {
      break;
    }

    int currentDistance =
        read_filtered_distance(sensor, lastValidDistance, lastSpeed,
                               minObjectSize);

    if (currentDistance < 0) {
      consecutiveRejects++;
      if (consecutiveRejects >= MAX_CONSECUTIVE_REJECTS) {
        // Filter is stuck — accept raw reading as new baseline
        lastValidDistance = sensor.get();
        consecutiveRejects = 0;
      }
      chassis.arcade(lastSpeed, 0);
      pros::delay(LOOP_DELAY_MS);
      continue;
    }

    consecutiveRejects = 0;
    lastValidDistance = currentDistance;
    double error = currentDistance - destination;

    if (std::abs(error) < TOLERANCE + OVERSHOOT_COMPENSATION)
      break; // exit early to let coasting cover the remaining distance

    // Check for oscillation
    int currentErrorSign = (error > 0) ? 1 : -1;
    if (prevErrorSign != 0 && currentErrorSign != prevErrorSign) {
      directionChanges++;
    }
    prevErrorSign = currentErrorSign;

    if (directionChanges > MAX_DIRECTION_CHANGES) {
      break;
    }

    double speed = error * kP;

    if (!sensorFacesForward) {
      speed = -speed;
    }

    // Clamp speed
    if (speed > maxSpeed)
      speed = maxSpeed;
    if (speed < -maxSpeed)
      speed = -maxSpeed;

    // Minimum speed to overcome friction
    if (std::abs(speed) < MIN_SPEED)
      speed = (speed > 0) ? MIN_SPEED : -MIN_SPEED;

    lastSpeed = speed;
    chassis.arcade(speed, 0);
    pros::delay(LOOP_DELAY_MS);
  }
  chassis.arcade(0, 0);
}
