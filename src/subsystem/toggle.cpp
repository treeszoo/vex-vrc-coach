#include "subsystem/toggle.h"
#include "robot_config.h"
#include <cmath>

void toggle_task_fn(void* param) {
    Lefttoggle.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
    Righttoggle.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);

    int left_step_count = 0;
    int right_step_count = 0;

    double left_target = Lefttoggle.get_position();
    double right_target = Righttoggle.get_position();

    bool target_is_90 = true;
    uint32_t left_stall_start = 0;
    uint32_t right_stall_start = 0;
    bool left_stalled = false;
    bool right_stalled = false;

    double prev_left_target = -99999.0;
    double prev_right_target = -99999.0;

    while (true) {
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_DOWN)) {
            left_step_count++;
            right_step_count++;
            left_target = left_step_count * DOWN_STEP_ANGLE;
            right_target = right_step_count * DOWN_STEP_ANGLE;
            left_stalled = right_stalled = false;
            left_stall_start = right_stall_start = 0;
        } else if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_Y)) {
            double delta_angle = target_is_90 ? Y_STEP_ANGLE : -Y_STEP_ANGLE;
            left_target = Lefttoggle.get_position() + delta_angle;
            right_target = Righttoggle.get_position() + delta_angle;
            target_is_90 = !target_is_90;

            left_step_count = static_cast<int>(left_target / DOWN_STEP_ANGLE);
            right_step_count = static_cast<int>(right_target / DOWN_STEP_ANGLE);

            left_stalled = right_stalled = false;
            left_stall_start = right_stall_start = 0;
        }

        uint32_t now = pros::millis();

        // Left motor stall check
        if (!left_stalled) {
            bool left_near = (std::abs(Lefttoggle.get_position() - left_target) < 5.0);
            int32_t left_current = Lefttoggle.get_current_draw();

            if (!left_near && std::abs(left_current) >= CURRENT_LIMIT_MA) {
                if (left_stall_start == 0) left_stall_start = now;
                else if (now - left_stall_start >= STALL_TIME_MS) left_stalled = true;
            } else {
                left_stall_start = 0;
            }
        }

        // Right motor stall check
        if (!right_stalled) {
            bool right_near = (std::abs(Righttoggle.get_position() - right_target) < 5.0);
            int32_t right_current = Righttoggle.get_current_draw();

            if (!right_near && std::abs(right_current) >= CURRENT_LIMIT_MA) {
                if (right_stall_start == 0) right_stall_start = now;
                else if (now - right_stall_start >= STALL_TIME_MS) right_stalled = true;
            } else {
                right_stall_start = 0;
            }
        }

        // Left motor movement
        if (left_stalled) {
            Lefttoggle.move(0);
        } else {
            double left_error = std::abs(Lefttoggle.get_position() - left_target);
            if (left_error <= TOLERANCE_DEG) {
                Lefttoggle.move(0);
                prev_left_target = -99999.0;
            } else if (left_target != prev_left_target) {
                Lefttoggle.move_absolute(left_target, ROLLER_POWER);
                prev_left_target = left_target;
            }
        }

        // Right motor movement
        if (right_stalled) {
            Righttoggle.move(0);
        } else {
            double right_error = std::abs(Righttoggle.get_position() - right_target);
            if (right_error <= TOLERANCE_DEG) {
                Righttoggle.move(0);
                prev_right_target = -99999.0;
            } else if (right_target != prev_right_target) {
                Righttoggle.move_absolute(right_target, ROLLER_POWER);
                prev_right_target = right_target;
            }
        }

        pros::delay(20);
    }
}