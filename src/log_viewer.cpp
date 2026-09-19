/**
 * @file log_viewer.cpp
 * @brief Log Viewer module implementation
 *
 * Touchscreen file browser and CSV log viewer for post-match analysis.
 * Reads log files from /usd/logs/ and displays them on the V5 brain screen.
 * Wrapped in log_viewer namespace for clean isolation from other modules.
 */

#include "log_viewer.h"
#include "pros/error.h"
#include "pros/misc.h"
#include "pros/rtos.hpp"
#include "pros/screen.hpp"

#include <cstdio>
#include <cstring>

// Text format aliases (macros don't work inside namespaces)
constexpr auto TEXT_SMALL = pros::E_TEXT_SMALL;
constexpr auto TEXT_MEDIUM = pros::E_TEXT_MEDIUM;

namespace log_viewer {

using namespace pros;

// ============================================================================
// UI Layout Constants (Screen: 480 x 240 pixels)
// ============================================================================

namespace UI {
constexpr int16_t SCREEN_WIDTH = 480;
constexpr int16_t SCREEN_HEIGHT = 240;

// Colors (matching config_ui palette)
constexpr uint32_t COLOR_BG = 0x000000;
constexpr uint32_t COLOR_TEXT = 0xFFFFFF;
constexpr uint32_t COLOR_TEXT_DARK = 0x000000;
constexpr uint32_t COLOR_HEADER_BG = 0x1A1A2E;
constexpr uint32_t COLOR_HEADER_TEXT = 0x00FFFF;
constexpr uint32_t COLOR_ROW_EVEN = 0x101010;
constexpr uint32_t COLOR_ROW_ODD = 0x1A1A1A;
constexpr uint32_t COLOR_BTN_BG = 0x303030;
constexpr uint32_t COLOR_BTN_BORDER = 0xFFFFFF;
constexpr uint32_t COLOR_COMMENT = 0x808080;
constexpr uint32_t COLOR_CSV_HEADER = 0x00FFFF;
constexpr uint32_t COLOR_SCROLLBAR = 0x606060;
constexpr uint32_t COLOR_SCROLLBAR_THUMB = 0x00FFFF;
constexpr uint32_t COLOR_ERROR = 0xFF0000;

// File Browser layout
namespace Browser {
constexpr int16_t TITLE_Y1 = 0;
constexpr int16_t TITLE_Y2 = 28;
constexpr int16_t TITLE_TEXT_X = 10;
constexpr int16_t TITLE_TEXT_Y = 6;

constexpr int16_t REFRESH_X1 = 390;
constexpr int16_t REFRESH_X2 = 470;
constexpr int16_t REFRESH_Y1 = 2;
constexpr int16_t REFRESH_Y2 = 26;

constexpr int16_t LIST_Y1 = 28;
constexpr int16_t ROW_HEIGHT = 30;
constexpr int16_t VISIBLE_ROWS = 6;
constexpr int16_t LIST_Y2 = LIST_Y1 + (ROW_HEIGHT * VISIBLE_ROWS); // 208

constexpr int16_t FILE_TEXT_X = 15;
constexpr int16_t FILE_TEXT_Y_OFF = 7;

constexpr int16_t NAV_Y1 = 208;
constexpr int16_t NAV_Y2 = 240;
constexpr int16_t UP_X1 = 340;
constexpr int16_t UP_X2 = 400;
constexpr int16_t DOWN_X1 = 410;
constexpr int16_t DOWN_X2 = 470;
constexpr int16_t PAGE_X = 10;
constexpr int16_t PAGE_Y = 216;
} // namespace Browser

// Log Content layout
namespace Content {
constexpr int16_t TITLE_Y1 = 0;
constexpr int16_t TITLE_Y2 = 28;

constexpr int16_t BACK_X1 = 5;
constexpr int16_t BACK_X2 = 75;
constexpr int16_t BACK_Y1 = 2;
constexpr int16_t BACK_Y2 = 26;

constexpr int16_t FILENAME_X = 85;
constexpr int16_t FILENAME_Y = 6;

constexpr int16_t TEXT_Y1 = 30;
constexpr int16_t LINE_HEIGHT = 20;
constexpr int16_t VISIBLE_LINES = 9;
constexpr int16_t TEXT_Y2 = TEXT_Y1 + (LINE_HEIGHT * VISIBLE_LINES); // 210
constexpr int16_t TEXT_X = 5;

constexpr int16_t NAV_Y1 = 210;
constexpr int16_t NAV_Y2 = 240;
constexpr int16_t UP_X1 = 340;
constexpr int16_t UP_X2 = 400;
constexpr int16_t DOWN_X1 = 410;
constexpr int16_t DOWN_X2 = 470;
constexpr int16_t LINE_INFO_X = 10;
constexpr int16_t LINE_INFO_Y = 218;

constexpr int16_t SCROLL_X1 = 472;
constexpr int16_t SCROLL_X2 = 479;
} // namespace Content
} // namespace UI

// ============================================================================
// State
// ============================================================================

enum class ViewerScreen { FILE_BROWSER, LOG_CONTENT };

static ViewerScreen currentScreen = ViewerScreen::FILE_BROWSER;

// File browser
static constexpr int MAX_FILES = 64;
static char fileNames[MAX_FILES][32];
static int fileCount = 0;
static int browserScroll = 0;

// Log content
static char selectedPath[64];
static char selectedName[32];

static constexpr int MAX_LINE_LEN = 60;
static constexpr int LINES_PER_PAGE = UI::Content::VISIBLE_LINES;
static char lineBuffer[LINES_PER_PAGE][MAX_LINE_LEN + 1];
static int linesInBuffer = 0;
static int currentTopLine = 0;
static int totalLines = 0;

// Line offset table for random-access seeking
static constexpr int MAX_LINES = 512;
static long lineOffsets[MAX_LINES];

// Touch
static uint32_t lastTouchTime = 0;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 200;

// ============================================================================
// Helpers
// ============================================================================

static bool inBounds(int16_t x, int16_t y, int16_t x1, int16_t y1,
                     int16_t x2, int16_t y2) {
  return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}

static void drawButton(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        const char *text) {
  screen::set_pen(UI::COLOR_BTN_BG);
  screen::fill_rect(x1, y1, x2, y2);
  screen::set_pen(UI::COLOR_BTN_BORDER);
  screen::draw_rect(x1, y1, x2, y2);
  screen::set_eraser(UI::COLOR_BTN_BG);
  screen::set_pen(UI::COLOR_TEXT);
  int16_t textW = strlen(text) * 8;
  int16_t textX = x1 + ((x2 - x1) - textW) / 2;
  int16_t textY = y1 + ((y2 - y1) / 2) - 6;
  screen::print(TEXT_MEDIUM, textX, textY, "%s", text);
  screen::set_eraser(UI::COLOR_BG);
}

// ============================================================================
// File Listing
// ============================================================================

static void loadFileList() {
  fileCount = 0;

  char listBuf[2048];
  memset(listBuf, 0, sizeof(listBuf));

  int32_t result =
      pros::c::usd_list_files("/logs", listBuf, sizeof(listBuf));
  if (result == PROS_ERR)
    return;

  // Parse newline-separated filenames
  char *tok = strtok(listBuf, "\n");
  while (tok && fileCount < MAX_FILES) {
    int len = strlen(tok);
    if (len > 4 && strcmp(tok + len - 4, ".csv") == 0) {
      strncpy(fileNames[fileCount], tok, 31);
      fileNames[fileCount][31] = '\0';
      fileCount++;
    }
    tok = strtok(nullptr, "\n");
  }

  // Reverse so most recent files appear first
  for (int i = 0; i < fileCount / 2; i++) {
    char temp[32];
    memcpy(temp, fileNames[i], 32);
    memcpy(fileNames[i], fileNames[fileCount - 1 - i], 32);
    memcpy(fileNames[fileCount - 1 - i], temp, 32);
  }

  browserScroll = 0;
}

// ============================================================================
// File Reading
// ============================================================================

static bool scanFileLines(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return false;

  totalLines = 0;
  char buf[128];

  while (totalLines < MAX_LINES) {
    lineOffsets[totalLines] = ftell(f);
    if (!fgets(buf, sizeof(buf), f))
      break;
    totalLines++;
  }

  fclose(f);
  return totalLines > 0;
}

static void loadPage(const char *path, int startLine) {
  FILE *f = fopen(path, "r");
  if (!f) {
    linesInBuffer = 0;
    return;
  }

  fseek(f, lineOffsets[startLine], SEEK_SET);
  linesInBuffer = 0;
  char raw[256];

  for (int i = 0; i < LINES_PER_PAGE && (startLine + i) < totalLines; i++) {
    if (!fgets(raw, sizeof(raw), f))
      break;

    // Strip trailing newline/carriage return
    int len = strlen(raw);
    while (len > 0 && (raw[len - 1] == '\n' || raw[len - 1] == '\r'))
      raw[--len] = '\0';

    strncpy(lineBuffer[i], raw, MAX_LINE_LEN);
    lineBuffer[i][MAX_LINE_LEN] = '\0';
    linesInBuffer++;
  }

  fclose(f);
}

// ============================================================================
// Drawing — File Browser
// ============================================================================

static void drawBrowserScreen() {
  screen::set_eraser(UI::COLOR_BG);
  screen::erase();

  // Title bar
  screen::set_pen(UI::COLOR_HEADER_BG);
  screen::fill_rect(0, UI::Browser::TITLE_Y1, UI::SCREEN_WIDTH,
                    UI::Browser::TITLE_Y2);
  screen::set_eraser(UI::COLOR_HEADER_BG);
  screen::set_pen(UI::COLOR_HEADER_TEXT);
  screen::print(TEXT_MEDIUM, UI::Browser::TITLE_TEXT_X,
                UI::Browser::TITLE_TEXT_Y, "LOG FILES (%d)", fileCount);
  screen::set_eraser(UI::COLOR_BG);

  // Refresh button
  drawButton(UI::Browser::REFRESH_X1, UI::Browser::REFRESH_Y1,
             UI::Browser::REFRESH_X2, UI::Browser::REFRESH_Y2, "REFRESH");

  // File rows
  for (int i = 0; i < UI::Browser::VISIBLE_ROWS; i++) {
    int fileIdx = browserScroll + i;
    int16_t rowY = UI::Browser::LIST_Y1 + (i * UI::Browser::ROW_HEIGHT);

    uint32_t rowColor =
        (i % 2 == 0) ? UI::COLOR_ROW_EVEN : UI::COLOR_ROW_ODD;
    screen::set_pen(rowColor);
    screen::fill_rect(0, rowY, UI::SCREEN_WIDTH,
                      rowY + UI::Browser::ROW_HEIGHT);

    if (fileIdx < fileCount) {
      screen::set_eraser(rowColor);
      screen::set_pen(UI::COLOR_TEXT);
      screen::print(TEXT_MEDIUM, UI::Browser::FILE_TEXT_X,
                    rowY + UI::Browser::FILE_TEXT_Y_OFF, "> %s",
                    fileNames[fileIdx]);
    }
  }
  screen::set_eraser(UI::COLOR_BG);

  // Empty state message
  if (fileCount == 0) {
    screen::set_pen(UI::COLOR_COMMENT);
    screen::print(TEXT_MEDIUM, 130, 110, "No log files found");
  }

  // Navigation bar — page info on left, buttons grouped on right
  if (fileCount > 0) {
    screen::set_pen(UI::COLOR_TEXT);
    int last = browserScroll + UI::Browser::VISIBLE_ROWS;
    if (last > fileCount)
      last = fileCount;
    screen::print(TEXT_SMALL, UI::Browser::PAGE_X, UI::Browser::PAGE_Y,
                  "%d-%d of %d", browserScroll + 1, last, fileCount);
  }
  if (browserScroll > 0) {
    drawButton(UI::Browser::UP_X1, UI::Browser::NAV_Y1, UI::Browser::UP_X2,
               UI::Browser::NAV_Y2, "UP");
  }
  if (browserScroll + UI::Browser::VISIBLE_ROWS < fileCount) {
    drawButton(UI::Browser::DOWN_X1, UI::Browser::NAV_Y1,
               UI::Browser::DOWN_X2, UI::Browser::NAV_Y2, "DN");
  }
}

// ============================================================================
// Drawing — Log Content
// ============================================================================

static void drawScrollbar() {
  if (totalLines <= LINES_PER_PAGE)
    return;

  int16_t trackH = UI::Content::TEXT_Y2 - UI::Content::TEXT_Y1;

  // Track
  screen::set_pen(UI::COLOR_SCROLLBAR);
  screen::fill_rect(UI::Content::SCROLL_X1, UI::Content::TEXT_Y1,
                    UI::Content::SCROLL_X2, UI::Content::TEXT_Y2);

  // Thumb
  float visFrac = (float)LINES_PER_PAGE / totalLines;
  float posFrac = (float)currentTopLine / (totalLines - LINES_PER_PAGE);

  int16_t thumbH = (int16_t)(trackH * visFrac);
  if (thumbH < 10)
    thumbH = 10;

  int16_t thumbY =
      UI::Content::TEXT_Y1 + (int16_t)((trackH - thumbH) * posFrac);

  screen::set_pen(UI::COLOR_SCROLLBAR_THUMB);
  screen::fill_rect(UI::Content::SCROLL_X1, thumbY, UI::Content::SCROLL_X2,
                    thumbY + thumbH);
}

static void drawContentScreen() {
  screen::set_eraser(UI::COLOR_BG);
  screen::erase();

  // Title bar
  screen::set_pen(UI::COLOR_HEADER_BG);
  screen::fill_rect(0, UI::Content::TITLE_Y1, UI::SCREEN_WIDTH,
                    UI::Content::TITLE_Y2);

  // Back button
  drawButton(UI::Content::BACK_X1, UI::Content::BACK_Y1, UI::Content::BACK_X2,
             UI::Content::BACK_Y2, "BACK");

  // Filename
  screen::set_eraser(UI::COLOR_HEADER_BG);
  screen::set_pen(UI::COLOR_HEADER_TEXT);
  screen::print(TEXT_MEDIUM, UI::Content::FILENAME_X, UI::Content::FILENAME_Y,
                "%s", selectedName);
  screen::set_eraser(UI::COLOR_BG);

  // Content lines
  for (int i = 0; i < linesInBuffer; i++) {
    int16_t lineY = UI::Content::TEXT_Y1 + (i * UI::Content::LINE_HEIGHT);

    // Color based on content type
    if (lineBuffer[i][0] == '#') {
      screen::set_pen(UI::COLOR_COMMENT);
    } else if (strncmp(lineBuffer[i], "elapsed,", 8) == 0) {
      screen::set_pen(UI::COLOR_CSV_HEADER);
    } else {
      screen::set_pen(UI::COLOR_TEXT);
    }

    screen::print(TEXT_SMALL, UI::Content::TEXT_X, lineY, "%s", lineBuffer[i]);
  }

  // Scrollbar
  drawScrollbar();

  // Navigation bar — line info on left, buttons grouped on right
  if (totalLines > 0) {
    screen::set_pen(UI::COLOR_TEXT);
    int lastLn = currentTopLine + linesInBuffer;
    screen::print(TEXT_SMALL, UI::Content::LINE_INFO_X,
                  UI::Content::LINE_INFO_Y, "Ln %d-%d of %d",
                  currentTopLine + 1, lastLn, totalLines);
  }
  if (currentTopLine > 0) {
    drawButton(UI::Content::UP_X1, UI::Content::NAV_Y1, UI::Content::UP_X2,
               UI::Content::NAV_Y2, "UP");
  }
  if (currentTopLine + LINES_PER_PAGE < totalLines) {
    drawButton(UI::Content::DOWN_X1, UI::Content::NAV_Y1,
               UI::Content::DOWN_X2, UI::Content::NAV_Y2, "DN");
  }
}

// ============================================================================
// File Open
// ============================================================================

static void openFile(int fileIdx) {
  snprintf(selectedPath, sizeof(selectedPath), "/usd/logs/%s",
           fileNames[fileIdx]);
  strncpy(selectedName, fileNames[fileIdx], sizeof(selectedName) - 1);
  selectedName[sizeof(selectedName) - 1] = '\0';

  if (!scanFileLines(selectedPath)) {
    screen::set_pen(UI::COLOR_ERROR);
    screen::print(TEXT_MEDIUM, 140, 120, "Cannot open file");
    delay(1000);
    drawBrowserScreen();
    return;
  }

  currentTopLine = 0;
  loadPage(selectedPath, 0);
  currentScreen = ViewerScreen::LOG_CONTENT;
  drawContentScreen();
}

// ============================================================================
// Touch Handling
// ============================================================================

static void handleBrowserTouch(int16_t x, int16_t y) {
  // Refresh button
  if (inBounds(x, y, UI::Browser::REFRESH_X1, UI::Browser::REFRESH_Y1,
               UI::Browser::REFRESH_X2, UI::Browser::REFRESH_Y2)) {
    loadFileList();
    drawBrowserScreen();
    return;
  }

  // File rows
  if (y >= UI::Browser::LIST_Y1 && y < UI::Browser::LIST_Y2) {
    int row = (y - UI::Browser::LIST_Y1) / UI::Browser::ROW_HEIGHT;
    int fileIdx = browserScroll + row;
    if (fileIdx < fileCount) {
      openFile(fileIdx);
    }
    return;
  }

  // Navigation
  if (y >= UI::Browser::NAV_Y1) {
    if (inBounds(x, y, UI::Browser::UP_X1, UI::Browser::NAV_Y1,
                 UI::Browser::UP_X2, UI::Browser::NAV_Y2)) {
      if (browserScroll > 0)
        browserScroll--;
      drawBrowserScreen();
    } else if (inBounds(x, y, UI::Browser::DOWN_X1, UI::Browser::NAV_Y1,
                        UI::Browser::DOWN_X2, UI::Browser::NAV_Y2)) {
      int maxScroll = fileCount - UI::Browser::VISIBLE_ROWS;
      if (maxScroll > 0 && browserScroll < maxScroll)
        browserScroll++;
      drawBrowserScreen();
    }
  }
}

static void handleContentTouch(int16_t x, int16_t y) {
  // Back button
  if (inBounds(x, y, UI::Content::BACK_X1, UI::Content::BACK_Y1,
               UI::Content::BACK_X2, UI::Content::BACK_Y2)) {
    currentScreen = ViewerScreen::FILE_BROWSER;
    drawBrowserScreen();
    return;
  }

  // Navigation buttons
  if (y >= UI::Content::NAV_Y1) {
    if (inBounds(x, y, UI::Content::UP_X1, UI::Content::NAV_Y1,
                 UI::Content::UP_X2, UI::Content::NAV_Y2)) {
      if (currentTopLine > 0)
        currentTopLine--;
      loadPage(selectedPath, currentTopLine);
      drawContentScreen();
    } else if (inBounds(x, y, UI::Content::DOWN_X1, UI::Content::NAV_Y1,
                        UI::Content::DOWN_X2, UI::Content::NAV_Y2)) {
      int maxTop = totalLines - LINES_PER_PAGE;
      if (maxTop > 0 && currentTopLine < maxTop)
        currentTopLine++;
      loadPage(selectedPath, currentTopLine);
      drawContentScreen();
    }
    return;
  }

  // Tap content area to page down
  if (y >= UI::Content::TEXT_Y1 && y < UI::Content::TEXT_Y2) {
    currentTopLine += LINES_PER_PAGE;
    int maxTop = totalLines - LINES_PER_PAGE;
    if (maxTop < 0)
      maxTop = 0;
    if (currentTopLine > maxTop)
      currentTopLine = maxTop;
    loadPage(selectedPath, currentTopLine);
    drawContentScreen();
  }
}

static void handleTouch(int16_t x, int16_t y) {
  if (currentScreen == ViewerScreen::FILE_BROWSER) {
    handleBrowserTouch(x, y);
  } else {
    handleContentTouch(x, y);
  }
}

// ============================================================================
// Public Entry Points (called by main.cpp dispatcher)
// ============================================================================

void initialize() {
  delay(50);
  loadFileList();
  drawBrowserScreen();
}

void disabled() {}

void competition_initialize() {}

void autonomous() {
  screen::set_pen(UI::COLOR_TEXT);
  screen::print(TEXT_MEDIUM, 100, 100, "Log Viewer - No Auton");
}

void opcontrol() {
  if (currentScreen == ViewerScreen::FILE_BROWSER) {
    drawBrowserScreen();
  } else {
    drawContentScreen();
  }

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

} // namespace log_viewer
