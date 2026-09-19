#pragma once

#include "lemlib/api.hpp"
#include "pros/rtos.hpp"

/**
 * @file movement_utils.h
 * @brief Movement helpers and position tracking for autonomous routines.
 *
 * Provides common movement patterns, position tracking utilities, and
 * helper functions for autonomous navigation.
 */

// ============================================================================
// Position Tracking
// ============================================================================

/**
 * Reset chassis pose to origin and accumulate offset
 */
void reset_pose();

// ============================================================================
// Movement Helpers
// ============================================================================

/**
 * Custom shake with configurable speeds and timings
 *
 * @param count Number of shake iterations
 * @param backupSpeed Speed for backup movement
 * @param backupTimeout Duration of backup movement (ms)
 * @param forwardSpeed Speed for forward movement
 * @param forwardTimeout Duration of forward movement (ms)
 */
void shakeCustom(int count, int backupSpeed, int backupTimeout,
                 int forwardSpeed, int forwardTimeout);

/**
 * Clear blocks from loader using moveToPose
 *
 * @param x Target X coordinate
 * @param y Target Y coordinate
 * @param theta Target heading (degrees)
 * @param speed Maximum speed
 * @param moveTimeout Timeout for movement (ms)
 * @param extraTimeout Additional delay after shaking (ms)
 */
void clear_loader(float x, float y, float theta, float speed, int moveTimeout,
                  int extraTimeout = 0);

/**
 * Move to goal and score with autonomous anti-jam
 *
 * @param x Target X coordinate
 * @param y Target Y coordinate
 * @param theta Target heading (degrees)
 * @param speed Maximum speed
 * @param moveTimeout Timeout for movement (ms)
 * @param scoreTimeout Duration to run rollers for scoring (ms)
 */
void goAndScoreHighGoal(float x, float y, float theta, float speed,
                        int moveTimeout, int scoreTimeout);

