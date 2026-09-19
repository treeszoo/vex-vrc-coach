#pragma once

#include "robot_config_data.h"

/**
 * @file auton_common.h
 * @brief Shared autonomous helpers used by both alliance and skills routines.
 *
 * Contains common initialization, alignment, and pickup functions that are
 * reused across different autonomous modes.
 */

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize autonomous mode
 * Sets up optical sensor, brake mode, and deploys tracking wheel
 *
 * @param config Autonomous configuration
 */
void init_auton(const AutonConfig &config);

// ============================================================================
// Goal Alignment
// ============================================================================

/**
 * Fast alignment to long goal (single turn, quick backup)
 * Used in aggressive skills
 */
void alignToLongGoal_fast();

/**
 * Slow alignment to long goal (double alignment)
 * Used in conservative skills
 */
void alignToLongGoal_slow();

// ============================================================================
// Descoring
// ============================================================================

/**
 * Perform descoring maneuver
 *
 * @param quickMode If true, use faster timing (for time-constrained routines)
 */
void performDescore(bool quickMode = false);

// ============================================================================
// Block Pickup
// ============================================================================

/**
 * Pick up blocks from loader with optional color filtering
 *
 * @param x Target X coordinate
 * @param y Target Y coordinate
 * @param theta Target heading (degrees)
 * @param speed Maximum approach speed
 * @param moveTimeout Timeout for approach movement (ms)
 * @param pickupTime Total time for pickup operation (ms)
 * @param config Autonomous configuration (nullptr disables color filtering)
 */
void pick_blocks_from_loader(float x, float y, float theta, float speed,
                             int moveTimeout, int pickupTime,
                             const AutonConfig *config = nullptr);

// ============================================================================
// Movement Helpers
// ============================================================================

/**
 * Swing forward with shaking motion (for loader clearing)
 *
 * @param moveDuration Duration of initial forward movement (ms)
 * @param swingDuration Duration of shaking motion (ms)
 * @param speed Speed for movements (default: 127)
 */
void swingForward(int moveDuration, int swingDuration, int speed = 127);

/**
 * Push last block in for bonus points
 * Quick back-and-forth motion at goal
 */
void pushLastBlockInForBonusPoints();
