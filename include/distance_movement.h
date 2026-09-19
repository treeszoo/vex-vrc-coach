#pragma once

#include "pros/distance.hpp"

/**
 * @file distance_movement.h
 * @brief Distance sensor-based movement with noise filtering.
 *
 * Provides filtered distance reading and P-controlled movement
 * toward a target distance measured by a VEX Distance sensor.
 */

/**
 * Read a filtered distance from the sensor, rejecting noise.
 *
 * Rejects readings where the detected object is too small (transient
 * obstruction) or the distance changed faster than physically possible
 * given the robot's current speed.
 *
 * @param sensor The distance sensor to read
 * @param lastValidDistance The last accepted distance reading (mm)
 * @param lastSpeed The last commanded motor speed (0-127 scale)
 * @param minObjectSize Minimum object size (0-400) to accept. 0 disables.
 * @return The filtered distance in mm, or -1 if the reading was rejected
 */
int read_filtered_distance(pros::Distance &sensor, int lastValidDistance,
                           double lastSpeed, int minObjectSize);

/**
 * Move to a specific distance from an object using a distance sensor
 *
 * @param sensor The distance sensor to use
 * @param sensorFacesForward True if the sensor is mounted on the front, false
 * if on the back
 * @param destination Target distance in mm
 * @param maxSpeed Maximum speed (0-127)
 * @param timeout Timeout in ms. Default is -1 (no timeout)
 * @param minObjectSize Minimum object size (0-400) to accept a reading.
 * Readings from objects smaller than this are treated as noise. Default is 0
 * (no filtering).
 */
void move_to_distance(pros::Distance &sensor, bool sensorFacesForward,
                      double destination, double maxSpeed, int timeout = -1,
                      int minObjectSize = 0);
