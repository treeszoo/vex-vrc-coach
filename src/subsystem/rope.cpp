#include "subsystem/rope.h"
#include "robot_config.h"
#include "lemlib/timer.hpp"
#include "pros/misc.hpp"
#include <algorithm>
#include <cmath>

pros::Task* globalRopeTask = nullptr;

static double target_position = ARM_MIN_LIMIT;
static bool is_pid_mode = false;

static void ropeTaskFunc(void* param) {
    RopeArgs* args = static_cast<RopeArgs*>(param);
    double target_deg = args->target_deg;
    int max_speed = args->max_speed;
    int timeout_ms = args->timeout_ms;
    delete args;

    lemlib::Timer timer(timeout_ms);
    const double error_threshold = 5.0;
    double kp = 1.5;

    timer.reset();

    while (!timer.isDone() && pros::competition::is_autonomous()) {
        double current_pos = Rope.get_position();
        double error = target_deg - current_pos;

        if (std::abs(error) < error_threshold) {
            break;
        }

        int speed = error * kp;
        speed = std::clamp(speed, -std::abs(max_speed), std::abs(max_speed));
        Rope.move(speed);

        pros::delay(20);
    }

    Rope.set_brake_mode(pros::MotorBrake::hold);
    Rope.move(10);
}

void startRopeTask(double target_deg, int max_speed, int timeout_ms) {
    RopeArgs* args = new RopeArgs{target_deg, max_speed, timeout_ms};
    pros::Task(ropeTaskFunc, static_cast<void*>(args), "Rope Task");
}

void stopRopeTask() {
    if (globalRopeTask != nullptr) {
        globalRopeTask->remove();
        delete globalRopeTask;
        globalRopeTask = nullptr;
        Rope.move(0);
    }
}

double get_protected_voltage(double target_voltage) {
    double current_angle = Rope.get_position();
    if (current_angle <= ARM_MIN_LIMIT && target_voltage < 0) return 0;
    if (current_angle >= ARM_MAX_LIMIT && target_voltage > 0) return 0;
    return target_voltage;
}

void update_rope_opcontrol(bool up_pressed, bool l1_held, bool l2_held) {
    double current_angle = Rope.get_position();
    double final_voltage = 0;

    if (up_pressed) {
        target_position = ARM_MIN_LIMIT;
        is_pid_mode = true;
    }

    if (l1_held) {
        is_pid_mode = false;
        final_voltage = 127;
    } else if (l2_held) {
        is_pid_mode = false;
        final_voltage = -100;
    } else if (is_pid_mode) {
        double error = target_position - current_angle;
        double abs_error = std::abs(error);

        final_voltage = error * ROPE_KP;

        if (error > 0) {
            final_voltage = std::min(final_voltage, 100.0);
        } else {
            double limit = (abs_error > 500) ? 120.0 : 25.0;
            final_voltage = std::clamp(final_voltage, -limit, limit);
        }

        if (abs_error < 2.0) {
            is_pid_mode = false;
            final_voltage = 0;
        }
    } else {
        final_voltage = 10;
    }

    final_voltage = get_protected_voltage(final_voltage);
    Rope.move(final_voltage);
}