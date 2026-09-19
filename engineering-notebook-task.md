# Engineering Notebook - Understanding Tasks in PROS


## What Are Tasks?

Tasks are like having multiple workers on our robot that can do different jobs at the same time. In PROS, we use `pros::Task` to create these "workers."

**Our robot has 3 tasks:**
1. **DrivetrainTask** - Controls driving and intake
2. **buttonTask** - Handles pneumatic buttons (loader, hook)
3. **screenTask1** - Updates the controller display

---

## Why Use Tasks Instead of One Big Loop?

### Before Tasks (Bad Way):
```cpp
void opcontrol() {
    while (true) {
        // Check joysticks
        // Check buttons  
        // Update screen
        pros::delay(10);
    }
}
```
**Problem:** Everything runs at the same speed and gets messy!

### With Tasks (Better Way):
Each system runs independently at its own speed:
- **Driving updates every 10ms** (super responsive!)
- **Buttons check every 20ms** (fast enough)
- **Screen updates every 100ms** (doesn't need to be super fast)

### Benefits:
✅ **Organized** - Each task does one job  
✅ **Different speeds** - Everything runs at the right rate  
✅ **No freezing** - If one task waits, others keep running  
✅ **Easier to debug** - Can fix one system without breaking others

---

## Downsides of Tasks

### 1. Race Conditions ⚠️
**Problem:** Two tasks trying to control the same thing at once!

**In our code:**
- `DrivetrainTask` controls `centergoal` pneumatic
- `buttonTask` could also try to control it
- They might fight over it!

**Fix:** Only let one task control each device, or use a mutex (lock).

### 2. Harder to Debug
- Tasks don't run in a predictable order
- Bugs might only happen sometimes
- Can't step through code as easily

### 3. Memory Usage
- Each task uses 8KB of RAM for its stack
- Our 3 tasks = 24 KB total
- (V5 Brain has 128 MB, so we're totally fine!)

### 4. Need to Be Careful
- Must add `pros::delay()` or tasks hog the CPU
- Have to think about which task controls what

---

## V5 Brain Technical Specs

**Processor:**
- ARM Cortex-A9, 667 MHz
- **1 CPU core** (not multiple!)
- 128 MB of RAM
- 32 MB Flash storage

**What This Means:**
Tasks don't *actually* run at the same time - the brain switches between them really fast (time-slicing). Since it switches every few milliseconds and runs at 667 MHz, it *feels* like they're all running together!

---

## How Task Scheduling Works

### Priority System:
- Priority ranges from **1 (lowest) to 15 (highest)**
- Our tasks all use **priority 8** (default)
- Higher priority tasks run first

### Time-Slicing:
```
Time: |--Task1--|-Task2-|--Task3--|-Task1-|...
      └── Brain switches every few ms ──┘
```

With 1 CPU core, tasks take turns using the processor.

### Why Our Code Works Well:
✅ All tasks have `pros::delay()` - gives other tasks time to run  
✅ Each task finishes quickly (< 1ms of work)  
✅ 667 MHz is plenty fast for 3 simple tasks  
✅ Good delays prevent CPU hogging

---

## Best Practices We Learned

### ✅ DO:
- Put `pros::delay()` in every task loop
- Keep tasks focused on one job
- Only let one task control each device
- Use longer delays for less important tasks (like screens)

### ❌ DON'T:
- Create infinite loops without delays (CPU hog!)
- Have multiple tasks control the same hardware
- Make tasks too complicated
- Forget that tasks keep running after `opcontrol()` ends

---

## Code Improvements to Make

1. **Add delay to buttonTask:**
   ```cpp
   pros::delay(20);  // At end of while loop
   ```

2. **Fix centergoal conflict:**
   - Only control it from DrivetrainTask
   - Remove from buttonTask control

3. **Optional - Adjust priorities:**
   - Drivetrain = Priority 10 (most important)
   - Buttons = Priority 8 (normal)
   - Screen = Priority 6 (least important)

---

## Conclusion

Tasks are really useful for organizing our code and making different systems run at different speeds. They make our robot more responsive and easier to program. The main thing to remember is to use `pros::delay()` and avoid having tasks fight over the same devices!

**Key Takeaway:** Tasks = Multiple workers doing different jobs at their own pace on a single-core CPU that switches between them super fast.

