/**
 * @file config_ui.cpp
 * @brief Configuration UI module implementation
 *
 * Touchscreen interface for configuring robot settings before a match.
 * Wrapped in config_ui namespace for clean isolation from competition code.
 */

#include "config_ui.h"
#include "robot_config_data.h"
#include "pros/screen.hpp"
#include "pros/rtos.hpp"
#include <cstdio>
#include <cstring>

// Text format aliases (macros don't work inside namespaces)
constexpr auto TEXT_SMALL = pros::E_TEXT_SMALL;
constexpr auto TEXT_MEDIUM = pros::E_TEXT_MEDIUM;

namespace config_ui {

using namespace pros;

// ============================================================================
// UI Layout Constants (Screen: 480 x 240 pixels)
// ============================================================================

namespace UI {
  // Screen dimensions
  constexpr int16_t SCREEN_WIDTH = 480;
  constexpr int16_t SCREEN_HEIGHT = 240;

  // Colors
  constexpr uint32_t COLOR_BACKGROUND = 0x000000;        // Black
  constexpr uint32_t COLOR_TEXT = 0xFFFFFF;              // White
  constexpr uint32_t COLOR_TEXT_DARK = 0x000000;         // Black (for light backgrounds)
  constexpr uint32_t COLOR_TEXT_DIMMED = 0x606060;       // Dimmed text
  constexpr uint32_t COLOR_SECTION_LABEL = 0x00FFFF;     // Cyan for section headers
  constexpr uint32_t COLOR_BUTTON_BORDER = 0xFFFFFF;     // White outline
  constexpr uint32_t COLOR_BUTTON_UNSELECTED = 0x303030; // Dark gray fill for unselected
  constexpr uint32_t COLOR_BUTTON_DIMMED = 0x1A1A1A;     // Very dark fill for disabled
  constexpr uint32_t COLOR_BORDER_DIMMED = 0x404040;     // Dimmed border
  constexpr uint32_t COLOR_RED_SELECTED = 0xFF0000;      // Red fill
  constexpr uint32_t COLOR_BLUE_SELECTED = 0x0080FF;     // Bright blue fill
  constexpr uint32_t COLOR_RED_DIMMED = 0x600000;        // Dimmed red
  constexpr uint32_t COLOR_BLUE_DIMMED = 0x003060;       // Dimmed blue
  constexpr uint32_t COLOR_SAVE_BUTTON = 0x00DD00;       // Green
  constexpr uint32_t COLOR_HIGHLIGHT = 0xFFFF00;         // Yellow (auton selection)
  constexpr uint32_t COLOR_CHECKBOX_ON = 0x00DD00;       // Green
  constexpr uint32_t COLOR_CHECKBOX_OFF = 0x404040;      // Dark gray

  // Auton Section (4 buttons)
  namespace Auton {
    constexpr int16_t LABEL_Y = 8;
    constexpr int16_t BTN_Y1 = 28;
    constexpr int16_t BTN_Y2 = 68;
    constexpr int16_t BTN_WIDTH = 111;
    constexpr int16_t BTN_GAP = 5;

    // 4 buttons across: 10..121, 126..237, 242..353, 358..469
    constexpr int16_t LEFT_X1 = 10;
    constexpr int16_t LEFT_X2 = LEFT_X1 + BTN_WIDTH;
    constexpr int16_t RIGHT_X1 = LEFT_X2 + BTN_GAP;
    constexpr int16_t RIGHT_X2 = RIGHT_X1 + BTN_WIDTH;
    constexpr int16_t SOLO_X1 = RIGHT_X2 + BTN_GAP;
    constexpr int16_t SOLO_X2 = SOLO_X1 + BTN_WIDTH;
    constexpr int16_t SKILLS_X1 = SOLO_X2 + BTN_GAP;
    constexpr int16_t SKILLS_X2 = SKILLS_X1 + BTN_WIDTH;
  }

  // Color Sensor Section (checkbox + RED/BLUE inline)
  namespace ColorSensor {
    constexpr int16_t LABEL_Y = 78;
    constexpr int16_t ROW_Y1 = 96;
    constexpr int16_t ROW_Y2 = 128;

    // Checkbox (30x30, vertically centered in row)
    constexpr int16_t CB_X1 = 10;
    constexpr int16_t CB_X2 = 40;
    constexpr int16_t CB_Y1 = 98;
    constexpr int16_t CB_Y2 = 128;

    // RED/BLUE buttons inline after checkbox
    constexpr int16_t RED_X1 = 55;
    constexpr int16_t RED_X2 = 215;
    constexpr int16_t BLUE_X1 = 225;
    constexpr int16_t BLUE_X2 = 385;
    constexpr int16_t BTN_Y1 = 96;
    constexpr int16_t BTN_Y2 = 128;
  }

  // Bottom Row (checkboxes + save button, no section header)
  namespace Bottom {
    constexpr int16_t ROW_Y1 = 142;
    constexpr int16_t ROW_Y2 = 175;

    // Aggressive checkbox
    constexpr int16_t AGGR_CB_X1 = 10;
    constexpr int16_t AGGR_CB_X2 = 40;
    constexpr int16_t AGGR_CB_Y1 = 145;
    constexpr int16_t AGGR_CB_Y2 = 175;
    constexpr int16_t AGGR_LABEL_X = 48;
    constexpr int16_t AGGR_LABEL_Y = 153;

    // Push Alliance checkbox
    constexpr int16_t PUSH_CB_X1 = 160;
    constexpr int16_t PUSH_CB_X2 = 190;
    constexpr int16_t PUSH_CB_Y1 = 145;
    constexpr int16_t PUSH_CB_Y2 = 175;
    constexpr int16_t PUSH_LABEL_X = 198;
    constexpr int16_t PUSH_LABEL_Y = 153;
  }

  // Save Button
  namespace Save {
    constexpr int16_t X1 = 330;
    constexpr int16_t X2 = 470;
    constexpr int16_t Y1 = 142;
    constexpr int16_t Y2 = 175;
  }
}

// ============================================================================
// Module State
// ============================================================================

// Current configuration (in-memory)
static AutonomousMode currentAuton = ConfigDefaults::AUTON_MODE;
static TeamColor currentColor = ConfigDefaults::TEAM_COLOR;
static bool useColorSensor = ConfigDefaults::USE_COLOR_SENSOR;
static bool aggressive = ConfigDefaults::AGGRESSIVE;
static bool pushAlliance = ConfigDefaults::PUSH_ALLIANCE;
static bool arcade_mode = ConfigDefaults::ARCADE_MODE;
static bool tank_mode = ConfigDefaults::TANK_MODE;

// Touch debounce tracking
static uint32_t lastTouchTime = 0;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 200;

// ============================================================================
// Helper Functions
// ============================================================================

static bool inBounds(int16_t x, int16_t y, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}

static void drawButton(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                       const char* text, bool selected,
                       uint32_t selectedColor) {
  if (selected) {
    // Selected: filled with color, white border, dark text
    screen::set_pen(selectedColor);
    screen::fill_rect(x1, y1, x2, y2);
    screen::set_pen(UI::COLOR_TEXT);
    screen::draw_rect(x1, y1, x2, y2);
    // Set eraser to match background so text doesn't have black box
    screen::set_eraser(selectedColor);
    screen::set_pen(UI::COLOR_TEXT_DARK);
  } else {
    // Unselected: dark fill, white border, white text
    screen::set_pen(UI::COLOR_BUTTON_UNSELECTED);
    screen::fill_rect(x1, y1, x2, y2);
    screen::set_pen(UI::COLOR_BUTTON_BORDER);
    screen::draw_rect(x1, y1, x2, y2);
    // Set eraser to match background
    screen::set_eraser(UI::COLOR_BUTTON_UNSELECTED);
    screen::set_pen(UI::COLOR_TEXT);
  }

  // Center text in button
  int16_t textWidth = strlen(text) * 8; // Approximate width
  int16_t textX = x1 + ((x2 - x1) - textWidth) / 2;
  int16_t textY = y1 + ((y2 - y1) / 2) - 6;
  screen::print(TEXT_MEDIUM, textX, textY, "%s", text);

  // Reset eraser to default background
  screen::set_eraser(UI::COLOR_BACKGROUND);
}

static void drawDimmedButton(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                             const char* text, bool selected,
                             uint32_t dimmedColor) {
  // Dimmed: dark fill with faint color hint, muted border, dimmed text
  if (selected) {
    screen::set_pen(dimmedColor);
  } else {
    screen::set_pen(UI::COLOR_BUTTON_DIMMED);
  }
  screen::fill_rect(x1, y1, x2, y2);
  screen::set_pen(UI::COLOR_BORDER_DIMMED);
  screen::draw_rect(x1, y1, x2, y2);

  screen::set_eraser(selected ? dimmedColor : UI::COLOR_BUTTON_DIMMED);
  screen::set_pen(UI::COLOR_TEXT_DIMMED);

  int16_t textWidth = strlen(text) * 8;
  int16_t textX = x1 + ((x2 - x1) - textWidth) / 2;
  int16_t textY = y1 + ((y2 - y1) / 2) - 6;
  screen::print(TEXT_MEDIUM, textX, textY, "%s", text);

  screen::set_eraser(UI::COLOR_BACKGROUND);
}

static void drawCheckbox(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool checked) {
  if (checked) {
    screen::set_pen(UI::COLOR_CHECKBOX_ON);
    screen::fill_rect(x1, y1, x2, y2);
    // Draw checkmark
    screen::set_pen(UI::COLOR_TEXT);
    screen::draw_line(x1 + 5, y1 + 15, x1 + 12, y2 - 5);
    screen::draw_line(x1 + 12, y2 - 5, x2 - 3, y1 + 5);
  } else {
    screen::set_pen(UI::COLOR_BUTTON_UNSELECTED);
    screen::fill_rect(x1, y1, x2, y2);
    screen::set_pen(UI::COLOR_CHECKBOX_OFF);
    screen::draw_rect(x1, y1, x2, y2);
  }
}

// ============================================================================
// Drawing Functions
// ============================================================================

static void drawSectionLabel(int16_t x, int16_t y, const char* text) {
  screen::set_pen(UI::COLOR_SECTION_LABEL);
  screen::print(TEXT_MEDIUM, x, y, "%s", text);
}

static void drawAutonSection() {
  screen::set_pen(UI::COLOR_BACKGROUND);
  screen::fill_rect(0, UI::Auton::LABEL_Y - 2, UI::SCREEN_WIDTH, UI::Auton::BTN_Y2 + 2);

  drawSectionLabel(10, UI::Auton::LABEL_Y, "AUTON MODE");

  drawButton(UI::Auton::LEFT_X1, UI::Auton::BTN_Y1,
             UI::Auton::LEFT_X2, UI::Auton::BTN_Y2,
             "LEFT",
             currentAuton == AutonomousMode::ALLIANCE_LEFT,
             UI::COLOR_HIGHLIGHT);

  drawButton(UI::Auton::RIGHT_X1, UI::Auton::BTN_Y1,
             UI::Auton::RIGHT_X2, UI::Auton::BTN_Y2,
             "RIGHT",
             currentAuton == AutonomousMode::ALLIANCE_RIGHT,
             UI::COLOR_HIGHLIGHT);

  drawButton(UI::Auton::SOLO_X1, UI::Auton::BTN_Y1,
             UI::Auton::SOLO_X2, UI::Auton::BTN_Y2,
             "SOLO",
             currentAuton == AutonomousMode::ALLIANCE_SOLO,
             UI::COLOR_HIGHLIGHT);

  drawButton(UI::Auton::SKILLS_X1, UI::Auton::BTN_Y1,
             UI::Auton::SKILLS_X2, UI::Auton::BTN_Y2,
             "SKILLS",
             currentAuton == AutonomousMode::SKILLS,
             UI::COLOR_HIGHLIGHT);
}

static void drawColorSensorSection() {
  screen::set_pen(UI::COLOR_BACKGROUND);
  screen::fill_rect(0, UI::ColorSensor::LABEL_Y - 2, UI::SCREEN_WIDTH, UI::ColorSensor::ROW_Y2 + 2);

  drawSectionLabel(10, UI::ColorSensor::LABEL_Y, "COLOR SENSOR");

  // Checkbox
  drawCheckbox(UI::ColorSensor::CB_X1, UI::ColorSensor::CB_Y1,
               UI::ColorSensor::CB_X2, UI::ColorSensor::CB_Y2,
               useColorSensor);

  // RED/BLUE buttons — dimmed when color sensor is off
  if (useColorSensor) {
    drawButton(UI::ColorSensor::RED_X1, UI::ColorSensor::BTN_Y1,
               UI::ColorSensor::RED_X2, UI::ColorSensor::BTN_Y2,
               "RED",
               currentColor == TeamColor::RED,
               UI::COLOR_RED_SELECTED);

    drawButton(UI::ColorSensor::BLUE_X1, UI::ColorSensor::BTN_Y1,
               UI::ColorSensor::BLUE_X2, UI::ColorSensor::BTN_Y2,
               "BLUE",
               currentColor == TeamColor::BLUE,
               UI::COLOR_BLUE_SELECTED);
  } else {
    drawDimmedButton(UI::ColorSensor::RED_X1, UI::ColorSensor::BTN_Y1,
                     UI::ColorSensor::RED_X2, UI::ColorSensor::BTN_Y2,
                     "RED",
                     currentColor == TeamColor::RED,
                     UI::COLOR_RED_DIMMED);

    drawDimmedButton(UI::ColorSensor::BLUE_X1, UI::ColorSensor::BTN_Y1,
                     UI::ColorSensor::BLUE_X2, UI::ColorSensor::BTN_Y2,
                     "BLUE",
                     currentColor == TeamColor::BLUE,
                     UI::COLOR_BLUE_DIMMED);
  }
}

static void drawBottomRow() {
  screen::set_pen(UI::COLOR_BACKGROUND);
  screen::fill_rect(0, UI::Bottom::ROW_Y1 - 2, UI::Save::X1 - 10, UI::Bottom::ROW_Y2 + 2);

  // Aggressive checkbox
  drawCheckbox(UI::Bottom::AGGR_CB_X1, UI::Bottom::AGGR_CB_Y1,
               UI::Bottom::AGGR_CB_X2, UI::Bottom::AGGR_CB_Y2,
               tank_mode);
  screen::set_pen(UI::COLOR_TEXT);
  screen::print(TEXT_MEDIUM, UI::Bottom::AGGR_LABEL_X, UI::Bottom::AGGR_LABEL_Y,
  //              "Aggressive");
       "Tank");

  // Push Alliance checkbox
  drawCheckbox(UI::Bottom::PUSH_CB_X1, UI::Bottom::PUSH_CB_Y1,
               UI::Bottom::PUSH_CB_X2, UI::Bottom::PUSH_CB_Y2,
               arcade_mode);
  screen::set_pen(UI::COLOR_TEXT);
  screen::print(TEXT_MEDIUM, UI::Bottom::PUSH_LABEL_X, UI::Bottom::PUSH_LABEL_Y,
//                "Push Alliance");
"Arcade");
}

static void drawSaveButton() {
  screen::set_pen(UI::COLOR_SAVE_BUTTON);
  screen::fill_rect(UI::Save::X1, UI::Save::Y1, UI::Save::X2, UI::Save::Y2);
  screen::set_pen(UI::COLOR_TEXT);
  screen::draw_rect(UI::Save::X1, UI::Save::Y1, UI::Save::X2, UI::Save::Y2);
  // Set eraser to match button background for clean text
  screen::set_eraser(UI::COLOR_SAVE_BUTTON);
  screen::set_pen(UI::COLOR_TEXT_DARK);
  screen::print(TEXT_MEDIUM, UI::Save::X1 + 20, UI::Save::Y1 + 8, "SAVE CONFIG");
  screen::set_eraser(UI::COLOR_BACKGROUND);
}

static void drawUI() {
  screen::set_eraser(UI::COLOR_BACKGROUND);
  screen::erase();
  drawAutonSection();
  drawColorSensorSection();
  drawBottomRow();
  drawSaveButton();
}

// ============================================================================
// Configuration File Functions
// ============================================================================

static bool saveConfig() {
  FILE* file = fopen(CONFIG_FILE_PATH, "w");
  if (!file) {
    return false;
  }

  fprintf(file, "auton=%d\n", static_cast<int>(currentAuton));
  fprintf(file, "color=%d\n", static_cast<int>(currentColor));
  fprintf(file, "use_color_sensor=%d\n", useColorSensor ? 1 : 0);
//  fprintf(file, "aggressive=%d\n", aggressive ? 1 : 0);
//  fprintf(file, "push_alliance=%d\n", pushAlliance ? 1 : 0);
  fprintf(file, "Tank=%d\n", tank_mode ? 1 : 0);
  fprintf(file, "Arcade=%d\n", arcade_mode ? 1 : 0);
  fclose(file);
  return true;
}

static bool loadConfig() {
  // Try to open config file - if SD card isn't present or file doesn't exist,
  // fopen will fail and we'll use defaults
  FILE* file = fopen(CONFIG_FILE_PATH, "r");
  if (!file) {
    return false;
  }

  int auton = -1, color = -1, colorSensor = -1, aggr = -1, push = -1;
  char line[64];

  while (fgets(line, sizeof(line), file)) {
    sscanf(line, "auton=%d", &auton);
    sscanf(line, "color=%d", &color);
    sscanf(line, "use_color_sensor=%d", &colorSensor);
    sscanf(line, "tank_mode=%d", &aggr);
    sscanf(line, "arcade_mode=%d", &push);
  }
  fclose(file);

  if (isValidAutonMode(auton)) {
    currentAuton = static_cast<AutonomousMode>(auton);
  }
  if (isValidTeamColor(color)) {
    currentColor = static_cast<TeamColor>(color);
  }
  if (colorSensor == 0 || colorSensor == 1) {
    useColorSensor = (colorSensor == 1);
  }
  if (aggr == 0 || aggr == 1) {
    tank_mode = (aggr == 1);
  }
  if (push == 0 || push == 1) {
    arcade_mode = (push == 1);
  }

  return true;
}

static void showSaveConfirmation(bool success) {
  int16_t msgX1 = 90, msgY1 = 50, msgX2 = 390, msgY2 = 150;

  uint32_t bgColor = success ? 0x006600 : 0x660000; // Dark green or dark red
  screen::set_pen(bgColor);
  screen::fill_rect(msgX1, msgY1, msgX2, msgY2);

  screen::set_pen(UI::COLOR_TEXT);
  screen::draw_rect(msgX1, msgY1, msgX2, msgY2);

  // Set eraser to match dialog background for clean text
  screen::set_eraser(bgColor);

  if (success) {
    screen::print(TEXT_MEDIUM, msgX1 + 75, msgY1 + 10, "CONFIG SAVED!");
    screen::print(TEXT_MEDIUM, msgX1 + 20, msgY1 + 32, "Auton: %s  Aggr: %s",
                  getAutonName(currentAuton), tank_mode ? "ON" : "OFF");
    screen::print(TEXT_MEDIUM, msgX1 + 20, msgY1 + 52, "Color: %s  Sensor: %s",
                  getColorName(currentColor), useColorSensor ? "ON" : "OFF");
    screen::print(TEXT_MEDIUM, msgX1 + 20, msgY1 + 72, "Push Alliance: %s",
                  arcade_mode ? "ON" : "OFF");
  } else {
    screen::print(TEXT_MEDIUM, msgX1 + 70, msgY1 + 25, "SAVE FAILED!");
    screen::print(TEXT_MEDIUM, msgX1 + 55, msgY1 + 50, "Check SD card");
  }

  screen::set_eraser(UI::COLOR_BACKGROUND);
  delay(1500);
  drawUI();
}

// ============================================================================
// Touch Handling
// ============================================================================

static void handleTouch(int16_t x, int16_t y) {
  bool sectionChanged = false;

  // Check auton buttons
  if (y >= UI::Auton::BTN_Y1 && y <= UI::Auton::BTN_Y2) {
    if (inBounds(x, y, UI::Auton::LEFT_X1, UI::Auton::BTN_Y1,
                 UI::Auton::LEFT_X2, UI::Auton::BTN_Y2)) {
      currentAuton = AutonomousMode::ALLIANCE_LEFT;
      sectionChanged = true;
    } else if (inBounds(x, y, UI::Auton::RIGHT_X1, UI::Auton::BTN_Y1,
                        UI::Auton::RIGHT_X2, UI::Auton::BTN_Y2)) {
      currentAuton = AutonomousMode::ALLIANCE_RIGHT;
      sectionChanged = true;
    } else if (inBounds(x, y, UI::Auton::SOLO_X1, UI::Auton::BTN_Y1,
                        UI::Auton::SOLO_X2, UI::Auton::BTN_Y2)) {
      currentAuton = AutonomousMode::ALLIANCE_SOLO;
      sectionChanged = true;
    } else if (inBounds(x, y, UI::Auton::SKILLS_X1, UI::Auton::BTN_Y1,
                        UI::Auton::SKILLS_X2, UI::Auton::BTN_Y2)) {
      currentAuton = AutonomousMode::SKILLS;
      sectionChanged = true;
    }

    if (sectionChanged) {
      drawAutonSection();
    }
  }

  // Check color sensor section
  if (y >= UI::ColorSensor::ROW_Y1 && y <= UI::ColorSensor::ROW_Y2) {
    // Checkbox (include some padding for easier touch)
    if (inBounds(x, y, UI::ColorSensor::CB_X1, UI::ColorSensor::CB_Y1,
                 UI::ColorSensor::CB_X2 + 5, UI::ColorSensor::CB_Y2)) {
      useColorSensor = !useColorSensor;
      drawColorSensorSection();
    }
    // RED/BLUE buttons — only respond when color sensor is enabled
    else if (useColorSensor) {
      if (inBounds(x, y, UI::ColorSensor::RED_X1, UI::ColorSensor::BTN_Y1,
                   UI::ColorSensor::RED_X2, UI::ColorSensor::BTN_Y2)) {
        currentColor = TeamColor::RED;
        drawColorSensorSection();
      } else if (inBounds(x, y, UI::ColorSensor::BLUE_X1, UI::ColorSensor::BTN_Y1,
                          UI::ColorSensor::BLUE_X2, UI::ColorSensor::BTN_Y2)) {
        currentColor = TeamColor::BLUE;
        drawColorSensorSection();
      }
    }
  }

  // Check bottom row checkboxes
  if (y >= UI::Bottom::ROW_Y1 && y <= UI::Bottom::ROW_Y2) {
    // Aggressive checkbox (include label area for easier touch)
    if (inBounds(x, y, UI::Bottom::AGGR_CB_X1, UI::Bottom::AGGR_CB_Y1,
                 UI::Bottom::AGGR_LABEL_X + 90, UI::Bottom::AGGR_CB_Y2)) {
      aggressive = !aggressive;
      drawBottomRow();
    }
    // Push Alliance checkbox (include label area)
    else if (inBounds(x, y, UI::Bottom::PUSH_CB_X1, UI::Bottom::PUSH_CB_Y1,
                      UI::Bottom::PUSH_LABEL_X + 120, UI::Bottom::PUSH_CB_Y2)) {
      pushAlliance = !pushAlliance;
      drawBottomRow();
    }
  }

  // Check save button
  if (inBounds(x, y, UI::Save::X1, UI::Save::Y1, UI::Save::X2, UI::Save::Y2)) {
    // Visual feedback - invert colors briefly
    screen::set_pen(UI::COLOR_TEXT);
    screen::fill_rect(UI::Save::X1, UI::Save::Y1, UI::Save::X2, UI::Save::Y2);
    screen::set_eraser(UI::COLOR_TEXT);
    screen::set_pen(UI::COLOR_TEXT_DARK);
    screen::print(TEXT_MEDIUM, UI::Save::X1 + 20, UI::Save::Y1 + 8, "SAVE CONFIG");
    screen::set_eraser(UI::COLOR_BACKGROUND);
    delay(100);

    bool success = saveConfig();
    showSaveConfirmation(success);
  }
}

// ============================================================================
// Public Entry Points (called by main.cpp dispatcher)
// ============================================================================

void initialize() {
  // Small delay to ensure screen is ready
  delay(50);
  loadConfig();
  drawUI();
}

void disabled() {
  // Ensure UI is visible during disabled state
  drawUI();
}

void competition_initialize() {
  // Nothing to do for config UI
}

void autonomous() {
  screen::set_pen(UI::COLOR_TEXT);
  screen::print(TEXT_MEDIUM, 100, 100, "Config UI - No Auton");
}

void opcontrol() {
  // Redraw UI when opcontrol starts (screen may have been cleared between phases)
  drawUI();

  while (true) {
    screen_touch_status_s_t status = screen::touch_status();

    if (status.touch_status == E_TOUCH_PRESSED) {
      uint32_t now = millis();
      if (now - lastTouchTime >= TOUCH_DEBOUNCE_MS) {
        lastTouchTime = now;
        handleTouch(status.x, status.y);
      }
    }

    delay(20);
  }
}

} // namespace config_ui
