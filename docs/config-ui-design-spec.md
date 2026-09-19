# VEX VRC Robot Configuration UI - Design Specification

## Overview

This document specifies the design for a robot configuration system that allows players to configure auton mode and team color via a touchscreen UI on the VEX V5 Brain. The configuration is saved to a file on the SD card and loaded by the competition application during initialization.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Git Repository                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────┐    ┌─────────────────────────────────┐     │
│  │  Branch: configure-UI       │    │  Branch: main                   │     │ 
│  │  ┌───────────────────────┐  │    │  ┌───────────────────────────┐  │     |
│  │  │ Config UI App         │  │    │  │ Competition App           │  │     │
│  │  │ • Display touch UI    │  │    │  │ • Load config.txt         │  │     │
│  │  │ • Save config.txt     │  │    │  │ • Run autonomous/opcontrol│  │     │
│  │  └───────────────────────┘  │    │  └───────────────────────────┘  │     │
│  └─────────────────────────────┘    └─────────────────────────────────┘     │
│                                                                             │
│  Shared: include/robot_config_data.h (enums, constants, helpers)            │
└─────────────────────────────────────────────────────────────────────────────┘
                          │                           │
                          ▼                           ▼
                    ┌───────────┐              ┌───────────┐
                    │  Upload   │              │  Upload   │
                    │  (setup)  │              │  (match)  │
                    └─────┬─────┘              └─────┬─────┘
                          │                          │
                          ▼                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              VEX V5 Brain                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────────┐            ┌────────────────────────────────────┐ │
│  │   Config UI App      │   Writes   │   Competition App                  │ │
│  │   (pre-match setup)  │ ─────────► │   (during match)                   │ │
│  │                      │            │                                    │ │
│  │  • Auton selection   │            │   Global Variables:                │ │
│  │  • Team color        │            │   • selectedAutonMode              │ │
│  │  • Color sensor on/off│           │   • teamColor                      │ │
│  └──────────────────────┘            │   • useColorSensor                 │ │
│                                      └────────────────────────────────────┘ │
│                                                                             |
│                    ┌─────────────────────────────┐                          │
│                    │     SD Card (/usd/)         │                          │
│                    │     └── config.txt          │                          │
│                    └─────────────────────────────┘                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Configuration Parameters

### 1. Auton Mode (`AutonomouMode` enum)

| Mode Name            | Enum Value              | Description                          |
|----------------------|-------------------------|--------------------------------------|
| Left Side Safe       | `ALLIANCE_LEFT_SAFE`    | Conservative left-side autonomous    |
| Left Side Aggressive | `ALLIANCE_LEFT_AGGRESSIVE` | Aggressive left-side autonomous   |
| Right Side           | `ALLIANCE_RIGHT`        | Right-side autonomous                |
| Solo AWP             | `ALLIANCE_SOLO`         | Solo Autonomous Win Point routine    |

### 2. Team Color (`TeamColor` enum)

| Color  | Enum Value | Description           |
|--------|------------|-----------------------|
| Red    | `RED`      | Red alliance team     |
| Blue   | `BLUE`     | Blue alliance team    |

### 3. Use Color Sensor (`bool`)

| Value | Description                                      |
|-------|--------------------------------------------------|
| true  | Enable color sensor for ball sorting/detection   |
| false | Disable color sensor (ignore sensor readings)    |

This toggle allows the robot to operate with or without color sensor functionality. When disabled, the robot will not use color-based ball sorting or detection, which can be useful if the sensor is malfunctioning or not needed for a particular match strategy.

---

## File Format

### Location
```
/usd/config.txt
```

### Format (Simple Key-Value)
```
auton=2
color=1
use_color_sensor=1
```

Where:
- `auton`: Integer value (0-3) corresponding to AutonomousMode enum
  - `0` = ALLIANCE_LEFT_SAFE
  - `1` = ALLIANCE_LEFT_AGGRESSIVE
  - `2` = ALLIANCE_RIGHT
  - `3` = ALLIANCE_SOLO
- `color`: Integer value (0-1) corresponding to TeamColor enum
  - `0` = RED
  - `1` = BLUE
- `use_color_sensor`: Integer value (0-1) for boolean toggle
  - `0` = Disabled (do not use color sensor)
  - `1` = Enabled (use color sensor)

---

## UI Design

### Screen Dimensions
- VEX V5 Brain Screen: **480 x 240 pixels**

### Layout

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         ROBOT CONFIGURATION                                 │
├────────────────────────────────────────────────────────────────────────────┤
│   AUTON MODE                                                                │
│   ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐                  │
│   │ LEFT SAFE │ │LEFT AGGR. │ │   RIGHT   │ │ SOLO AWP  │                  │
│   └───────────┘ └───────────┘ └───────────┘ └───────────┘                  │
├────────────────────────────────────────────────────────────────────────────┤
│   TEAM COLOR                                                                │
│        ┌──────────────────┐         ┌──────────────────┐                   │
│        │       RED        │         │       BLUE       │                   │
│        └──────────────────┘         └──────────────────┘                   │
├────────────────────────────────────────────────────────────────────────────┤
│   OPTIONS                                                                   │
│        ┌────┐                                                               │
│        │ ✓  │  Use Color Sensor                                            │
│        └────┘                                                               │
├────────────────────────────────────────────────────────────────────────────┤
│                         ┌─────────────────┐                                 │
│                         │      SAVE       │                                 │
│                         └─────────────────┘                                 │
└────────────────────────────────────────────────────────────────────────────┘
```

### Pixel Coordinates

| Element               | X1   | Y1   | X2   | Y2   | Width | Height |
|-----------------------|------|------|------|------|-------|--------|
| **Title Bar**         | 0    | 0    | 480  | 25   | 480   | 25     |
| **Auton Section**     |      |      |      |      |       |        |
| - Section Label       | 10   | 28   | 470  | 43   | 460   | 15     |
| - Left Safe Btn       | 10   | 46   | 120  | 86   | 110   | 40     |
| - Left Aggressive Btn | 125  | 46   | 235  | 86   | 110   | 40     |
| - Right Btn           | 240  | 46   | 350  | 86   | 110   | 40     |
| - Solo AWP Btn        | 355  | 46   | 470  | 86   | 115   | 40     |
| **Color Section**     |      |      |      |      |       |        |
| - Section Label       | 10   | 92   | 470  | 107  | 460   | 15     |
| - Red Btn             | 60   | 110  | 220  | 150  | 160   | 40     |
| - Blue Btn            | 260  | 110  | 420  | 150  | 160   | 40     |
| **Options Section**   |      |      |      |      |       |        |
| - Section Label       | 10   | 156  | 470  | 171  | 460   | 15     |
| - Checkbox            | 60   | 174  | 90   | 204  | 30    | 30     |
| - Checkbox Label      | 100  | 174  | 300  | 204  | 200   | 30     |
| **Save Button**       | 165  | 210  | 315  | 238  | 150   | 28     |

### Visual Feedback

- **Selected Button**: Filled with highlight color (inverted background)
- **Unselected Button**: Outlined only (border with transparent/background fill)
- **Touch Feedback**: Brief visual inversion when button is pressed

### Color Scheme

| Element                  | Color (Hex)  | Description                  |
|--------------------------|--------------|------------------------------|
| Background               | `0x000000`   | Black                        |
| Text (Primary)           | `0xFFFFFF`   | White                        |
| Button Border            | `0xFFFFFF`   | White outline                |
| Red Button (selected)    | `0xFF0000`   | Red fill                     |
| Red Button (unselected)  | `0x800000`   | Dark red outline             |
| Blue Button (selected)   | `0x0000FF`   | Blue fill                    |
| Blue Button (unselected) | `0x000080`   | Dark blue outline            |
| Save Button              | `0x00FF00`   | Green                        |
| Selection Highlight      | `0xFFFF00`   | Yellow highlight for auton   |
| Checkbox (checked)       | `0x00FF00`   | Green fill with checkmark    |
| Checkbox (unchecked)     | `0x404040`   | Dark gray outline            |

---

## Application 1: Configure UI App

### File Structure

```
src/
├── configure.cpp        # Main configuration UI application
include/
├── robot_config_data.h  # Shared configuration types and file paths
```

### Pseudocode

```cpp
// configure.cpp - Configuration UI Application

#include "robot_config_data.h"

// Current selections (in-memory)
AutonomousMode currentAuton = AutonomousMode::ALLIANCE_LEFT_SAFE;
TeamColor currentColor = TeamColor::RED;
bool useColorSensor = true;  // Default to enabled

void initialize() {
    // Load existing config if available
    loadConfig();

    // Draw initial UI
    drawUI();
}

void opcontrol() {
    while (true) {
        // Check for screen touch
        screen_touch_status_s_t status = pros::screen::touch_status();

        if (status.touch_status == E_TOUCH_PRESSED) {
            handleTouch(status.x, status.y);
        }

        pros::delay(50);  // 20Hz polling
    }
}

void drawUI() {
    // Clear screen
    pros::screen::erase();

    // Draw title
    drawTitle();

    // Draw auton mode section
    drawAutonSection();

    // Draw team color section
    drawColorSection();

    // Draw options section (checkboxes)
    drawOptionsSection();

    // Draw save button
    drawSaveButton();
}

void handleTouch(int x, int y) {
    // Check auton buttons
    if (inBounds(x, y, AUTON_LEFT_SAFE_BTN)) {
        currentAuton = AutonomousMode::ALLIANCE_LEFT_SAFE;
        drawAutonSection();
    }
    // ... similar for other auton buttons

    // Check color buttons
    if (inBounds(x, y, RED_BTN)) {
        currentColor = TeamColor::RED;
        drawColorSection();
    }
    // ... similar for blue

    // Check color sensor checkbox
    if (inBounds(x, y, COLOR_SENSOR_CHECKBOX)) {
        useColorSensor = !useColorSensor;  // Toggle
        drawOptionsSection();
    }

    // Check save button
    if (inBounds(x, y, SAVE_BTN)) {
        saveConfig();
        showSaveConfirmation();
    }
}

void drawOptionsSection() {
    // Draw section label
    pros::screen::print(TEXT_MEDIUM, 10, 156, "OPTIONS");

    // Draw checkbox
    if (useColorSensor) {
        // Filled checkbox with checkmark
        pros::screen::set_pen(0x00FF00);  // Green
        pros::screen::fill_rect(60, 174, 90, 204);
        pros::screen::set_pen(0xFFFFFF);
        // Draw checkmark (simplified as X or filled)
    } else {
        // Empty checkbox
        pros::screen::set_pen(0x404040);  // Dark gray
        pros::screen::draw_rect(60, 174, 90, 204);
    }

    // Draw label
    pros::screen::set_pen(0xFFFFFF);
    pros::screen::print(TEXT_MEDIUM, 100, 182, "Use Color Sensor");
}

void saveConfig() {
    FILE* file = fopen("/usd/config.txt", "w");
    if (file) {
        fprintf(file, "auton=%d\n", static_cast<int>(currentAuton));
        fprintf(file, "color=%d\n", static_cast<int>(currentColor));
        fprintf(file, "use_color_sensor=%d\n", useColorSensor ? 1 : 0);
        fclose(file);
    }
}

void loadConfig() {
    FILE* file = fopen("/usd/config.txt", "r");
    if (file) {
        int auton, color, colorSensor;
        fscanf(file, "auton=%d\n", &auton);
        fscanf(file, "color=%d\n", &color);
        fscanf(file, "use_color_sensor=%d\n", &colorSensor);
        fclose(file);

        currentAuton = static_cast<AutonomousMode>(auton);
        currentColor = static_cast<TeamColor>(color);
        useColorSensor = (colorSensor != 0);
    }
}
```

---

## Application 2: Competition App (main.cpp)

### Changes Required

1. Add shared header include
2. Add global `teamColor` variable
3. Add `loadRobotConfig()` function
4. Call `loadRobotConfig()` in `initialize()`
5. Update `autonomous()` to use the new auton modes

### Pseudocode for Config Loading

```cpp
// In main.cpp

#include "robot_config_data.h"

// Global configuration variables
AutonomousMode selectedAutonMode = AutonomousMode::ALLIANCE_LEFT_SAFE;
TeamColor teamColor = TeamColor::RED;
bool useColorSensor = true;  // Default to enabled

void loadRobotConfig() {
    FILE* file = fopen("/usd/config.txt", "r");
    if (file) {
        int auton = 0, color = 0, colorSensor = 1;
        if (fscanf(file, "auton=%d\n", &auton) == 1) {
            if (auton >= 0 && auton <= 3) {
                selectedAutonMode = static_cast<AutonomousMode>(auton);
            }
        }
        if (fscanf(file, "color=%d\n", &color) == 1) {
            if (color >= 0 && color <= 1) {
                teamColor = static_cast<TeamColor>(color);
            }
        }
        if (fscanf(file, "use_color_sensor=%d\n", &colorSensor) == 1) {
            useColorSensor = (colorSensor != 0);
        }
        fclose(file);
    }
    // If file doesn't exist or has errors, defaults are used
}

void initialize() {
    pros::lcd::initialize();
    loadRobotConfig();

    // Display loaded config on LCD for verification
    pros::lcd::print(0, "Auton: %s", getAutonName(selectedAutonMode));
    pros::lcd::print(1, "Color: %s", teamColor == TeamColor::RED ? "RED" : "BLUE");
    pros::lcd::print(2, "Color Sensor: %s", useColorSensor ? "ON" : "OFF");

    // ... rest of initialization
}

// Example usage of useColorSensor in robot code:
void sortBall() {
    if (!useColorSensor) {
        // Skip color-based sorting when disabled
        return;
    }

    // Color sensor sorting logic here...
    TeamColor detectedColor = readColorSensor();
    if (detectedColor != teamColor) {
        ejectBall();
    }
}
```

---

## Shared Header: robot_config_data.h

```cpp
#pragma once

/**
 * Autonomous mode selection enum
 */
enum class AutonomousMode {
    ALLIANCE_LEFT_SAFE = 0,   // Left side - conservative
    ALLIANCE_LEFT_AGGRESSIVE = 1,  // Left side - aggressive
    ALLIANCE_RIGHT = 2,       // Right side
    ALLIANCE_SOLO = 3         // Solo AWP
};

/**
 * Team color enum
 */
enum class TeamColor {
    RED = 0,
    BLUE = 1
};

/**
 * Robot configuration structure
 * Groups all configurable parameters together
 */
struct RobotConfig {
    AutonomousMode autonMode = AutonomousMode::ALLIANCE_LEFT_SAFE;
    TeamColor teamColor = TeamColor::RED;
    bool useColorSensor = true;  // Enable/disable color sensor functionality
};

// Configuration file path
constexpr const char* CONFIG_FILE_PATH = "/usd/config.txt";

// Helper function to get auton mode name
inline const char* getAutonName(AutonomousMode mode) {
    switch (mode) {
        case AutonomousMode::ALLIANCE_LEFT_SAFE:  return "LEFT SAFE";
        case AutonomousMode::ALLIANCE_LEFT_AGGRESSIVE: return "LEFT AGGRESSIVE";
        case AutonomousMode::ALLIANCE_RIGHT:      return "RIGHT";
        case AutonomousMode::ALLIANCE_SOLO:       return "SOLO AWP";
        default: return "UNKNOWN";
    }
}

// Helper function to get team color name
inline const char* getColorName(TeamColor color) {
    switch (color) {
        case TeamColor::RED:  return "RED";
        case TeamColor::BLUE: return "BLUE";
        default: return "UNKNOWN";
    }
}
```

---

## Implementation Plan

### Phase 1: Shared Types & Config Loading (Competition App - `main` branch)
1. Create `include/robot_config_data.h` with shared types
2. Update `AutonomousMode` enum in `main.cpp` to use new values (add `ALLIANCE_LEFT_SAFE`, `ALLIANCE_LEFT_AGGRESSIVE`)
3. Add `TeamColor` and `useColorSensor` global variables
4. Implement `loadRobotConfig()` function with fallback defaults
5. Call `loadRobotConfig()` in `initialize()`
6. Test with manually created config file on SD card

### Phase 2: Configure UI App (`configure-UI` branch)
1. Checkout `configure-UI` branch
2. Replace `src/main.cpp` with Config UI implementation
3. Implement UI drawing functions (buttons, checkboxes)
4. Implement touch handling
5. Implement save/load functionality
6. Add visual feedback for selections
7. Copy `robot_config_data.h` to ensure compatibility

### Phase 3: Integration & Testing
1. Test workflow: Config UI → save → reboot → Competition App loads config
2. Verify config persists across power cycles
3. Test edge cases:
   - Missing SD card → defaults used
   - Corrupt/missing file → defaults used
   - Invalid values → per-field defaults
4. Verify application never crashes due to config issues

---

## Alternative Considered (Not Used)

~~**Single Application with Mode Toggle**~~

This alternative was considered but **not chosen**:
- Configuration mode integrated within the main application
- Toggle between config and competition modes

**Why not used**: Separate branches provide cleaner separation of concerns and prevent accidental deployment of the wrong mode during competition.

---

## Technical Considerations

### SD Card File I/O in PROS
- Use standard C file I/O: `fopen()`, `fprintf()`, `fscanf()`, `fclose()`
- Path prefix: `/usd/` for SD card root
- Check `pros::usd::is_installed()` before file operations

### Screen Touch Handling
- Use `pros::screen::touch_status()` to poll touch state
- Touch coordinates are in pixels (0-479 x, 0-239 y)
- Touch events: `E_TOUCH_PRESSED`, `E_TOUCH_HELD`, `E_TOUCH_RELEASED`

### Thread Safety
- Configuration loading should complete before other tasks start
- Use `initialize()` for config loading to ensure it runs first

---

## File Manifest

### Branch: `main` (Competition App)
| File                          | Type    | Description                         |
|-------------------------------|---------|-------------------------------------|
| `include/robot_config_data.h` | Header  | Shared enums, constants, helpers    |
| `src/main.cpp`                | Source  | Competition app with config loading |

### Branch: `configure-UI` (Config UI App)
| File                          | Type    | Description                         |
|-------------------------------|---------|-------------------------------------|
| `include/robot_config_data.h` | Header  | Shared enums, constants, helpers    |
| `src/main.cpp`                | Source  | Config UI app (replaces main.cpp)   |

### SD Card
| File                          | Type    | Description                         |
|-------------------------------|---------|-------------------------------------|
| `/usd/config.txt`             | Data    | Saved configuration (persisted)     |

---

## Design Decisions

### 1. Deployment Strategy
**Decision**: Use a separate Git branch (`configure-UI`) within the same repository.

- The `configure-UI` branch contains the Config UI application code
- The `main` branch contains the Competition application code
- Both branches share common headers (e.g., `robot_config_data.h`)
- To deploy Config UI: checkout `configure-UI` branch, build, and upload
- To deploy Competition: checkout `main` branch, build, and upload

### 2. Auton Mode Expansion
**Decision**: Expand the `AutonomousMode` enum to include distinct Left Safe and Left Aggressive modes.

The existing `ALLIANCE_LEFT` will be split into:
- `ALLIANCE_LEFT_SAFE` - Conservative approach, safer but potentially lower scoring
- `ALLIANCE_LEFT_AGGRESSIVE` - Higher risk, higher reward approach

### 3. Team Color & Color Sensor Usage
**Decision**: Configuration is stored for future functionality.

**Current State**: Not actively used in robot behavior.

**Future Functionality** (planned):
- Use the color sensor to detect opponent's ball color during intake
- When `useColorSensor` is enabled AND opponent's color is detected, stop picking/eject the ball
- `teamColor` determines what is "our" color vs "opponent's" color:
  - If `teamColor = RED`, opponent's color is BLUE
  - If `teamColor = BLUE`, opponent's color is RED

This gated functionality allows us to:
1. Develop and test the feature incrementally
2. Disable it during matches if it causes issues
3. Configure team color per match

### 4. Fallback Behavior
**Decision**: Always use safe default values; never crash due to configuration failures.

**Default Values** (used when config cannot be loaded):
| Parameter        | Default Value              | Rationale                          |
|------------------|----------------------------|------------------------------------|
| `autonMode`      | `ALLIANCE_LEFT_SAFE`       | Safe, conservative autonomous      |
| `teamColor`      | `RED`                      | Arbitrary default                  |
| `useColorSensor` | `false`                    | Disabled by default for safety     |

**Error Handling**:
- SD card not present → Use defaults, log warning
- File missing → Use defaults, no error
- File corrupt/unreadable → Use defaults, log warning
- Invalid values → Use defaults for that field only
- Application must **never crash** due to configuration issues

---

## Fallback Implementation

```cpp
// Default configuration values (fallback)
namespace ConfigDefaults {
    constexpr AutonomousMode AUTON_MODE = AutonomousMode::ALLIANCE_LEFT_SAFE;
    constexpr TeamColor TEAM_COLOR = TeamColor::RED;
    constexpr bool USE_COLOR_SENSOR = false;  // Disabled for safety
}

// Global configuration (initialized to defaults)
AutonomousMode selectedAutonMode = ConfigDefaults::AUTON_MODE;
TeamColor teamColor = ConfigDefaults::TEAM_COLOR;
bool useColorSensor = ConfigDefaults::USE_COLOR_SENSOR;

bool loadRobotConfig() {
    // Check if SD card is installed
    if (!pros::usd::is_installed()) {
        // SD card not present - use defaults (already set)
        return false;
    }

    FILE* file = fopen(CONFIG_FILE_PATH, "r");
    if (!file) {
        // File doesn't exist or can't be opened - use defaults
        return false;
    }

    // Read each value with validation
    int auton = -1, color = -1, colorSensor = -1;

    // Parse file (each field independently so partial files work)
    char line[64];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "auton=%d", &auton);
        sscanf(line, "color=%d", &color);
        sscanf(line, "use_color_sensor=%d", &colorSensor);
    }
    fclose(file);

    // Validate and apply auton mode
    if (auton >= 0 && auton <= 3) {
        selectedAutonMode = static_cast<AutonomousMode>(auton);
    }
    // else: keep default

    // Validate and apply team color
    if (color >= 0 && color <= 1) {
        teamColor = static_cast<TeamColor>(color);
    }
    // else: keep default

    // Validate and apply color sensor setting
    if (colorSensor == 0 || colorSensor == 1) {
        useColorSensor = (colorSensor == 1);
    }
    // else: keep default

    return true;
}
```

---

*Document Version: 1.2*
*Created: January 2026*
*Last Updated: January 2026 - Finalized design decisions*
