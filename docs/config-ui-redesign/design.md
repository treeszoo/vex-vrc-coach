# Config UI Redesign

## Problem

The current Config UI uses screen space inefficiently:
- 5 auton mode buttons (L SAFE, L AGGR, RIGHT, SOLO, SKILLS) — "aggressive" is really a modifier, not a separate mode
- TEAM COLOR section takes a full row with a section label, but only matters when color sensor is on
- Only one checkbox (color sensor) in the OPTIONS section

We need room for two new options (Aggressive, Push Alliance) without making the UI feel cramped.

## Changes

1. **Auton modes: 5 → 4** — Remove L SAFE / L AGGR distinction. Replace with single LEFT + Aggressive checkbox.
2. **Combine color sensor + RED/BLUE** into one compact row. RED/BLUE are dimmed when color sensor is off.
3. **Add Aggressive checkbox** — replaces the old L AGGR auton mode. Stored in config, passed to all auton routines.
4. **Add Push Alliance checkbox** — new option for autonomous behavior.
5. **Remove OPTIONS label** — checkboxes sit directly on the bottom row alongside SAVE CONFIG.

## New UI Layout (480 x 240)

```
 ┌──────────────────────────────────────────────────────┐
 │ AUTON MODE                                           │
 │ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ │  y≈8..68
 │ │   LEFT    │ │   RIGHT   │ │   SOLO    │ │  SKILLS   │ │
 │ └───────────┘ └───────────┘ └───────────┘ └───────────┘ │
 │                                                      │
 │ COLOR SENSOR                                         │
 │ [✓]    ┌──────────────┐    ┌──────────────┐          │  y≈78..118
 │        │     RED      │    │     BLUE     │          │
 │        └──────────────┘    └──────────────┘          │
 │                                                      │
 │ [✓] Aggressive  [✓] Push Alliance  ┌─────────────┐  │  y≈128..168
 │                                     │ SAVE CONFIG │  │
 │                                     └─────────────┘  │
 └──────────────────────────────────────────────────────┘
```

- When color sensor **unchecked**: RED/BLUE drawn dimmed (dark fill, no colored highlight, non-interactive)
- When color sensor **checked**: RED/BLUE active with full color fills
- Bottom row has no section header — just two checkboxes and the save button

## Data Model Changes

### AutonomousMode enum (robot_config_data.h)

```
Before:                          After:
ALLIANCE_LEFT_SAFE = 0           ALLIANCE_LEFT = 0
ALLIANCE_LEFT_AGGRESSIVE = 1     ALLIANCE_RIGHT = 1
ALLIANCE_RIGHT = 2               ALLIANCE_SOLO = 2
ALLIANCE_SOLO = 3                SKILLS = 3
SKILLS = 4
```

### AutonConfig struct

```cpp
struct AutonConfig {
  TeamColor teamColor = TeamColor::RED;
  bool useColorSensor = false;
  bool aggressive = false;      // NEW
  bool pushAlliance = false;    // NEW
};
```

### Config file format

```
auton=0
color=0
use_color_sensor=1
aggressive=1
push_alliance=0
```

Old files without `aggressive`/`push_alliance` fields default to `false`.

## Competition Dispatch Changes

```cpp
case AutonomousMode::ALLIANCE_LEFT:
  auton_alliance_left(autonConfig, autonConfig.aggressive, true);
  break;
```

The `pickBlocksUnderLongGoal` parameter to `auton_alliance_left()` now comes from `autonConfig.aggressive` instead of being a separate auton mode.

## Files Modified

| File | Change |
|------|--------|
| `include/robot_config_data.h` | Update enum, add fields to AutonConfig, update helpers |
| `src/config_ui.cpp` | Redesign layout, add checkboxes, combine color row |
| `src/competition.cpp` | Update dispatch, load new config fields |
