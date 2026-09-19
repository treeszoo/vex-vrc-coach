# Anti-Jam System Implementation Summary

**Date:** January 22, 2026  
**Status:** ✅ Complete

---

## Overview

Successfully implemented a comprehensive anti-jam system for the VEX VRC robot's intake and outtake motors. The system detects when the intake motor stalls during scoring operations and automatically executes brief reversal cycles to clear jams, while maintaining continuous outtake operation.

---

## Implementation Details

### Files Modified

#### 1. **`include/robot_utils.h`**
- Added anti-jam state machine enums and structs:
  - `AntiJamMode`: NORMAL, JAM_DETECTED, REVERSING
  - `AntiJamConfig`: Configuration parameters (thresholds, timings)
  - `AntiJamState`: Runtime state tracking
  - `RollerOperation`: Enum for driver control operations
- Added function declarations for anti-jam system
- Exported roller velocity constants
- **Breaking Change:** Removed `anti_jam` parameter from `controll_rollers()` and helper functions

#### 2. **`src/robot_utils.cpp`**
- **Core Anti-Jam Functions:**
  - `detectJam()`: Monitors intake motor velocity with debouncing
  - `updateAntiJamStateMachine()`: State machine logic, returns adjusted intake velocity
  - `resetAntiJamState()`: Resets state to NORMAL
  
- **Driver Control Helper:**
  - `executeRollerOperation()`: Handles all roller operations with integrated anti-jam
  
- **Autonomous Function:**
  - `controll_rollers_auton()`: Time-boxed roller control with anti-jam for autonomous mode
  
- **Simplified Functions:** Removed `anti_jam` parameter from:
  - `controll_rollers()`
  - `score_high_goal()`
  - `score_mid_goal()`
  - `score_mid_goal_skill_manual()`
  - All other roller helper functions

#### 3. **`src/competition.cpp`**
- **Refactored `opcontrol()` function:**
  - Added `AntiJamState` instance for driver control
  - Added `RollerOperation` tracking (current and previous)
  - Detects operation changes and resets anti-jam state
  - Uses `executeRollerOperation()` for all roller control
  - Anti-jam automatically active for HIGH_GOAL and MID_GOAL operations
  - No anti-jam for INTAKE_ONLY and REVERSE operations

#### 4. **`src/auton_alliance.cpp`**
- Updated 4 locations to use new function signatures:
  - Removed `anti_jam` parameter from `controll_rollers()` calls
  - Replaced `score_high_goal(false)` with `controll_rollers_auton(600, 600, duration, true)`
  - Replaced blocking roller calls with time-boxed `controll_rollers_auton()`

#### 5. **`src/auton_skills.cpp`**
- Updated 2 locations to use new function signatures:
  - Removed `anti_jam` parameter from `controll_rollers()` calls
  - Replaced `score_high_goal(false)` with `controll_rollers_auton(600, 600, duration, true)`

#### 6. **`src/robot_utils.cpp` (Helper Function)**
- Updated `goAndScoreHighGoal()` to use `controll_rollers_auton()` instead of deprecated `score_high_goal(false)`

---

## How It Works

### Driver Control Mode

```
User presses L1 (HIGH_GOAL) or L2 (MID_GOAL)
    ↓
opcontrol() detects operation change
    ↓
resetAntiJamState() called
    ↓
executeRollerOperation() called every 10ms loop
    ↓
updateAntiJamStateMachine() monitors intake velocity
    ↓
If jam detected (velocity ~0 for >50ms):
    - Reverse intake motor for 200ms
    - Outtake continues normally
    - Return to normal operation
    ↓
User releases button or changes operation
    ↓
resetAntiJamState() called (ready for next operation)
```

### Autonomous Mode

```
Autonomous function calls controll_rollers_auton(intake, outtake, duration, true)
    ↓
Creates local AntiJamState instance
    ↓
Runs loop for specified duration (e.g., 1220ms):
    - Every 10ms: updateAntiJamStateMachine()
    - Monitors intake velocity
    - Reverses if jam detected
    - Outtake continues normally
    ↓
Duration complete, function returns
```

---

## Configuration Parameters

Default values in `AntiJamConfig`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `velocityThreshold` | 10 RPM | Velocity below which motor considered jammed |
| `debounceTimeMs` | 50 ms | Time motor must be stopped before jam confirmed |
| `reversalDurationMs` | 200 ms | How long to reverse intake motor |
| `reversalVelocity` | 200 RPM | Velocity for reversal (backward) |
| `cooldownTimeMs` | 100 ms | Time after reversal before detecting next jam |

These can be tuned by modifying the `AntiJamConfig` struct in the `AntiJamState` instance.

---

## State Machine

```
NORMAL (monitoring)
   ↓ (velocity ~0 for >50ms)
JAM_DETECTED (transition state)
   ↓ (immediately)
REVERSING (reverse for 200ms)
   ↓ (duration elapsed)
NORMAL (resume operation)
```

**State Resets:**
- When operation changes (L1 → L2, L1 → NONE, etc.)
- When button released
- At start of new autonomous operation

---

## Key Features

✅ **Velocity-Based Detection:** Uses actual motor velocity instead of unreliable current draw  
✅ **Debouncing:** 50ms confirmation period prevents false positives  
✅ **Non-Blocking:** Driver control remains responsive during reversal  
✅ **Operation Tracking:** Automatically resets state when switching operations  
✅ **Dual Implementation:** Separate patterns for driver control (event loop) vs autonomous (linear)  
✅ **Outtake Unaffected:** Outtake motor continues at commanded velocity during reversal  
✅ **Configurable:** All timing and threshold parameters can be tuned  
✅ **Modular:** Clean separation between low-level motor commands and anti-jam logic  

---

## Testing Recommendations

### Driver Control Tests
1. ✅ Normal scoring (L1/L2) without jams
2. ⚠️ Manually block intake during scoring - verify reversal occurs
3. ⚠️ Rapid button changes (L1 → L2 → L1) - verify state resets
4. ⚠️ Hold L1, release, press again - verify state resets
5. ⚠️ R1 (intake only) - verify no anti-jam activation
6. ⚠️ R2 (reverse) - verify no anti-jam activation

### Autonomous Tests
1. ✅ Normal autonomous routines
2. ⚠️ Intentionally cause jam during autonomous scoring
3. ⚠️ Verify timing remains consistent with anti-jam active
4. ⚠️ Test all autonomous modes (left, right, solo, skills)

### Parameter Tuning
1. ⚠️ Monitor false positive rate (anti-jam activating when not needed)
2. ⚠️ Monitor false negative rate (jams not detected)
3. ⚠️ Adjust `velocityThreshold` if needed (currently 10 RPM)
4. ⚠️ Adjust `debounceTimeMs` if too sensitive/insensitive (currently 50ms)
5. ⚠️ Adjust `reversalDurationMs` if jams not clearing (currently 200ms)
6. ⚠️ Adjust `reversalVelocity` if reversal too weak/strong (currently 200 RPM)

---

## Breaking Changes

### Function Signatures Changed

**Before:**
```cpp
void controll_rollers(int velocity_intake, int velocity_outtake, bool anti_jam = false);
void score_high_goal(bool anti_jam = true);
```

**After:**
```cpp
void controll_rollers(int velocity_intake, int velocity_outtake);
void score_high_goal();
```

### Migration Required

All code calling these functions with the `anti_jam` parameter must be updated:
- ✅ Driver control: Refactored to use state machine
- ✅ Autonomous alliance: Updated 4 locations
- ✅ Autonomous skills: Updated 2 locations
- ✅ Helper functions: Updated 1 location

---

## Known Limitations

1. **Motor API Dependency:** Relies on `motor.get_actual_velocity()` accuracy
2. **No Telemetry:** Currently no logging of jam events (can be added later)
3. **Fixed Parameters:** Configuration requires code changes (could add SD card config)
4. **No Controller Feedback:** No rumble or screen notification on jam (can be added)

---

## Future Enhancements

- [ ] Add controller rumble feedback when jam detected
- [ ] Log jam events to SD card for post-match analysis
- [ ] Add configurable parameters via SD card or config UI
- [ ] Add different configs for high goal vs mid goal
- [ ] Add telemetry display on brain screen
- [ ] Add jam count tracking and reporting

---

## Code Statistics

- **Lines Added:** ~300
- **Lines Modified:** ~50
- **Files Modified:** 6
- **New Functions:** 5
- **New Structs/Enums:** 4

---

## Approval Status

- [x] Implementation Complete
- [ ] Driver Control Testing
- [ ] Autonomous Testing
- [ ] Parameter Tuning
- [ ] Competition Ready

---

## References

- Technical Specification: `/docs/anti-jam-technical-spec.md`
- PROS Motor API: `motor.get_actual_velocity()`
- State Machine Pattern: Three-state loop (NORMAL → JAM_DETECTED → REVERSING)

