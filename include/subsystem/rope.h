#pragma once
#include "pros/rtos.hpp"

// Limit constants
constexpr double ARM_MIN_LIMIT = 0.0;
constexpr double ARM_MAX_LIMIT = 2880.0;
constexpr double ROPE_KP = 1.5;

// Parameter struct for LemLib/PROS background tasks
struct RopeArgs {
    double target_deg;
    int max_speed;
    int timeout_ms;
};

// Control functions
void startRopeTask(double target_deg, int max_speed, int timeout_ms);
void stopRopeTask();
double get_protected_voltage(double target_voltage);
void update_rope_opcontrol(bool up_pressed, bool l1_held, bool l2_held);