# Debug System Usage Guide

## Overview

The debug system provides modular LCD output to help diagnose issues with the robot. You can easily switch between different debug modes by changing one line of code.

## Quick Start

### Switching Debug Modes

**File:** `src/competition.cpp`  
**Line:** ~40 (in the Debug System section)

```cpp
// Change this line to switch debug output:
static const DebugMode CURRENT_DEBUG_MODE = DebugMode::INTAKE_MOTOR;
```

**Available Modes:**
- `DebugMode::INTAKE_MOTOR` - Display intake motor and anti-jam state
- `DebugMode::OPTICAL_SENSOR` - Display optical sensor information
- `DebugMode::DISABLED` - No debug output

## Debug Modes

### 1. INTAKE_MOTOR Mode

**Purpose:** Diagnose anti-jam system and intake motor issues

**LCD Display (5 lines):**
```
Line 1: Intake: A=123 C=600
        ↑ Actual velocity | ↑ Commanded velocity

Line 2: State: NORMAL (T=15)
        ↑ Anti-jam state | ↑ Velocity threshold

Line 3: Debounce: 45ms/50ms
        ↑ Current | ↑ Required (for jam detection)

Line 4: Op: HIGH_GOAL
        ↑ Current roller operation

Line 5: X: 12.3  Y: 45.6
        ↑ Robot position
```

**What to Look For:**

**Anti-Jam Not Triggering:**
1. **Check Actual Velocity (Line 1):**
   - Is it dropping to ~0 when you block the roller?
   - If not, motor might not be stalling properly
   
2. **Check State (Line 2):**
   - Should be NORMAL when running normally
   - Should change to JAM_DETECT then REVERSING when blocked
   - If stuck in NORMAL, velocity might not be below threshold
   
3. **Check Debounce Timer (Line 3):**
   - Should count up when motor is stopped
   - Should reach 50ms before jam is confirmed
   - Resets to 0 when motor starts moving again
   
4. **Check Threshold (Line 2):**
   - Current threshold is 15 RPM
   - If actual velocity stays above 15, jam won't trigger
   - May need to adjust threshold in `robot_utils.h`

**State Transitions:**
```
NORMAL → JAM_DETECT → REVERSING → NORMAL
```

### 2. OPTICAL_SENSOR Mode

**Purpose:** Diagnose color sensor and block detection

**LCD Display (5 lines):**
```
Line 1: Proximity: 123
Line 2: Hue: 234.5
Line 3: Detected: RED
Line 4: (blank)
Line 5: X: 12.3  Y: 45.6
```

### 3. DISABLED Mode

No debug output displayed on LCD.

## Troubleshooting Anti-Jam

### Problem: Anti-jam never triggers

**Check:**
1. Is `CURRENT_DEBUG_MODE` set to `INTAKE_MOTOR`?
2. Are you pressing L1 or L2 (HIGH_GOAL or MID_GOAL)?
   - Anti-jam only active for scoring operations
   - Not active for R1 (INTAKE_ONLY) or R2 (REVERSE)
3. Watch Line 1 (Actual velocity):
   - Does it drop below 15 RPM when blocked?
   - If not, motor might not be stalling
4. Watch Line 3 (Debounce):
   - Does it count up to 50ms?
   - If not, velocity might be fluctuating

**Solutions:**
- If velocity stays above 15 RPM, lower threshold in `robot_utils.h`:
  ```cpp
  int velocityThreshold = 10;  // Try lower value
  ```
- If debounce doesn't reach 50ms, reduce debounce time:
  ```cpp
  int debounceTimeMs = 30;  // Try shorter time
  ```

### Problem: Anti-jam triggers too often (false positives)

**Check:**
1. Watch Line 1 during normal operation
2. Does actual velocity drop below 15 RPM during normal scoring?

**Solutions:**
- Increase velocity threshold:
  ```cpp
  int velocityThreshold = 20;  // Higher threshold
  ```
- Increase debounce time:
  ```cpp
  int debounceTimeMs = 100;  // Longer confirmation time
  ```

### Problem: Reversal too short/long

**Adjust reversal duration in `robot_utils.h`:**
```cpp
int reversalDurationMs = 60;  // Current: 60ms, try 100-200ms
```

**Adjust reversal velocity:**
```cpp
int reversalVelocity = 200;  // Current: 200 RPM, try higher/lower
```

## Configuration Parameters

**File:** `include/robot_utils.h`  
**Struct:** `AntiJamConfig`

```cpp
struct AntiJamConfig {
  int velocityThreshold = 15;  // RPM below which motor considered jammed
  int debounceTimeMs = 50;     // Time motor must be stopped before jam detected
  int reversalDurationMs = 60; // How long to reverse motor
  int reversalVelocity = 200;  // Velocity for reversal (positive = backward)
};
```

## Tips

1. **Start with INTAKE_MOTOR mode** when debugging anti-jam
2. **Block the roller manually** and watch the state transitions
3. **Record the values** you see on the LCD for analysis
4. **Adjust one parameter at a time** and test
5. **Switch to OPTICAL_SENSOR mode** when debugging color detection
6. **Use DISABLED mode** during competition if debug output not needed

## Example Debug Session

```
1. Set CURRENT_DEBUG_MODE = DebugMode::INTAKE_MOTOR
2. Upload code to robot
3. Press L1 to start scoring
4. Observe LCD:
   - Line 1: Intake: A=580 C=600  (motor running normally)
   - Line 2: State: NORMAL (T=15)
   - Line 3: Debounce: 0ms/50ms
   - Line 4: Op: HIGH_GOAL
5. Manually block the roller
6. Watch LCD change:
   - Line 1: Intake: A=5 C=600     (velocity dropped!)
   - Line 2: State: NORMAL (T=15)  (still normal, waiting for debounce)
   - Line 3: Debounce: 23ms/50ms   (counting up...)
7. After 50ms:
   - Line 2: State: JAM_DETECT     (jam confirmed!)
8. Immediately:
   - Line 2: State: REVERSING      (reversing intake)
   - Line 1: Intake: A=-200 C=600  (negative velocity = reverse)
9. After 60ms:
   - Line 2: State: NORMAL         (back to normal)
   - Line 1: Intake: A=580 C=600   (resumed normal operation)
```

If you don't see these transitions, use the troubleshooting guide above!

