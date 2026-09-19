# Unified Debug System

## Overview

Both the controller screen and brain LCD now display synchronized debug information controlled by a single button combo (**UP + A**). The system provides 4 modes optimized for each screen's format.

## Debug Modes

Press **UP + A** to cycle through modes (both screens change together):

### Mode 1: POSITION (Default)
**Purpose:** Monitor robot location and orientation during driving

**Controller Screen (3 lines):**
```
XY:12.3,45.6        (Current position)
H:89.0 I:89.2       (Heading, IMU rotation)
A:12.3,45.6         (Absolute position)
```

**Brain LCD (5 lines):**
```
Position: X=12.3, Y=45.6
Heading: 89.0 deg
IMU: 89.2 deg
Absolute: X=12.3, Y=45.6
(blank)
```

### Mode 2: OPTICAL_SENSOR
**Purpose:** Monitor color sensor and block detection

**Controller Screen (3 lines):**
```
Prx:123 Hue:234     (Proximity, Hue)
Color:RED           (Detected color)
XY:12.3,45.6        (Position for reference)
```

**Brain LCD (5 lines):**
```
Proximity: 123
Hue: 234.5
Detected: RED
(blank)
X: 12.3  Y: 45.6
```

### Mode 3: INTAKE_MOTOR
**Purpose:** Diagnose anti-jam system and motor issues

**Controller Screen (3 lines):**
```
A:580 C:600         (Actual vs Commanded velocity)
NORMAL T:15         (State, Threshold)
Db:45/350ms         (Debounce timer)
  OR
Rv:145/300ms        (Reversal progress when reversing)
```

**Brain LCD (5 lines):**
```
Intake: A=580 C=600
State: NORMAL (T=15)
Debounce: 45ms/350ms  (or Reverse: X/300ms)
Op: HIGH_GOAL
Now:5432 Db:123 Rv:0  (Timestamps for debugging)
```

### Mode 4: DISABLED
**Purpose:** Minimal output during competition

**Controller Screen:** Cleared  
**Brain LCD:** No output

## Controls

| Button Combo | Action                                            | Feedback                          |
| ------------ | ------------------------------------------------- | --------------------------------- |
| **UP + A**   | Cycle debug mode                                  | Rumble pattern (disabled in code) |
|              | POSITION → OPTICAL → INTAKE → DISABLED → POSITION | 1-4 pulses                        |

## Usage

### During Practice/Testing
- **POSITION mode (default):** Always visible, track robot location
- **OPTICAL mode:** When testing color sensor and block detection
- **INTAKE mode:** When debugging anti-jam or motor issues

### During Competition
- **POSITION mode:** Monitor position during driver control
- **DISABLED mode:** Clean screens if debug info is distracting

## Technical Details

### Synchronization
- Single `DebugMode` enum controls both screens
- Both `controllerScreenControl()` and `lcdScreenControl()` tasks read the same variable
- Mode changes instantly affect both screens

### Update Rate
- **Controller:** 50ms (20Hz)
- **Brain LCD:** 50ms (20Hz)

### Format Constraints
- **Controller:** 3 lines × 18 characters (compact, abbreviations)
- **Brain LCD:** 5 lines × unlimited (detailed, full names)

## Implementation

### Files
- `include/debug_output.h` - Debug mode enum and function declarations
- `src/debug_output.cpp` - Controller and LCD specific implementations
- `src/competition.cpp` - Debug mode control and task coordination

### Key Functions
```cpp
// Controller (compact)
printPositionDebugController()
printOpticalDebugController()
printIntakeMotorDebugController(state, op)

// Brain LCD (detailed)
printPositionDebugLCD()
printOpticalDebugLCD()
printIntakeMotorDebugLCD(state, op)
```

### Adding New Debug Modes

1. Add enum value to `DebugMode` in `debug_output.h`
2. Implement controller and LCD functions in `debug_output.cpp`
3. Add cases to both switch statements in `competition.cpp`
4. Update modulo divisor: `% 4` → `% 5` (for 5 modes)
5. Add rumble pattern to array

## Example: Monitoring Anti-Jam

**Step 1:** Switch to INTAKE_MOTOR mode
- Press **UP + A** until "A:XXX C:XXX" appears on controller

**Step 2:** Start scoring (L1 or L2)
- Controller shows: `A:580 C:600` (motor running)
- Controller shows: `NORMAL T:15` (monitoring for jams)

**Step 3:** Block the roller manually
- Watch controller: `A:5 C:600` (velocity drops)
- Watch controller: `Db:45/350ms` (debounce counting)
- After 350ms: `JAM_DETECT` then `REVERSING`
- Watch controller: `Rv:145/300ms` (reversal progress)

**Step 4:** Check brain LCD for details
- Line 5 shows timestamps for advanced debugging

## Troubleshooting

**Problem:** Screens not synchronized
- Check that both tasks use `CURRENT_DEBUG_MODE` variable
- Verify button toggle updates the shared variable

**Problem:** Controller display garbled
- Strings longer than 18 chars will wrap
- Use abbreviations (Prx, Db, Rv, etc.)

**Problem:** Can't see position during scoring
- Switch to POSITION mode with UP+A
- Position always visible on line 3 of OPTICAL mode

## Future Enhancements

- [ ] Battery voltage display mode
- [ ] Temperature monitoring mode
- [ ] Match timer display
- [ ] Motor current monitoring
- [ ] Persistent mode selection (saved to SD card)
- [ ] Different modes for controller vs LCD
