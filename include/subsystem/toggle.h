#pragma once
#include "pros/rtos.hpp"

constexpr int CURRENT_LIMIT_MA = 2000;
constexpr int ROLLER_POWER = 127;
constexpr double DOWN_STEP_ANGLE = 180.0 * 2.5;
constexpr double Y_STEP_ANGLE = 90.0 * 2.5;
constexpr uint32_t STALL_TIME_MS = 100;
constexpr double TOLERANCE_DEG = 3.0;

void toggle_task_fn(void* param);