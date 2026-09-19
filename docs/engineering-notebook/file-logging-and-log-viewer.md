# Engineering Notebook: File Logging & Log Viewer

**Date:** March 2026
**Team:** 24580A
**Authors:** Team Members

---

## The Problem

When our robot runs an autonomous routine during a match, we have no way to know what actually happened inside the code. The autonomous only lasts 15 seconds (or 60 seconds for skills), and once it's done, we can't go back and see what the robot was thinking.

Before this feature, we could only debug by:
- Watching the robot and guessing what went wrong
- Printing numbers to the brain screen, but they flash by too fast to read
- Plugging the robot into a laptop and using a terminal, which we can't do during a tournament

This is a big problem at competitions. Between matches we only have a few minutes. If our auton misses a goal or takes a wrong path, we need to figure out why and fix it fast. Without data, we're just guessing.

**We need a way to record what the robot does during a match and review it quickly afterward.**

---

## Solutions We Considered

### Option 1: Save Log Messages in Memory and Show on the Brain Screen

**How it works:** Store all log messages in the brain's memory (RAM) and build a scrollable viewer on the touchscreen. After the match, we could scroll up and down through all the messages right on the brain.

**Pros:**
- Very easy to start coding (one line per message to log)
- No extra hardware needed
- Can scroll back and forth through the full history after the match
- Shows info in real time during the match

**Cons:**
- Uses up the brain's limited memory (the V5 only has 512KB for our program) — the more we log, the less memory is left for everything else
- Once we turn off the robot or switch programs, all the data is gone forever — there's no way to save it
- We have to review the data while the program is still running, which means we can't power cycle the robot or upload new code first
- If the program crashes, we lose all the log data along with it

**Verdict:** Better than just printing 8 lines, but the data is still temporary. We need something that survives power cycles so we can review logs even after restarting the robot.

### Option 2: Print to a Laptop Terminal (USB)

**How it works:** Connect the robot to a laptop with a USB cable and use `printf()` to send messages to a terminal window.

This option was ruled out immediately. During a VRC competition match, the robot cannot be tethered to a laptop — it's not allowed by the rules, and the cable would interfere with the match. Since the whole point of this feature is to debug issues at competitions, a solution that only works at home during practice doesn't solve our problem. We still use this method during development, but we need something that works on the competition field.

### Option 3: Save Logs to the SD Card

**How it works:** Write log messages to a CSV file on the micro SD card inside the V5 brain. After the match, pull out the SD card and read the file on a laptop.

**Pros:**
- Data survives after the match — we can review it anytime
- CSV files open in Excel or Google Sheets for easy analysis
- Can log as much data as we want (SD card has plenty of space)
- Works during real matches with no cables

**Cons:**
- Writing to the SD card is slow — if we write during auton, it could delay our robot's movements
- We have to physically remove the SD card and find a laptop to read it, which takes time between matches
- Need to be careful not to crash the robot code if the SD card is missing

**Verdict:** This is the right idea, but we need to solve the speed problem and make it easier to review logs without removing the SD card.

### Option 4: Save Logs to SD Card + On-Screen Log Viewer (Our Solution)

**How it works:** Combine Option 3 with a touchscreen app on the brain that lets us browse and read log files directly on the robot's screen — no laptop needed.

**Pros:**
- All the benefits of SD card logging (persistent, detailed data)
- We solve the speed problem with a special "double buffer" technique (explained below)
- After a match, we just switch to the log viewer program and tap through the logs on the touchscreen
- No need to remove the SD card or find a laptop between matches
- Fast turnaround — we can review data in under 30 seconds

**Cons:**
- More complex to build (two features instead of one)
- The brain screen is small (480x240 pixels), so we can only show a few lines at a time
- Need to scroll through logs on a tiny touchscreen

**Verdict:** This is our chosen solution. The extra complexity is worth it for the huge time savings at tournaments.

---

## Our Solution: How It Works

### Part 1: The File Logger in the Competition Program

The file logger runs in the background during autonomous and saves everything to a `.csv` file on the SD card.

#### How the Auton Code Uses It

From the auton code's perspective, logging is just one function call:

```cpp
fileLogPose("reached loader");
```

This call returns **instantly** — it does not wait for the SD card. The auton code keeps running its next movement right away without any pause. Behind the scenes, the message gets dropped into a slot in memory, and a separate background task picks it up later and writes it to the SD card file. The auton code never knows or cares about the file — it just logs and moves on.

This is the key idea: **the code that logs and the code that writes to the file are separate**. The auton logs messages, and a background writer saves them. They run independently so neither one slows down the other.

#### The Problem: How Do They Share Data Safely?

Since we have two pieces of code running at the same time (the auton logging messages, and the writer saving them to the SD card), we need a way to hand off messages without them stepping on each other. If the auton is writing a message into a slot at the same moment the writer tries to read that same slot, the data could get corrupted.

We brainstormed two approaches to solve this:

**Approach A: Use a Mutex Lock (Simpler but Slower)**

The simpler approach is to use a "mutex" (short for "mutual exclusion") — a locking mechanism provided by PROS as `pros::Mutex`. It works like a padlock on a shared notepad. The two key operations are:

- `mutex.take()` — grab the lock. If someone else already has it, your code **stops and waits** until they release it.
- `mutex.give()` — release the lock so the other side can grab it.

The auton code would call `mutex.take()` before writing a message, write it, then call `mutex.give()`. The background writer would do the same before reading messages to save them to the SD card. Since only one side can hold the mutex at a time, they never touch the shared data at the same moment.

This is easier to code, but the problem is timing. Writing to the SD card is slow — it can take several milliseconds. While the writer holds the mutex to save entries to the SD card, the auton code calls `mutex.take()` and gets **stuck waiting** until the writer calls `mutex.give()`. On the V5 brain, which has a single-core processor, this waiting pauses our auton movements and throws off timing. In a 15-second autonomous where every millisecond matters, we can't afford random pauses caused by SD card writes.

**Approach B: Double Buffer with Atomic Operations (Our Choice)**

Instead of sharing one notepad with a mutex, we give each side its own notepad and swap them. This way, the auton and the writer are always working on **different** notepads, so they never need to wait for each other. No mutex is used anywhere.

The key mechanism is `std::atomic` — a special variable type where reads and writes happen in a single CPU instruction that can't be interrupted. We use two atomic operations:

- `fetch_add(1)` — atomically increments a counter and returns the old value. The auton uses this to claim the next available slot in the notepad. Even if multiple parts of the code call this at the exact same time, each one gets a unique slot number — no duplicates, no conflicts, and it completes instantly.
- `compare_exchange` — atomically swaps which notepad is active. The writer uses this to say "switch from Notepad A to Notepad B" in a single step. It's impossible for the swap to happen halfway or for a message to go to the wrong notepad.

These atomic operations are built into the ARM processor on the V5 brain. They take nanoseconds — thousands of times faster than a mutex lock/unlock cycle. The auton code is never blocked, not even for a microsecond.

We chose Approach B because the whole point of the logger is to help us debug without affecting robot performance. A logger that sometimes pauses the robot would be worse than no logger at all.

#### How the Double Buffer Works

Instead of one shared notepad with a lock, we use **two notepads and a clever swap trick**.

Think of it like this: the auton code quickly jots notes on Notepad A. Meanwhile, the background writer is copying everything from Notepad B to the SD card. They are working on **different notepads**, so they never interfere with each other — no lock needed.

When the writer finishes with Notepad B, it swaps the two: now the auton starts writing on Notepad B (which is empty), and the writer starts copying Notepad A. The swap itself is done using a special CPU instruction called an "atomic operation" that completes in a single step — it's impossible for the swap to happen halfway.

```
Auton Code                          Background Writer
    |                                      |
    |-- writes to Notepad A ---->          |
    |   (instant, no waiting)      copies Notepad B to SD card
    |                                      |
    |       <<< SWAP (atomic) >>>          |
    |                                      |
    |-- writes to Notepad B ---->          |
    |   (instant, no waiting)      copies Notepad A to SD card
    |                                      |
```

Here's why no entries get lost:
- The auton claims a slot using a single atomic counter — even if multiple parts of the code log at the exact same time, each one gets its own unique slot
- Each slot has a "ready" flag. The auton writes its message first, then sets the flag. The writer waits for the flag before reading, so it never sees a half-written message
- The swap only happens after the writer finishes draining the old notepad, so no messages are skipped

Each notepad holds 64 messages. The writer checks every 50 milliseconds if there's anything to save. The auton code never waits — if both notepads are full (which almost never happens), the message is simply dropped rather than blocking the robot.

#### What Gets Logged

Every log entry includes a timestamp showing how many seconds have passed since the auton started. For example:

```
# mode=RIGHT color=RED sensor=ON aggr=ON push=OFF
elapsed,label,x,y,heading
000.045,Auton started,0.0,0.0,0.0
001.230,reached loader,12.5,-27.8,45.2
003.450,First 3 blocks picked up,15.0,-30.1,90.0
005.120,Robot disabled.,30.9,0.0,142.0
# end=005.235 dropped=0
```

The first line records our configuration (which auton, team color, settings). Each data line shows the time, a label describing what happened, and the robot's position (x, y, heading). The last line tells us the total time and if any messages were dropped.

#### Safety First

The logger is designed to never crash the robot. Every function is wrapped in error handling so that if anything goes wrong (SD card missing, file can't open, etc.), the logger quietly does nothing instead of crashing. Logging should help us debug — it should never BE the bug.

### Part 2: The Log Viewer

The log viewer is a separate program we upload to a different slot on the brain. After a match, we switch to it on the brain's screen.

It has two screens:

**Screen 1: File Browser**
- Shows a list of all log files on the SD card
- Most recent files appear first
- Tap a filename to open it
- UP and DN buttons to scroll through the list

**Screen 2: Log Content**
- Shows the contents of the selected log file
- Color-coded: gray for comments, cyan for the CSV header, white for data
- BACK button to return to the file list
- UP and DN buttons to scroll one line at a time
- A scrollbar on the right shows our position in the file
- Tap anywhere on the text to jump down a full page

The buttons are grouped together on the right side of the bottom bar so we can quickly tap between UP and DN with one finger. The page info (like "Ln 1-9 of 25") is shown on the left.

---

## Key Design Decisions

| Decision | What We Chose | Why |
|---|---|---|
| Log format | CSV (comma-separated values) | Opens in Excel/Sheets, easy to parse, human-readable |
| Timestamp format | SSS.mmm (seconds.milliseconds) | Auton is max 120 seconds, so 3-digit seconds is enough. Shorter than minutes:seconds |
| Buffer size | 64 entries per buffer | Enough for typical auton logging without using too much memory |
| Write interval | Every 50ms | Fast enough to keep up with logging, slow enough to not waste CPU |
| Log viewer as separate program | Upload to a different brain slot | Keeps competition code clean, can switch between programs on the brain |
| File naming | auton_name + date + time | Each run creates a unique file so old logs are never overwritten |

---

## Results

With this system, our debugging workflow at tournaments is:

1. Run the match
2. Switch to the log viewer program on the brain
3. Tap the most recent log file
4. Scroll through to see exactly what happened — positions, timing, and labels
5. Identify the issue and adjust our auton code

This takes about 30 seconds compared to the old process of guessing or needing a laptop. The file logger has zero impact on robot performance because of the double buffer design, and the log viewer gives us instant access to match data right on the brain screen.

---

## Files We Created/Modified

| File | What It Does |
|---|---|
| `include/file_logger.h` | Declares the logging functions other code can call |
| `src/file_logger.cpp` | The full logger implementation (double buffer, background writer, file I/O) |
| `include/log_viewer.h` | Declares the log viewer entry points |
| `src/log_viewer.cpp` | The touchscreen file browser and log content viewer |
| `src/competition.cpp` | Modified to start the logger in auton and stop it when disabled |
| `src/main.cpp` | Modified to add LOG_VIEWER as a program mode |
| `src/auton/*.cpp` | Modified to add `fileLogPose()` calls at key waypoints |
