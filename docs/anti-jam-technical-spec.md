# Anti-Jam System Technical Specification

**Version:** 1.0  
**Date:** January 22, 2026  
**Author:** AI Assistant  
**Status:** Design Proposal

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Problem Statement](#problem-statement)
3. [Current Implementation Analysis](#current-implementation-analysis)
4. [System Requirements](#system-requirements)
5. [Proposed Architecture](#proposed-architecture)
6. [Implementation Design](#implementation-design)
7. [Integration Points](#integration-points)
8. [Testing Strategy](#testing-strategy)
9. [Risk Analysis](#risk-analysis)
10. [Open Questions](#open-questions)

---

## 1. Executive Summary

The anti-jam system prevents intake motor stalls during outtake operations by detecting when the intake motor stops spinning (jams) and automatically executing a brief reversal cycle. Due to different execution patterns in driver control (event-loop based) vs. autonomous (linear execution), two distinct implementation strategies are required.

**Key Design Goals:**
- Detect intake motor jams during outtake operations
- Execute brief reversal cycles without disrupting outtake motor
- Provide seamless operation in both driver control and autonomous modes
- Minimize impact on existing codebase

---

## 2. Problem Statement

### 2.1 Scenario
When outtaking blocks from the robot, two motors operate simultaneously:
- **Intake Motor (Port 10):** Pushes blocks forward through the intake path
- **Outtake Motor (Port 17):** Ejects blocks from the robot

### 2.2 Jam Definition
A jam occurs when:
1. The robot is in an outtake operation (both motors running at specific velocities)
2. The intake motor **stops spinning** (velocity ≈ 0) despite being commanded to move
3. This indicates a mechanical blockage in the intake path

### 2.3 Required Response
Upon detecting a jam:
1. **Reverse** the intake motor briefly (short duration, e.g., 150-300ms)
2. **Continue** normal outtake motor operation (uninterrupted)
3. **Resume** normal intake motor operation after reversal
4. **Repeat** if jam persists

### 2.4 Mode-Specific Challenges

#### Driver Control Mode (Op Control)
- **Execution Pattern:** Infinite loop with 10ms delay cycles
- **State Management:** Must track jam state across loop iterations
- **User Input:** Must remain responsive to controller input changes
- **Timing:** Cannot use blocking delays (would freeze controls)

#### Autonomous Mode
- **Execution Pattern:** Linear, sequential code execution
- **Timing:** Can use blocking delays safely
- **Duration:** Operations have known time windows
- **Integration:** Must work with existing autonomous functions

---

## 3. Current Implementation Analysis

### 3.1 Existing Code (robot_utils.cpp)

```cpp
bool is_intake_jammed() { 
    return (intakeMotor.get_current_draw() >= 2200); 
}

void controll_rollers(int velocity_intake, int velocity_outtake, bool anti_jam) {
    bool shouldReverse = anti_jam && is_intake_jammed();
    outtakeMotor.move_velocity(velocity_outtake);
    intakeMotor.move_velocity(shouldReverse ? -velocity_intake / 10 : velocity_intake);
}
```

### 3.2 Current Problems

| Issue                  | Description                                            | Impact                                                |
| ---------------------- | ------------------------------------------------------ | ----------------------------------------------------- |
| **Detection Method**   | Uses current draw (≥2200mA) instead of actual velocity | False positives; high current doesn't always mean jam |
| **No Time Component**  | Reversal happens for only one loop iteration (~10ms)   | Ineffective; jam not cleared                          |
| **No State Machine**   | No tracking of reversal state or duration              | Cannot maintain reversal for adequate time            |
| **Immediate Response** | Reverses on first detection without debouncing         | Unstable; noise causes spurious reversals             |
| **Mode Agnostic**      | Same logic for both op control and autonomous          | Doesn't account for different execution patterns      |

### 3.3 Why It Doesn't Work

1. **Current draw is not reliable:** High current can occur during normal operation (acceleration, high load)
2. **Reversal too brief:** 10ms reversal (1 loop cycle) is insufficient to clear a jam
3. **No sustained reversal:** Cannot maintain reversal state across multiple loop iterations
4. **No autonomous support:** Linear autonomous code needs different approach

---

## 4. System Requirements

### 4.1 Functional Requirements

| ID   | Requirement                                                            | Priority |
| ---- | ---------------------------------------------------------------------- | -------- |
| FR-1 | Detect when intake motor velocity is near zero during commanded motion | MUST     |
| FR-2 | Execute intake motor reversal for configurable duration (150-300ms)    | MUST     |
| FR-3 | Maintain outtake motor operation during reversal                       | MUST     |
| FR-4 | Support non-blocking operation in driver control mode                  | MUST     |
| FR-5 | Support blocking operation in autonomous mode                          | MUST     |
| FR-6 | Provide debouncing to prevent false positives                          | SHOULD   |
| FR-7 | Allow configuration of jam detection thresholds                        | SHOULD   |
| FR-8 | Provide telemetry/logging for jam events                               | COULD    |

### 4.2 Non-Functional Requirements

| ID    | Requirement                     | Target                                                |
| ----- | ------------------------------- | ----------------------------------------------------- |
| NFR-1 | Jam detection response time     | < 100ms                                               |
| NFR-2 | Loop overhead in driver control | < 2ms per cycle                                       |
| NFR-3 | Code maintainability            | High (clear state machine, documented)                |
| NFR-4 | Backward compatibility          | Existing function signatures preserved where possible |

### 4.3 Constraints

- PROS API limitations for motor velocity sensing
- 10ms loop cycle time in driver control
- Cannot block in driver control mode
- Must integrate with existing `controll_rollers()` function
- Motor velocity update rate (~10-20ms)

---

## 5. Proposed Architecture

### 5.1 High-Level Design

```
┌─────────────────────────────────────────────────────────────┐
│                    Anti-Jam System                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────┐         ┌──────────────────┐        │
│  │  Jam Detector    │────────▶│  State Machine   │        │
│  │                  │         │                  │        │
│  │  - Velocity      │         │  - NORMAL        │        │
│  │  - Threshold     │         │  - JAM_DETECTED  │        │
│  │  - Debouncing    │         │  - REVERSING     │        │
│  └──────────────────┘         └──────────────────┘        │
│           │                            │                   │
│           │                            ▼                   │
│           │                   ┌──────────────────┐        │
│           └──────────────────▶│  Motor Control   │        │
│                               │                  │        │
│                               │  - Intake Motor  │        │
│                               │  - Outtake Motor │        │
│                               └──────────────────┘        │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                    Mode-Specific Wrappers                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────────┐     ┌──────────────────────┐    │
│  │  Op Control Wrapper  │     │  Autonomous Wrapper  │    │
│  │                      │     │                      │    │
│  │  - Non-blocking      │     │  - Blocking          │    │
│  │  - State persistence │     │  - Time-boxed        │    │
│  │  - Per-loop update   │     │  - Inline delays     │    │
│  └──────────────────────┘     └──────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 State Machine

```
                     ┌──────────────┐
          ┌──────────│    NORMAL    │◀─────────────┐
          │          │              │              │
          │          └──────────────┘              │
          │                  │                     │
          │                  │ Velocity ≈ 0       │
          │                  │ for > debounce     │
          │                  │ time               │
          │                  ▼                     │
          │          ┌──────────────┐              │
          │          │              │              │
          │          │JAM_DETECTED  │              │
          │          │              │              │
          │          └──────────────┘              │
          │                  │                     │
          │                  │ Start reversal     │
          │                  │                     │
          │                  ▼                     │
          │          ┌──────────────┐              │
          │          │              │              │
          │          │  REVERSING   │──────────────┘
          │          │              │  Reversal time
          │          └──────────────┘  elapsed
          │                  
          │ Operation stopped or command changed
          │ (resetAntiJamState() called)
          │
          └─────────▶ Return to NORMAL

Notes:
- State starts in NORMAL mode (default)
- State resets to NORMAL when operation stops (button released)
- State resets to NORMAL when switching operations (L1 → L2, etc.)
- No IDLE or DISABLED state - always ready to handle jams
- Simple three-state machine: NORMAL → JAM_DETECTED → REVERSING → NORMAL
```

### 5.3 Data Flow

**Driver Control Mode:**
```
Controller Input → Loop Iteration (10ms) → Jam Detector → State Machine 
                                                ↓
                      Motor Commands ← Motor Control Logic ← State Machine
```

**Autonomous Mode:**
```
Autonomous Function → controll_rollers_auton(duration) → {
    Loop until duration:
        Jam Detector → State Machine → Motor Control
        delay(10ms)
}
```

---

## 6. Implementation Design

### 6.1 Core Components

#### 6.1.1 AntiJamState Structure

```cpp
/**
 * Anti-jam state machine states
 */
enum class AntiJamMode {
    NORMAL,       // Normal operation, monitoring for jams
    JAM_DETECTED, // Jam detected, preparing to reverse
    REVERSING     // Currently reversing intake motor
};

/**
 * Anti-jam configuration parameters
 */
struct AntiJamConfig {
    int velocityThreshold = 10;        // RPM below which motor considered jammed
    int debounceTimeMs = 50;           // Time motor must be stopped before jam detected
    int reversalDurationMs = 200;      // How long to reverse motor
    int reversalVelocity = 200;        // Velocity for reversal (positive = backward)
    int cooldownTimeMs = 100;          // Time after reversal before detecting next jam
};

/**
 * Anti-jam runtime state
 */
struct AntiJamState {
    AntiJamMode mode = AntiJamMode::NORMAL;
    uint32_t stateStartTime = 0;      // When current state began (pros::millis())
    uint32_t lastJamTime = 0;         // When last jam was detected
    int commandedIntakeVelocity = 0;  // What intake velocity we're trying to achieve
    AntiJamConfig config;              // Configuration parameters
};
```

#### 6.1.2 Jam Detection Function

```cpp
/**
 * Check if intake motor is jammed
 * 
 * @param state Anti-jam state structure
 * @param commandedVelocity Velocity we commanded the motor to move at
 * @return true if jam detected, false otherwise
 */
bool detectJam(AntiJamState& state, int commandedVelocity) {
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
        if (state.stateStartTime == 0) {
            // First detection, start debounce timer
            state.stateStartTime = now;
            return false;
        }
        
        // Check if debounce time elapsed
        if ((now - state.stateStartTime) >= state.config.debounceTimeMs) {
            return true; // Jam confirmed
        }
    } else {
        // Motor is moving, reset debounce timer
        state.stateStartTime = 0;
    }
    
    return false;
}
```

#### 6.1.3 State Machine Update Function

```cpp
/**
 * Update anti-jam state machine and get adjusted intake motor velocity
 * 
 * @param state Anti-jam state structure
 * @param commandedIntakeVelocity Velocity we want the intake motor at
 * @return Actual velocity to command to intake motor (may be reversed if jam detected)
 */
int updateAntiJamStateMachine(AntiJamState& state, int commandedIntakeVelocity) {
    uint32_t now = pros::millis();
    
    // Store commanded velocity
    state.commandedIntakeVelocity = commandedIntakeVelocity;
    
    switch (state.mode) {
        case AntiJamMode::NORMAL: {
            // Check for jam
            if (detectJam(state, commandedIntakeVelocity)) {
                // Jam detected, transition to JAM_DETECTED
                state.mode = AntiJamMode::JAM_DETECTED;
                state.stateStartTime = now;
                state.lastJamTime = now;
            }
            
            // Return normal commanded velocity
            return commandedIntakeVelocity;
        }
        
        case AntiJamMode::JAM_DETECTED: {
            // Immediately transition to reversing
            state.mode = AntiJamMode::REVERSING;
            state.stateStartTime = now;
            // Fall through to REVERSING case
        }
        
        case AntiJamMode::REVERSING: {
            // Check if reversal duration elapsed
            uint32_t elapsedMs = now - state.stateStartTime;
            if (elapsedMs >= state.config.reversalDurationMs) {
                // Reversal complete, return to normal
                state.mode = AntiJamMode::NORMAL;
                state.stateStartTime = 0;
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
```

#### 6.1.4 Reset Function

```cpp
/**
 * Reset anti-jam state machine
 * Useful when transitioning between different operations
 */
void resetAntiJamState(AntiJamState& state) {
    state.mode = AntiJamMode::NORMAL;
    state.stateStartTime = 0;
    state.consecutiveJams = 0;
    state.commandedVelocity = 0;
}
```

### 6.2 Driver Control Integration

#### 6.2.1 Simplified controll_rollers Function

**Key Change:** Remove `anti_jam` parameter - this is now a low-level motor command function only.

```cpp
/**
 * Control intake and outtake rollers (low-level motor commands)
 * 
 * @param velocity_intake Desired intake velocity
 * @param velocity_outtake Desired outtake velocity
 */
void controll_rollers(int velocity_intake, int velocity_outtake) {
    intakeMotor.move_velocity(velocity_intake);
    outtakeMotor.move_velocity(velocity_outtake);
}
```

#### 6.2.2 High-Level Helper Functions (Updated)

These functions remain for convenience but now just call the simplified `controll_rollers()`:

```cpp
void score_high_goal() {
    controll_rollers(INTAKE_HIGH_GOAL_VELOCITY, OUTTAKE_HIGH_GOAL_VELOCITY);
}

void score_mid_goal() {
    controll_rollers(INTAKE_MID_GOAL_VELOCITY, OUTTAKE_MID_GOAL_VELOCITY);
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

void stop_rollers() {
    controll_rollers(0, 0);
}
```

#### 6.2.3 Roller Operation Helper Function

**Extract roller control logic into separate function for maintainability:**

```cpp
/**
 * Roller operation types
 */
enum class RollerOperation {
    NONE,
    HIGH_GOAL,
    MID_GOAL,
    INTAKE_ONLY,
    REVERSE
};

/**
 * Execute roller operation with anti-jam support for scoring operations
 * 
 * @param operation Type of roller operation to execute
 * @param antiJamState Anti-jam state machine (modified if anti-jam active)
 */
void executeRollerOperation(RollerOperation operation, AntiJamState& antiJamState) {
    switch (operation) {
        case RollerOperation::HIGH_GOAL: {
            middleGoalActuator.retract();
            
            // Update anti-jam and get adjusted intake velocity
            int intakeVel = updateAntiJamStateMachine(antiJamState, INTAKE_HIGH_GOAL_VELOCITY);
            controll_rollers(intakeVel, OUTTAKE_HIGH_GOAL_VELOCITY);
            break;
        }
        
        case RollerOperation::MID_GOAL: {
            middleGoalActuator.deploy();
            
            // Update anti-jam and get adjusted intake velocity
            int intakeVel = updateAntiJamStateMachine(antiJamState, INTAKE_MID_GOAL_VELOCITY);
            controll_rollers(intakeVel, OUTTAKE_MID_GOAL_VELOCITY);
            break;
        }
        
        case RollerOperation::INTAKE_ONLY: {
            middleGoalActuator.retract();
            intake_only();  // No anti-jam for intake only
            break;
        }
        
        case RollerOperation::REVERSE: {
            reverse_rollers();  // No anti-jam for manual reverse
            break;
        }
        
        case RollerOperation::NONE: {
            stop_rollers();
            break;
        }
    }
}
```

#### 6.2.4 Op Control Loop with Anti-Jam State Machine

**Major Change:** Implement anti-jam handling with modular roller operation function.

```cpp
void opcontrol() {
    chassis.setBrakeMode(MOTOR_BRAKE_COAST);
    if (autonConfig.useColorSensor) {
        optical.set_led_pwm(100);
    }

    ButtonTracker buttonA, buttonLeft, buttonWheelup;
    bool isWheelupDeployed = false;
    
    // Anti-jam state machine for driver control
    AntiJamState antiJamState;
    
    // Track roller operations to detect changes
    RollerOperation currentRollerOp = RollerOperation::NONE;
    RollerOperation previousRollerOp = RollerOperation::NONE;

    while (true) {
        // Deploy wheelup only after user provides any controller input
        if (!isWheelupDeployed && hasAnyControllerInput()) {
            wheelupActuator.deploy();
            isWheelupDeployed = true;
        }

        // Drive control
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
        chassis.arcade(leftY, rightX * 0.85);

        // Determine current roller operation based on button state
        if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1)) {
            currentRollerOp = RollerOperation::HIGH_GOAL;
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)) {
            currentRollerOp = RollerOperation::MID_GOAL;
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R1)) {
            currentRollerOp = RollerOperation::INTAKE_ONLY;
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)) {
            currentRollerOp = RollerOperation::REVERSE;
        } else {
            currentRollerOp = RollerOperation::NONE;
        }

        // Detect operation changes and reset anti-jam state
        if (currentRollerOp != previousRollerOp) {
            resetAntiJamState(antiJamState);
            previousRollerOp = currentRollerOp;
        }

        // Execute roller operation with anti-jam
        executeRollerOperation(currentRollerOp, antiJamState);

        // Pneumatic control (unchanged)
        handleButtonToggle(E_CONTROLLER_DIGITAL_B, buttonA,
                          []() { loaderActuator.toggle(); });
        handleButtonToggle(E_CONTROLLER_DIGITAL_DOWN, buttonLeft,
                          []() { descorerActuator.toggle(); });
        handleMultiButtonToggle(
            {pros::E_CONTROLLER_DIGITAL_X, pros::E_CONTROLLER_DIGITAL_UP},
            buttonWheelup, []() {
                wheelupActuator.toggle();
                chassis.calibrate();
                chassis.setPose(0, 0, 0);
            });

        delay(10);
    }
}
```

### 6.3 Autonomous Integration

#### 6.3.1 Time-Boxed Autonomous Function

```cpp
/**
 * Control rollers for a specific duration in autonomous mode
 * Includes anti-jam support with blocking implementation
 * 
 * @param velocity_intake Desired intake velocity
 * @param velocity_outtake Desired outtake velocity
 * @param duration_ms How long to run (milliseconds)
 * @param enable_anti_jam Enable anti-jam system (default: true)
 */
void controll_rollers_auton(int velocity_intake, int velocity_outtake, 
                            int duration_ms, bool enable_anti_jam = true) {
    if (!enable_anti_jam) {
        // Simple case: no anti-jam, just set and wait
        controll_rollers(velocity_intake, velocity_outtake);
        pros::delay(duration_ms);
        return;
    }
    
    // Anti-jam enabled: use state machine
    AntiJamState autonState;  // Starts in NORMAL mode
    
    uint32_t startTime = pros::millis();
    uint32_t endTime = startTime + duration_ms;
    
    const int UPDATE_INTERVAL_MS = 10; // Match driver control loop rate
    
    while (pros::millis() < endTime) {
        // Update anti-jam state machine and get adjusted intake velocity
        int actualIntakeVel = updateAntiJamStateMachine(autonState, velocity_intake);
        
        // Send commands to motors (outtake always runs at commanded velocity)
        controll_rollers(actualIntakeVel, velocity_outtake);
        
        // Wait before next update
        pros::delay(UPDATE_INTERVAL_MS);
    }
}
```

#### 6.3.2 Async Task-Based Autonomous Function

For use with existing `runAsync()` helper:

```cpp
/**
 * Start roller control as async task with anti-jam
 * Returns Task object that can be managed by caller
 * 
 * @param velocity_intake Desired intake velocity
 * @param velocity_outtake Desired outtake velocity
 * @param duration_ms How long to run (milliseconds)
 * @param anti_jam Enable anti-jam system
 * @return Task object
 */
pros::Task startRollersAsync(int velocity_intake, int velocity_outtake, 
                             int duration_ms, bool anti_jam = true) {
    return pros::Task([=]() {
        controll_rollers_auton(velocity_intake, velocity_outtake, duration_ms, anti_jam);
    });
}
```

#### 6.3.3 Example Autonomous Usage

**Before (current code):**
```cpp
void goAndScoreHighGoal(float x, float y, float theta, float speed,
                        int moveTimeout, int scoreTimeout) {
    chassis.moveToPose(x, y, theta, moveTimeout, {...}, false);
    score_high_goal(false);  // Anti-jam disabled
    delay(scoreTimeout);
}
```

**After (with anti-jam):**
```cpp
void goAndScoreHighGoal(float x, float y, float theta, float speed,
                        int moveTimeout, int scoreTimeout) {
    chassis.moveToPose(x, y, theta, moveTimeout, {...}, false);
    // Use new autonomous anti-jam function
    controll_rollers_auton(INTAKE_HIGH_GOAL_VELOCITY, 
                          OUTTAKE_HIGH_GOAL_VELOCITY, 
                          scoreTimeout, 
                          true);  // Anti-jam enabled
}
```

---

## 7. Integration Points

### 7.1 Files to Modify

| File                     | Changes Required                                                                            | Complexity  |
| ------------------------ | ------------------------------------------------------------------------------------------- | ----------- |
| `include/robot_utils.h`  | Add new structs, function declarations; remove `anti_jam` parameter from existing functions | Low         |
| `src/robot_utils.cpp`    | Implement anti-jam logic; simplify `controll_rollers()` to remove `anti_jam` parameter      | Medium      |
| `src/competition.cpp`    | Refactor `opcontrol()` to include anti-jam state machine and operation tracking             | Medium-High |
| `src/auton_alliance.cpp` | Replace blocking roller calls with `controll_rollers_auton()`                               | Medium      |
| `src/auton_skills.cpp`   | Replace blocking roller calls with `controll_rollers_auton()`                               | Medium      |

### 7.2 Breaking Changes and Migration

**Breaking Changes:**
- `controll_rollers()` signature changed: `anti_jam` parameter removed
- All helper functions (`score_high_goal()`, etc.) no longer take `anti_jam` parameter
- Op control loop requires significant refactoring to implement state machine

**Migration Requirements:**

**Step 1 - Update Low-Level Functions:**
```cpp
// OLD
void controll_rollers(int velocity_intake, int velocity_outtake, bool anti_jam);
score_high_goal(true);  // With anti-jam

// NEW  
void controll_rollers(int velocity_intake, int velocity_outtake);
score_high_goal();  // Anti-jam handled in op control loop
```

**Step 2 - Refactor Op Control:**
- Add anti-jam state machine to `opcontrol()` function
- Add operation tracking (`CurrentOperation` enum)
- Add state reset logic when operations change

**Step 3 - Migrate Autonomous:**
- Replace direct `score_high_goal()` calls in autonomous with `controll_rollers_auton()`
- Add duration parameters to autonomous scoring operations

**Migration Path:**
1. Implement core anti-jam system (structs, state machine functions)
2. Simplify `controll_rollers()` and helper functions
3. Refactor `opcontrol()` with state machine
4. Test driver control mode thoroughly
5. Migrate autonomous functions to use `controll_rollers_auton()`
6. Test autonomous mode
7. Tune parameters based on real-world testing

### 7.3 Configuration and Tuning

Parameters that may need tuning:

```cpp
AntiJamConfig defaultConfig = {
    .velocityThreshold = 10,       // Tune: what velocity counts as "stopped"?
    .debounceTimeMs = 50,          // Tune: how long to confirm jam?
    .reversalDurationMs = 200,     // Tune: how long to reverse?
    .reversalVelocity = 200,       // Tune: how fast to reverse?
    .cooldownTimeMs = 100,         // Tune: delay before detecting next jam?
    .maxConsecutiveJams = 3        // Safety: disable after this many jams
};
```

**Tuning Process:**
1. Start with conservative values (longer debounce, shorter reversal)
2. Test on actual robot with real blocks
3. Adjust based on observed behavior
4. Add telemetry to track jam frequency and effectiveness

---

## 8. Testing Strategy

### 8.1 Unit Testing (Simulated)

| Test Case           | Description                     | Expected Behavior                    |
| ------------------- | ------------------------------- | ------------------------------------ |
| Normal Operation    | Run rollers without jam         | No reversals, smooth operation       |
| Single Jam          | Block intake manually           | Detect jam, reverse, resume          |
| Repeated Jams       | Block intake multiple times     | Handle each jam independently        |
| Max Jams Exceeded   | Jam more than threshold         | Disable anti-jam, stop motor         |
| No Commanded Motion | Anti-jam enabled but velocity=0 | No false jam detections              |
| High Load           | Normal operation with blocks    | No false positives from high current |

### 8.2 Integration Testing (On Robot)

**Driver Control Tests:**
1. Score high goal normally (L1 button)
2. Manually block intake during scoring
3. Verify reversal occurs and outtake continues
4. Verify resume after reversal
5. Test rapid button presses (mode changes)

**Autonomous Tests:**
1. Run normal autonomous routine
2. Add test path that intentionally causes jam
3. Verify anti-jam activates during autonomous
4. Verify timing remains consistent
5. Test multiple autonomous modes

### 8.3 Performance Testing

**Metrics to Collect:**
- Jam detection latency (target: <100ms)
- False positive rate (target: <5%)
- False negative rate (target: <1%)
- Loop overhead in driver control (target: <2ms)
- Successful jam clears (target: >90%)

### 8.4 Edge Cases

1. **Rapid mode switches:** L1 → R1 → L1 quickly
2. **Jam during reversal:** Block occurs while already reversing
3. **Controller disconnect:** What happens if controller lost during jam?
4. **Motor disconnect:** What if intake motor disconnected?
5. **Timeout during autonomous:** Match ends during reversal

---

## 9. Risk Analysis

### 9.1 Technical Risks

| Risk                              | Likelihood | Impact | Mitigation                                     |
| --------------------------------- | ---------- | ------ | ---------------------------------------------- |
| False positives in driver control | Medium     | Medium | Debouncing, velocity threshold tuning          |
| Reversal too short to clear jam   | Medium     | High   | Tunable duration, test extensively             |
| State machine bugs                | Low        | High   | Thorough code review, state diagram validation |
| Performance overhead              | Low        | Low    | Profiling, optimize if needed                  |
| Motor API limitations             | Medium     | Medium | Test motor velocity reading accuracy           |

### 9.2 Operational Risks

| Risk                                           | Likelihood | Impact   | Mitigation                                        |
| ---------------------------------------------- | ---------- | -------- | ------------------------------------------------- |
| Jam detection interferes with normal operation | Low        | High     | Conservative thresholds, extensive testing        |
| Reversal damages mechanism                     | Low        | Critical | Limit reversal velocity, test mechanical limits   |
| Anti-jam disables during competition           | Low        | Medium   | Tune max consecutive jams appropriately           |
| Difficult to debug in competition              | Medium     | Medium   | Add controller feedback (rumble, screen messages) |

### 9.3 Competition Risks

| Risk                               | Likelihood | Impact | Mitigation                                           |
| ---------------------------------- | ---------- | ------ | ---------------------------------------------------- |
| Anti-jam activates when not needed | Low        | Medium | Disable easily via config if issues arise            |
| Causes autonomous timing issues    | Low        | High   | Test extensively, measure timing impact              |
| Team unfamiliar with new behavior  | Medium     | Low    | Documentation, training, clearly communicate changes |

---

## 10. Open Questions

### 10.1 Technical Questions

1. **Q:** What is the actual update rate of `motor.get_actual_velocity()`?  
   **A:** Needs testing. May be 10-20ms. Could affect debounce tuning.

2. **Q:** Should we use velocity or position change for jam detection?  
   **A:** Velocity is simpler. Position could be more accurate but requires more state tracking.

3. **Q:** Should reversal velocity scale with commanded velocity?  
   **A:** Current design uses fixed reversal velocity. Could make proportional for tuning.

4. **Q:** How do we handle color sensor rejection + anti-jam simultaneously?  
   **A:** Color sensor already calls `hold_intake()`, which disables anti-jam. Should be fine.

### 10.2 Configuration Questions

1. **Q:** Should anti-jam parameters be configurable from SD card?  
   **A:** Could add to config UI. Probably overkill for initial implementation.

2. **Q:** Should we add controller rumble feedback on jam?  
   **A:** Nice to have for driver awareness. Low priority.

3. **Q:** Should we log jam events to SD card?  
   **A:** Useful for post-match analysis. Could add later.

### 10.3 Design Questions

1. **Q:** Should we have different configs for high goal vs. mid goal?  
   **A:** Probably not needed. Single config should work for both.

2. **Q:** Should autonomous functions automatically enable anti-jam?  
   **A:** Make it optional with default=true. Some routines might not want it.

3. **Q:** Should we add a "gentle" reversal mode that ramps velocity?  
   **A:** Could be gentler on mechanism. Test if needed.

---

## Appendix A: API Reference

### Motor Control Functions

```cpp
// PROS Motor API (relevant functions)
int32_t pros::Motor::get_actual_velocity();      // Returns RPM
int32_t pros::Motor::get_current_draw();         // Returns mA
int32_t pros::Motor::move_velocity(int velocity); // Set velocity (RPM)
```

### Timing Functions

```cpp
// PROS Timing API
uint32_t pros::millis();              // Current time in milliseconds
void pros::delay(uint32_t milliseconds); // Blocking delay
```

---

## Appendix B: State Machine Diagram (Detailed)

```
┌─────────────────────────────────────────────────────────────────┐
│                         NORMAL MODE                             │
│                                                                 │
│  Entry: Default state / Reset from operation change             │
│  Action: Monitor motor velocity every loop                      │
│  Exit: Velocity < threshold for > debounce time                 │
│                                                                 │
└──────────────────────────┬──────────────────┬───────────────────┘
                          │                   │
       Jam Detected       │                   │ Operation stopped/changed
       (velocity ~0)      │                   │ (resetAntiJamState())
                          ▼                   │
┌─────────────────────────────────────────┐   │
│           JAM_DETECTED MODE             │   │
│                                         │   │
│  Entry: Record jam time                 │   │
│  Action: Prepare for reversal           │   │
│  Exit: Immediately (transition state)   │   │
│                                         │   │
└──────────────────┬──────────────────────┘   │
                  │ Start reversal            │
                  ▼                           │
┌─────────────────────────────────────────┐   │
│          REVERSING MODE                 │   │
│                                         │   │
│  Entry: Start reversal timer            │   │
│  Action: Reverse intake motor           │   │
│         (outtake continues normally)    │   │
│  Exit: Timer elapsed (200ms default)    │   │
│                                         │   │
└──────────────────┬──────────────────────┘   │
                  │ Reversal complete         │
                  │ (duration elapsed)        │
                  ▼                           │
         Return to NORMAL  ◄──────────────────┘
         (continue operation)         Return to NORMAL
                                     (operation stopped/changed)
```

**Key Points:**
- NORMAL: Default state, active monitoring, jam detection enabled
- JAM_DETECTED: Transition state, immediately moves to REVERSING
- REVERSING: Intake reverses for fixed duration, outtake unaffected
- State resets to NORMAL when operation changes or stops (via resetAntiJamState())
- No IDLE or DISABLED state - simple three-state loop
- Always ready to handle jams during scoring operations

---

## Appendix C: Example Code Snippets

### Complete Example: Autonomous High Goal Scoring

```cpp
void auton_score_three_high_goals() {
    // Move to scoring position
    chassis.moveToPose(24, 24, 90, 2000, {.maxSpeed = 80}, false);
    
    // Score with anti-jam (3 seconds)
    controll_rollers_auton(
        INTAKE_HIGH_GOAL_VELOCITY,   // 600 RPM intake
        OUTTAKE_HIGH_GOAL_VELOCITY,  // 600 RPM outtake
        3000,                        // 3 seconds
        true                         // Enable anti-jam
    );
    
    // Stop rollers
    stop_rollers();
}
```

### Complete Example: Driver Control Scoring

```cpp
// Simplified opcontrol() with modular roller operation function
void opcontrol() {
    // ... initialization ...
    
    AntiJamState antiJamState;
    RollerOperation currentRollerOp = RollerOperation::NONE;
    RollerOperation previousRollerOp = RollerOperation::NONE;

    while (true) {
        // ... drive control ...
        
        // Determine current roller operation
        if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1)) {
            currentRollerOp = RollerOperation::HIGH_GOAL;
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)) {
            currentRollerOp = RollerOperation::MID_GOAL;
        } else {
            currentRollerOp = RollerOperation::NONE;
        }
        
        // Reset state on operation change
        if (currentRollerOp != previousRollerOp) {
            resetAntiJamState(antiJamState);
            previousRollerOp = currentRollerOp;
        }
        
        // Execute roller operation (handles anti-jam internally)
        executeRollerOperation(currentRollerOp, antiJamState);
        
        delay(10);
    }
}
```

---

## Revision History

| Version | Date       | Author       | Changes                                                                                                                                                                                                                                                                                     |
| ------- | ---------- | ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1.0     | 2026-01-22 | AI Assistant | Initial draft specification                                                                                                                                                                                                                                                                 |
| 1.1     | 2026-01-22 | AI Assistant | Updated based on feedback: removed DISABLED state, simplified controll_rollers(), added operation change detection, separated anti-jam logic from low-level functions                                                                                                                       |
| 1.2     | 2026-01-22 | AI Assistant | Refinements: removed IDLE state (use NORMAL as default), simplified updateAntiJamStateMachine() to return int instead of out parameters, renamed to RollerOperation, extracted executeRollerOperation() helper function for maintainability, combined HIGH_GOAL and MID_GOAL logic patterns |

---

## Approval

- [ ] Technical Lead Review
- [ ] Driver Review
- [ ] Implementation Ready
- [ ] Testing Plan Approved

