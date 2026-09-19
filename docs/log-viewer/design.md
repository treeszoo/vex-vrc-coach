# Log Viewer — Design Document

## Overview

Touchscreen application for the V5 brain that lets users browse and read CSV log files directly on the robot after a match. Eliminates the need to remove the SD card and transfer files to a computer for quick post-match analysis.

### Goals

- **Immediate access**: View logs on the brain screen seconds after a match ends
- **No SD card removal**: Browse and read files without physically accessing the card
- **Familiar UI**: Same visual style and interaction patterns as the existing Config UI
- **Zero interference**: Runs as a separate program slot — never touches competition code
- **Memory efficient**: Never loads entire files into memory — reads pages on demand

### Non-Goals

- Editing or deleting log files (read-only viewer)
- Graphical plotting of pose data (CSV text display only)
- Filtering or searching within files (full analysis is done on a computer)
- Real-time log streaming during a match (that's the file logger's job)

## Architecture

Two-screen state machine with touch-driven navigation. The viewer is a standalone `AppMode` dispatched by `main.cpp`, identical in structure to `CONFIG_UI` and `COMPETITION`.

```
                    ┌─────────────────┐
                    │  FILE BROWSER   │
                    │                 │
       ┌───────────│  /usd/logs/*.csv │
       │  tap file │  (6 visible)    │
       │           └────────┬────────┘
       │                    │ REFRESH / UP / DOWN
       │                    └──────────────────┐
       v                                       │
┌──────────────┐                               │
│  LOG CONTENT │                               │
│              │───── BACK ────────────────────>│
│  (9 lines)   │                               │
│  scrollable  │                               │
└──────────────┘                               │
       │                                       │
       │  UP / DOWN / tap content              │
       └───────────────────────────────────────┘
```

### AppMode Integration

The log viewer is the third compile-time application mode, following the same `if constexpr` dispatch pattern established by `CONFIG_UI`:

```cpp
enum class AppMode {
  COMPETITION, // For matches (upload to slot 4)
  CONFIG_UI,   // For pre-match setup (upload to slot 1)
  LOG_VIEWER   // For post-match logs (upload to slot 2)
};
```

Each PROS callback (`initialize`, `disabled`, `competition_initialize`, `autonomous`, `opcontrol`) routes to the corresponding `log_viewer::` function when `APP_MODE == AppMode::LOG_VIEWER`. The compiler eliminates all unused module code — zero runtime overhead, no dead code in the binary.

### Deployment

To use the log viewer:
1. Change `APP_MODE = AppMode::LOG_VIEWER` in `src/main.cpp`
2. Build and upload to a separate V5 brain program slot (e.g., slot 2)
3. Switch to that slot on the brain after a match to view logs

The competition program in slot 4 is completely unaffected.

## State Machine

```cpp
enum class ViewerScreen {
  FILE_BROWSER,  // List of log files, tap to open
  LOG_CONTENT    // Viewing a specific file's contents
};
```

| Current Screen | User Action | Next Screen | Side Effect |
|---|---|---|---|
| `FILE_BROWSER` | Tap a file row | `LOG_CONTENT` | Scan line offsets, load first page |
| `FILE_BROWSER` | Tap REFRESH | `FILE_BROWSER` | Re-scan `/usd/logs/` directory |
| `FILE_BROWSER` | Tap UP/DOWN | `FILE_BROWSER` | Scroll file list by 6 rows |
| `LOG_CONTENT` | Tap BACK | `FILE_BROWSER` | Return to file list |
| `LOG_CONTENT` | Tap UP/DOWN | `LOG_CONTENT` | Scroll by 9 lines, reload page |
| `LOG_CONTENT` | Tap content area | `LOG_CONTENT` | Page down by 9 lines |

## Data Structures

All state is file-scoped (`static`) within `log_viewer.cpp`, inside the `log_viewer` namespace. Nothing leaks into the header.

### File Browser State

```cpp
static constexpr int MAX_FILES = 64;
static char fileNames[MAX_FILES][32];    // Parsed filenames (name only, not path)
static int fileCount = 0;                // Number of .csv files found
static int browserScroll = 0;            // Index of first visible file
```

Filenames are stored as fixed 32-byte strings. With names like `left_safe_0304_1430.csv` (24 chars), 32 bytes is sufficient. The list holds up to 64 files — enough for a full competition day.

**Memory**: 64 x 32 = **2,048 bytes**

### Log Content State

```cpp
static char selectedPath[64];            // Full path: "/usd/logs/<name>.csv"
static char selectedName[32];            // Just the filename for title display

static constexpr int MAX_LINE_LEN = 60;  // Lines truncated to fit 480px width
static constexpr int LINES_PER_PAGE = 9; // Lines visible on screen at once
static char lineBuffer[LINES_PER_PAGE][MAX_LINE_LEN + 1];  // Current page of text
static int linesInBuffer = 0;            // Lines loaded in current page
static int currentTopLine = 0;           // Line index of first displayed line
static int totalLines = 0;              // Total lines in file
```

Only 9 lines are in memory at any time. Lines longer than 60 characters are truncated for display — the full data remains on the SD card for computer analysis.

**Memory**: 9 x 61 = **549 bytes**

### Line Offset Table

```cpp
static constexpr int MAX_LINES = 512;
static long lineOffsets[MAX_LINES];      // ftell() position of each line start
```

This is the key to efficient random-access file reading. When a file is opened, we scan it once and record the byte offset of each line start. Scrolling then uses `fseek()` to jump directly to any line without re-reading from the beginning.

**Memory**: 512 x 4 = **2,048 bytes**

### Touch State

```cpp
static uint32_t lastTouchTime = 0;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 200;
```

Matches the 200ms debounce used in `config_ui.cpp`.

## Memory Budget

| Component | Size |
|---|---|
| File name array (64 x 32 bytes) | 2,048 bytes |
| Line offset table (512 x 4 bytes) | 2,048 bytes |
| Line buffer (9 x 61 bytes) | 549 bytes |
| Paths, names, state variables | ~200 bytes |
| `usd_list_files` temporary buffer | 2,048 bytes (stack, freed after `loadFileList`) |
| **Total** | **~7 KB** |

Well within the V5 brain's 512KB user memory. The log viewer has no background tasks, no double-buffers, and no persistent file handles.

## Directory Listing

### `usd_list_files` API

File listing uses the PROS `usd_list_files()` function (declared in `pros/misc.h`):

```cpp
int32_t pros::c::usd_list_files(const char* path, char* buffer, int32_t len);
```

- **Path convention**: `/logs` (NOT `/usd/logs/` — the `/usd/` prefix is implied)
- **Output**: Newline-separated filenames written to `buffer`
- **Return**: `1` on success, `PROS_ERR` on failure
- **Limit**: Only files that fit in the buffer are returned; overflow is silently truncated

### Parsing and Sorting

```cpp
static void loadFileList() {
    char listBuf[2048];
    pros::c::usd_list_files("/logs", listBuf, sizeof(listBuf));

    // Parse: strtok on '\n', filter for ".csv" suffix
    // Reverse: most recent files first (filesystem returns creation order)
}
```

The 2,048-byte buffer can hold approximately 80 filenames (at ~25 chars each). Files are reversed so the most recent log appears at the top of the list — the file you most likely want to view right after a match.

Only `.csv` files are shown. Any non-CSV files in `/usd/logs/` are silently ignored.

## File Reading Strategy

### Line-Offset Table Approach

Rather than loading entire files into memory (risky for large files) or re-reading from the beginning on every scroll, we use a **line-offset table** for O(1) random access:

```
Step 1: Scan               Step 2: Display           Step 3: Scroll

fgets() line 0 ──> offset[0] = 0       fseek(offset[0])    fseek(offset[9])
fgets() line 1 ──> offset[1] = 47      fgets() x 9 lines   fgets() x 9 lines
fgets() line 2 ──> offset[2] = 89      ──> lineBuffer[]     ──> lineBuffer[]
...                                     ──> drawContentScreen ──> drawContentScreen
fgets() line N ──> offset[N] = ...
```

#### Phase 1: Scan (`scanFileLines`)

On file open, read through the entire file with `fgets()`, recording `ftell()` at each line start:

```cpp
while (totalLines < MAX_LINES) {
    lineOffsets[totalLines] = ftell(f);
    if (!fgets(buf, sizeof(buf), f)) break;
    totalLines++;
}
```

This is fast — it reads sequentially and discards the content. For a typical 50-line log file, this takes < 1ms.

#### Phase 2: Load Page (`loadPage`)

To display a page starting at line N:

```cpp
fseek(f, lineOffsets[startLine], SEEK_SET);
for (int i = 0; i < LINES_PER_PAGE; i++) {
    fgets(raw, sizeof(raw), f);
    // Strip newlines, truncate to MAX_LINE_LEN
    strncpy(lineBuffer[i], raw, MAX_LINE_LEN);
}
```

The file is opened, read, and closed for each page load. No file handles are kept open between touch events.

#### Phase 3: Scroll

UP/DOWN buttons change `currentTopLine` by `LINES_PER_PAGE` (9 lines), then call `loadPage()` + `drawContentScreen()`. Scrolling is instant because `fseek()` jumps directly to the target line.

### Limits

| Parameter | Value | Rationale |
|---|---|---|
| Max lines per file | 512 | Sufficient for any autonomous log (typically 10-50 lines). Offset table costs 2KB. |
| Max display line length | 60 chars | Fits the 480px screen width with `TEXT_SMALL`. Longer lines are truncated. |
| Max files listed | 64 | Covers a full competition day. 2KB for filenames. |
| Read buffer per line | 256 bytes | Reads full lines from disk even if display is truncated. |

## Screen Layouts

### Screen Hardware

- **Resolution**: 480 x 240 pixels
- **Touch**: Single-point capacitive, polled via `pros::screen::touch_status()`
- **Drawing API**: `pros::screen::` namespace (fill_rect, draw_rect, print, erase)
- **Text sizes**: `TEXT_MEDIUM` (~8px wide, 16px tall), `TEXT_SMALL` (~7px wide, 12px tall)

### File Browser Screen

```
 0 ┌──────────────────────────────────────────────────┐
   │ LOG FILES (12)                        [REFRESH]  │ <- Title bar (dark navy bg)
28 ├──────────────────────────────────────────────────┤
   │ > skills_0305_1520.csv                           │ <- Row 0 (dark gray)
58 │ > left_aggr_0305_1000.csv                        │ <- Row 1 (slightly lighter)
88 │ > solo_awp_0305_0915.csv                         │ <- Row 2
118│ > right_0305_0900.csv                             │ <- Row 3
148│ > skills_0304_1520.csv                            │ <- Row 4
178│ > left_safe_0304_1430.csv                         │ <- Row 5
208├──────────────────────────────────────────────────┤
   │ [ UP ]          1-6 of 12           [ DOWN ]     │ <- Navigation bar
240└──────────────────────────────────────────────────┘
```

**Layout regions:**

| Region | Y range | Content |
|---|---|---|
| Title bar | 0-28 | "LOG FILES (n)" label + REFRESH button |
| File list | 28-208 | 6 rows x 30px each, alternating background colors |
| Navigation | 208-240 | UP/DOWN buttons (conditional), page info |

**Touch targets:**

| Target | Bounds | Action |
|---|---|---|
| REFRESH button | x:390-470, y:2-26 | Re-scan `/usd/logs/` directory |
| File row i | x:0-480, y:(28+i*30) to (58+i*30) | Open file at `browserScroll + i` |
| UP button | x:10-120, y:208-240 | Scroll up by 6 files |
| DOWN button | x:360-470, y:208-240 | Scroll down by 6 files |

### Log Content Screen

```
 0 ┌──────────────────────────────────────────────────┐
   │[BACK] skills_0305_1520.csv                       │ <- Title bar
28 ├──────────────────────────────────────────────────┤
30 │ # mode=SKILLS color=RED sensor=OFF               │ <- Gray (comment)
50 │ elapsed,label,x,y,heading                        │ <- Cyan (CSV header)
70 │ 00:00.000000,START,0.0,0.0,0.0                   │ <- White (data)
90 │ 00:00.152341,APPROACH_LOADER,-13.5,1.7,0.0       │
110│ 00:01.043127,AT_LOADER,-13.5,-27.8,0.1           │
130│ 00:01.044000,loader_deploy                        │
150│ 00:02.500512,LOADERS_DONE,-1.5,-29.5,90.0        │ <- 7 data lines visible
170│ 00:03.100000,ALIGNED,-2.0,-30.0,180.0            │
190│ # end=00:58.234000 dropped=0                      │ <- Gray (footer)
210├──────────────────────────────────────────────┤ ║ │ <- Scrollbar (right edge)
   │ [ UP ]      Ln 1-9 of 23       [ DOWN ]     │ ║ │ <- Navigation bar
240└──────────────────────────────────────────────┘   │
```

**Layout regions:**

| Region | Y range | Content |
|---|---|---|
| Title bar | 0-28 | BACK button + filename in cyan |
| Content | 30-210 | 9 lines x 20px each, color-coded by type |
| Scrollbar | 30-210 (x:472-479) | Proportional thumb showing position |
| Navigation | 210-240 | UP/DOWN buttons (conditional), line info |

**Color coding:**

| Line Type | Detection | Color | Hex |
|---|---|---|---|
| Comment | Starts with `#` | Gray | `0x808080` |
| CSV header | Starts with `elapsed,` | Cyan | `0x00FFFF` |
| Data / free-form | Everything else | White | `0xFFFFFF` |

**Touch targets:**

| Target | Bounds | Action |
|---|---|---|
| BACK button | x:5-75, y:2-26 | Return to file browser |
| Content area | x:0-480, y:30-210 | Page down by 9 lines |
| UP button | x:10-120, y:210-240 | Page up by 9 lines |
| DOWN button | x:360-470, y:210-240 | Page down by 9 lines |

### Scrollbar

The scrollbar provides visual position feedback on files longer than 9 lines. It is **display-only** (not draggable).

```cpp
thumb_height = track_height * (LINES_PER_PAGE / totalLines)   // proportional
thumb_position = track_top + (track_height - thumb_height) * (currentTopLine / max_scroll)
```

Minimum thumb height is 10px to remain visible on very long files.

## Color Scheme

The palette intentionally matches `config_ui.cpp` so both apps feel like part of the same family:

| Element | Color | Hex | Source |
|---|---|---|---|
| Background | Black | `0x000000` | Same as `config_ui::COLOR_BACKGROUND` |
| Title bar background | Dark navy | `0x1A1A2E` | Subtle distinction from pure black |
| Title/header text | Cyan | `0x00FFFF` | Same as `config_ui::COLOR_SECTION_LABEL` |
| Normal text | White | `0xFFFFFF` | Same as `config_ui::COLOR_TEXT` |
| Button background | Dark gray | `0x303030` | Same as `config_ui::COLOR_BUTTON_UNSELECTED` |
| Button border | White | `0xFFFFFF` | Same as `config_ui::COLOR_BUTTON_BORDER` |
| File row (even) | Very dark gray | `0x101010` | Alternating stripe |
| File row (odd) | Slightly lighter | `0x1A1A1A` | Alternating stripe |
| Comment lines (`#`) | Gray | `0x808080` | De-emphasized metadata |
| CSV header line | Cyan | `0x00FFFF` | Matches accent color |
| Data lines | White | `0xFFFFFF` | Primary content |
| Scrollbar track | Gray | `0x606060` | Unobtrusive |
| Scrollbar thumb | Cyan | `0x00FFFF` | Matches accent color |
| Error text | Red | `0xFF0000` | Alarm color |

## Touch Interaction Model

### Polling Loop

Touch input is polled in the `opcontrol()` main loop at 50Hz (20ms delay), matching the pattern from `config_ui.cpp`:

```cpp
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
```

### Debounce

200ms debounce between touch events prevents accidental double-taps, especially when scrolling quickly. This matches the value used in `config_ui.cpp`.

### Hit Detection

All touch targets use axis-aligned bounding box (AABB) collision via the `inBounds()` helper:

```cpp
static bool inBounds(int16_t x, int16_t y,
                     int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}
```

File row hit detection uses arithmetic rather than per-row bounds checks:

```cpp
int row = (y - LIST_Y1) / ROW_HEIGHT;    // Which row was tapped?
int fileIdx = browserScroll + row;        // Map to file index
```

### Dispatch

`handleTouch()` dispatches based on `currentScreen`:

```
handleTouch(x, y)
├── FILE_BROWSER → handleBrowserTouch(x, y)
│   ├── REFRESH button → loadFileList() + drawBrowserScreen()
│   ├── File row → openFile(fileIdx)
│   ├── UP button → browserScroll -= 6, drawBrowserScreen()
│   └── DOWN button → browserScroll += 6, drawBrowserScreen()
└── LOG_CONTENT → handleContentTouch(x, y)
    ├── BACK button → currentScreen = FILE_BROWSER, drawBrowserScreen()
    ├── UP button → currentTopLine -= 9, loadPage(), drawContentScreen()
    ├── DOWN button → currentTopLine += 9, loadPage(), drawContentScreen()
    └── Content area tap → page down (same as DOWN)
```

## Drawing Implementation

### Drawing Primitives

All rendering uses the `pros::screen::` API (raw pixel drawing). LVGL widgets are not used, consistent with the existing codebase.

| Function | Usage |
|---|---|
| `screen::erase()` | Clear entire screen |
| `screen::set_pen(color)` | Set drawing color |
| `screen::set_eraser(color)` | Set text background color |
| `screen::fill_rect(x1,y1,x2,y2)` | Filled rectangle |
| `screen::draw_rect(x1,y1,x2,y2)` | Rectangle outline |
| `screen::print(size, x, y, fmt, ...)` | Formatted text at position |

### Button Drawing

Buttons follow the same pattern as `config_ui.cpp` — dark gray fill, white border, centered white text:

```cpp
static void drawButton(x1, y1, x2, y2, text) {
    fill_rect(x1, y1, x2, y2);           // Dark gray background
    draw_rect(x1, y1, x2, y2);           // White border
    // Center text: x = x1 + (width - textWidth) / 2
    print(TEXT_MEDIUM, centeredX, centeredY, text);
}
```

Unlike `config_ui.cpp`'s `drawButton` which takes a `selected` state and `selectedColor`, the log viewer's buttons have no selection state — they are action buttons (REFRESH, UP, DOWN, BACK), not toggle buttons.

### Conditional Rendering

Navigation buttons are only drawn when they are actionable:

- **UP** only appears when `scrollOffset > 0`
- **DOWN** only appears when there are more items below the visible area

This prevents misleading UI elements. The touch handler also checks bounds, so even if a phantom button area is tapped, nothing happens.

### Full Redraws

Every touch event triggers a full screen redraw (`drawBrowserScreen()` or `drawContentScreen()`). This is simpler than incremental updates and eliminates visual artifacts. At 50Hz polling with 200ms debounce, redraws happen at most 5 times per second — well within the screen's capability.

## Public API

Header file: `include/log_viewer.h`

```cpp
namespace log_viewer {

void initialize();              // Scan files, draw browser
void disabled();                // No-op
void competition_initialize();  // No-op
void autonomous();              // Display "no auton" message
void opcontrol();               // Main touch loop

} // namespace log_viewer
```

### API Design Rationale

- **Namespace `log_viewer`**: Matches `competition::` and `config_ui::` convention
- **Five entry points**: Required by the PROS/AppMode dispatch pattern in `main.cpp`
- **No public state**: All state is file-scoped static inside `log_viewer.cpp`
- **No dependencies on robot hardware**: Does not include `robot_config.h`, LemLib, or any motor/sensor headers. The log viewer is purely a file-browsing UI.

## Initialization Flow

1. `delay(50)` — Ensure screen hardware is ready (matches `config_ui` pattern)
2. Call `loadFileList()`:
   a. `usd_list_files("/logs", buffer, 2048)` to get filenames
   b. Parse newline-separated results, filter for `.csv` suffix
   c. Reverse order (most recent first)
   d. Reset `browserScroll = 0`
3. Call `drawBrowserScreen()` — Render the file list

The viewer is immediately usable after `initialize()`. If no SD card is present, `usd_list_files` returns `PROS_ERR` and the browser shows "No log files found".

## File Open Flow

When a user taps a file row:

1. Build full path: `snprintf(selectedPath, ..., "/usd/logs/%s", fileNames[idx])`
2. Call `scanFileLines(selectedPath)`:
   a. `fopen(path, "r")`
   b. Loop: `lineOffsets[i] = ftell(f)`, `fgets()` to advance, increment `totalLines`
   c. `fclose(f)`
3. If scan fails (file deleted between listing and opening): show error, return to browser
4. `currentTopLine = 0`
5. Call `loadPage(selectedPath, 0)`:
   a. `fopen`, `fseek(lineOffsets[0])`, read 9 lines into `lineBuffer[]`, `fclose`
6. Set `currentScreen = LOG_CONTENT`
7. Call `drawContentScreen()`

## Error Handling

Following the project's "never crash" philosophy:

| Scenario | Behavior |
|---|---|
| No SD card | `usd_list_files` returns `PROS_ERR`. Browser shows "No log files found" |
| Empty `/usd/logs/` directory | `fileCount == 0`. Browser shows "No log files found" |
| No `.csv` files in directory | `fileCount == 0` after filtering. Same as above |
| File deleted between listing and opening | `fopen` fails in `scanFileLines`. Shows "Cannot open file" error for 1 second, returns to browser |
| File has >512 lines | Only first 512 lines are indexed. Lines beyond that are not accessible via scrolling. Acceptable for log files which typically have 10-50 lines |
| Very long lines (>60 chars) | Truncated to 60 characters for display. Full data remains on SD card |
| `usd_list_files` buffer overflow | Later filenames silently truncated. Only files fitting in the 2048-byte buffer are shown |
| Tap on empty file row | `fileIdx >= fileCount` check prevents out-of-bounds access |
| Scroll past boundaries | `browserScroll` and `currentTopLine` are clamped to valid ranges |

No assertions, no exceptions, no error dialogs beyond the brief "Cannot open file" message.

## Files Summary

| File | Action |
|---|---|
| `include/log_viewer.h` | **New** — namespace with 5 entry-point declarations |
| `src/log_viewer.cpp` | **New** — all implementation (~580 lines) |
| `src/main.cpp` | **Modify** — add `LOG_VIEWER` to `AppMode` enum, add `else if constexpr` branches in all 5 dispatch functions |

No build system changes needed — the Makefile auto-discovers `.cpp` files in `src/`.

## Relationship to File Logger

The log viewer reads files produced by the file logger (`src/file_logger.cpp`). The two modules are **completely independent** — they share no code, no headers, and no state. The only connection is the file format convention:

- **File logger writes**: `/usd/logs/<name>_<MMDD>_<HHMM>.csv`
- **Log viewer reads**: All `.csv` files in `/usd/logs/`

This means:
- The log viewer can read files created by any version of the file logger
- The log viewer can read manually-created CSV files placed in `/usd/logs/`
- The file logger does not need to know the log viewer exists (no index file, no registration)
- Both can be updated independently

## Fallback: Index File Approach

If `usd_list_files()` proves unreliable on certain V5 firmware versions, a fallback approach is available:

1. In `fileLoggerInit()`, append the new filename to `/usd/logs/index.txt`:
   ```cpp
   FILE* idx = fopen("/usd/logs/index.txt", "a");
   if (idx) {
       fprintf(idx, "%s\n", logFilePath + 10);  // Skip "/usd/logs/" prefix
       fclose(idx);
   }
   ```
2. In `loadFileList()`, read `/usd/logs/index.txt` line by line instead of calling `usd_list_files`

This requires a 2-line change in `file_logger.cpp` and a rewrite of `loadFileList()`. It trades the clean independence between modules for guaranteed compatibility.
