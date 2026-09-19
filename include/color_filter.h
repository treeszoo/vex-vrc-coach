#pragma once

#include "robot_config_data.h"

/**
 * @file color_filter.h
 * @brief Color sensor operations for block detection and filtering.
 *
 * Provides functions to detect block colors and filter out opponent blocks
 * during autonomous loader pickup.
 */

// Color detection bounds
#define RED_UPPER_BOUND 30
#define RED_LOWER_BOUND 0

#define BLUE_UPPER_BOUND 230
#define BLUE_LOWER_BOUND 170

/**
 * Check if the optical sensor detects a red block
 * @return true if red block detected
 */
bool is_red();

/**
 * Check if the optical sensor detects a blue block
 * @return true if blue block detected
 */
bool is_blue();

/**
 * Check if we should reject the currently detected block
 *
 * @param ourTeam Our team color
 * @return true if the detected block is the opponent's color
 */
bool should_reject_block(TeamColor ourTeam);

/**
 * Delay that monitors optical sensor and exits early if opponent block detected
 *
 * @param ms Delay duration in milliseconds
 * @param config Autonomous configuration (nullptr to disable color filtering)
 * @return true if interrupted by opponent block (should exit early)
 */
bool colorFilteredDelay(int ms, const AutonConfig *config);

/**
 * shakeCustom with color filtering - exits early if opponent block detected
 *
 * @param count Number of shake iterations
 * @param backupSpeed Speed for backup movement
 * @param backupTimeout Duration of backup movement (ms)
 * @param forwardSpeed Speed for forward movement
 * @param forwardTimeout Duration of forward movement (ms)
 * @param config Autonomous configuration
 * @return true if interrupted by opponent block
 */
bool shakeCustomWithColorFilter(int count, int backupSpeed, int backupTimeout,
                                int forwardSpeed, int forwardTimeout,
                                const AutonConfig *config);
