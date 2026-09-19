#include "roller_control.h"
#include "pros/rtos.hpp"
#include "robot_config.h"
#include "robot_config_data.h"

#include <cstdlib>
#include <string>

using namespace pros;

// ============================================================================
// Velocity Constants
// ============================================================================

const int HOLD_INTAKE_VELOCITY = 100;

const int INTAKE_HIGH_GOAL_VELOCITY = 600;
const int OUTTAKE_HIGH_GOAL_VELOCITY = 600;

const int INTAKE_MID_GOAL_VELOCITY = 420;
const int OUTTAKE_MID_GOAL_VELOCITY = 380;

const int INTAKE_MID_GOAL_MANUAL_VELOCITY = 300;
const int OUTTAKE_MID_GOAL_MANUAL_VELOCITY = 300;

const int OUTTAKE_IDLE_VELOCITY = 50;

// ============================================================================
// Low-Level Motor Control
// ============================================================================

void controll_rollers(int velocity_intake, int velocity_outtake) {
  intakeMotor.move_velocity(velocity_intake);
  outtakeMotor.move_velocity(velocity_outtake);
}

void intake_only() {
  controll_rollers(INTAKE_HIGH_GOAL_VELOCITY, -OUTTAKE_IDLE_VELOCITY);
}

void reverse_rollers() {
  controll_rollers(-INTAKE_HIGH_GOAL_VELOCITY, -OUTTAKE_HIGH_GOAL_VELOCITY);
}

void hold_intake() {
  controll_rollers(HOLD_INTAKE_VELOCITY, -OUTTAKE_IDLE_VELOCITY);
}

void stop_rollers() { controll_rollers(0, 0); }

// ============================================================================
// Anti-Jam System
// ============================================================================

bool detectJam(AntiJamState &state, int commandedVelocity) {
  // Don't detect jams if disabled or already handling one
  if (state.mode != AntiJamMode::NORMAL) {
    return false;
  }

  // Don't detect jam if motor isn't supposed to be moving
  if (abs(commandedVelocity) < state.config.velocityThreshold) {
    return false;
  }

  // Get actual motor velocity
  double actualVelocity = intakeMotor.get_actual_velocity();

  // Check if motor is stalled (actual velocity near zero)
  if (abs(actualVelocity) < state.config.velocityThreshold) {
    // Motor is stopped, check debounce time
    uint32_t now = pros::millis();
    if (state.debounceStartTime == 0) {
      // First detection, start debounce timer
      state.debounceStartTime = now;
      return false;
    }

    // Check if debounce time elapsed
    if ((now - state.debounceStartTime) >=
        (uint32_t)state.config.debounceTimeMs) {
      return true; // Jam confirmed
    }
  } else {
    // Motor is moving, reset debounce timer
    state.debounceStartTime = 0;
  }

  return false;
}

int updateAntiJamStateMachine(AntiJamState &state,
                              int commandedIntakeVelocity) {
  uint32_t now = pros::millis();

  // Store commanded velocity
  state.commandedIntakeVelocity = commandedIntakeVelocity;

  switch (state.mode) {
  case AntiJamMode::NORMAL: {
    // Check for jam
    if (detectJam(state, commandedIntakeVelocity)) {
      // Jam detected, transition to JAM_DETECTED
      state.mode = AntiJamMode::JAM_DETECTED;
      state.lastJamTime = now;
      // Note: debounceStartTime already set by detectJam()
    }

    // Return normal commanded velocity
    return commandedIntakeVelocity;
  }

  case AntiJamMode::JAM_DETECTED: {
    // Immediately transition to reversing
    state.mode = AntiJamMode::REVERSING;
    state.reversalStartTime = now; // Start reversal timer
    // Fall through to REVERSING case
  }
    [[fallthrough]];

  case AntiJamMode::REVERSING: {
    // Check if reversal duration elapsed
    uint32_t elapsedMs = now - state.reversalStartTime;
    if (elapsedMs >= (uint32_t)state.config.reversalDurationMs) {
      // Reversal complete, return to normal
      state.mode = AntiJamMode::NORMAL;
      state.debounceStartTime = 0; // Reset debounce timer
      state.reversalStartTime = 0; // Reset reversal timer
      return commandedIntakeVelocity;
    } else {
      // Continue reversing
      return -state.config.reversalVelocity;
    }
  }
  }

  // Fallback
  return commandedIntakeVelocity;
}

void resetAntiJamState(AntiJamState &state) {
  state.mode = AntiJamMode::NORMAL;
  state.debounceStartTime = 0;
  state.reversalStartTime = 0;
  state.lastJamTime = 0;
  state.commandedIntakeVelocity = 0;
}

// ============================================================================
// Driver Control Operations
// ============================================================================

void executeRollerOperation(AutonomousMode autonMode, RollerOperation operation,
                            AntiJamState &antiJamState) {
  switch (operation) {
  case RollerOperation::HIGH_GOAL: {
    middleGoalActuator.retract();

    // Update anti-jam and get adjusted intake velocity
    int intakeVel =
        updateAntiJamStateMachine(antiJamState, INTAKE_HIGH_GOAL_VELOCITY);
    controll_rollers(intakeVel, OUTTAKE_HIGH_GOAL_VELOCITY);
    break;
  }

  case RollerOperation::MID_GOAL: {
    if (!middleGoalActuator.isDeployed()) {
      middleGoalActuator.deploy();
      delay(100);
    }

    // Update anti-jam and get adjusted intake velocity
    int intakeVel =
        updateAntiJamStateMachine(antiJamState, INTAKE_MID_GOAL_VELOCITY);
    controll_rollers(
        autonMode == AutonomousMode::SKILLS ? 280 : intakeVel,
        autonMode == AutonomousMode::SKILLS ? 200 : OUTTAKE_MID_GOAL_VELOCITY);
    break;
  }

  case RollerOperation::INTAKE_ONLY: {
    middleGoalActuator.retract();
    intake_only(); // No anti-jam for intake only
    break;
  }

  case RollerOperation::REVERSE: {
    reverse_rollers(); // No anti-jam for manual reverse
    break;
  }

  case RollerOperation::NONE: {
    stop_rollers();
    break;
  }
  }
}

// ============================================================================
// Autonomous Operations
// ============================================================================

void controll_rollers_auton(int velocity_intake, int velocity_outtake,
                            int duration_ms, bool enable_anti_jam) {
  if (!enable_anti_jam) {
    // Simple case: no anti-jam, just set and wait
    controll_rollers(velocity_intake, velocity_outtake);
    pros::delay(duration_ms);
    return;
  }

  // Anti-jam enabled: use state machine
  AntiJamState autonState; // Starts in NORMAL mode

  uint32_t startTime = pros::millis();
  uint32_t endTime = startTime + duration_ms;

  const int UPDATE_INTERVAL_MS = 10; // Match driver control loop rate

  while (pros::millis() < endTime) {
    // Update anti-jam state machine and get adjusted intake velocity
    int actualIntakeVel =
        updateAntiJamStateMachine(autonState, velocity_intake);

    // Send commands to motors (outtake always runs at commanded velocity)
    controll_rollers(actualIntakeVel, velocity_outtake);

    // Wait before next update
    pros::delay(UPDATE_INTERVAL_MS);
  }
}
