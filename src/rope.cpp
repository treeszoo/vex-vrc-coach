#include "roller_control.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"

#include <cstdlib>
#include <string>

using namespace pros;

void ropeTaskFunc(void* param) {
    // 1. 解析并复制传入的参数
    RopeArgs* args = static_cast<RopeArgs*>(param);
    double target_deg = args->target_deg;
    int max_speed = args->max_speed;
    int timeout_ms = args->timeout_ms;
    
    // 2. 释放动态分配的结构体内存，防止内存泄漏
    delete args; 

    // 3. 准备控制变量
    lemlib::Timer timer(timeout_ms);    // 初始化 LemLib 计时器
    
    const double error_threshold = 5.0; // 允许的角度误差范围（度）
    double kp = 1.5;                    // 简易比例控制系数（可根据实际抖动情况微调）

    timer.reset();                      // 计时器清零

    // 4. 核心控制循环
    // 条件：未超时 且 机器人仍处于自动阶段（如果场控提前切入遥控，循环立刻终止）
    while (!timer.isDone() && pros::competition::is_autonomous()) {
        // 获取当前马达组的平均位置
        double current_pos = Rope.get_position();
        
        // 计算目标与当前的误差
        double error = target_deg - current_pos;
        
        // 如果进入误差允许范围，提前退出循环
        if (std::abs(error) < error_threshold) {
            break;
        }
        
        // 计算 P 控制输出速度
        int speed = error * kp;
        
        // 限制输出速度在 [-max_speed, max_speed] 之间
        speed = std::clamp(speed, -std::abs(max_speed), std::abs(max_speed));
        
        // 驱动马达
        Rope.move(speed);
        
        // 延时 20 毫秒，保证 PROS 任务调度正常，防止卡死 CPU
        pros::delay(20);
    }
    
    // 5. 动作结束或超时后的安全处理
    // 将刹车模式设为 HOLD（锁死状态），防止重力导致马达下滑
    Rope.set_brake_mode(pros::MotorBrake::hold);
    Rope.move(10); 
}


// ==========================================
// 3. 任务控制封装接口（供自治程序调用）
// ==========================================

/**
 * @brief 异步启动 Rope 控制任务（非阻塞，底盘可同时移动）
 */
void startRopeTask(double target_deg, int max_speed, int timeout_ms) {
    // 不再使用全局指针去 remove，直接每次 new 一个独立任务
    // PROS 会自动调度它们，由于我们在任务内部加了限制，它们不会冲突
    RopeArgs* args = new RopeArgs{target_deg, max_speed, timeout_ms};
    pros::Task(ropeTaskFunc, static_cast<void*>(args), "Rope Task");
}
/**
 * @brief 强行停止 Rope 后台任务
 */
void stopRopeTask() {
    if (globalRopeTask != nullptr) {
        globalRopeTask->remove(); // 销毁线程
        delete globalRopeTask;    // 释放内存
        globalRopeTask = nullptr; // 指针置空
                // 确保马达断电停止
        Rope.move(0);      
    }
}