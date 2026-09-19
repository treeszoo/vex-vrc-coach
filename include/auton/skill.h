#pragma once

#include "robot_config_data.h"

/**
 * @file auton_skills.h
 * @brief Skills autonomous routine entry point.
 *
 * The skills routine handles 4 loaders with scoring phases:
 * 1. Loaders 1 & 2 (left side)
 * 2. Middle goal + Loader 3
 * 3. Score Loader 3 (right side)
 * 4. Loader 4 + Park
 */

/**
 * Main skills autonomous routine
 *
 * @param config Autonomous configuration
 */
void skill_safe(const AutonConfig &config);

void skill_aggr(const AutonConfig &config);
