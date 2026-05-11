# HomeCare Hub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a HomeCare Hub household control-tablet app to the ESP32-P4 Brookesia demo.

**Architecture:** Create one new Brookesia phone app that owns its LVGL UI and simulated home-care state. Register it with the existing apps component and install it from the main app startup alongside the existing sample apps.

**Tech Stack:** ESP-IDF, C++, LVGL, ESP Brookesia Phone app framework.

---

## File Structure

- Create `components/apps/homecare_hub/HomeCareHub.hpp`: app class declaration and simulated state types.
- Create `components/apps/homecare_hub/HomeCareHub.cpp`: LVGL UI construction, demo state updates, button callbacks.
- Modify `components/apps/CMakeLists.txt`: include the new source and include directory.
- Modify `components/apps/apps.h`: expose `HomeCareHub`.
- Modify `main/main.cpp`: install `HomeCareHub`.

## Tasks

### Task 1: Register a New Empty App

**Files:**
- Create: `components/apps/homecare_hub/HomeCareHub.hpp`
- Create: `components/apps/homecare_hub/HomeCareHub.cpp`
- Modify: `components/apps/CMakeLists.txt`
- Modify: `components/apps/apps.h`
- Modify: `main/main.cpp`

- [ ] Create a minimal `HomeCareHub` class inheriting `ESP_Brookesia_PhoneApp`.
- [ ] Add it to the apps component build.
- [ ] Include it from `apps.h`.
- [ ] Install it from `main/main.cpp` before the general sample apps.
- [ ] Run `idf.py reconfigure` or `idf.py build` to catch missing symbols.

### Task 2: Add Simulated State

**Files:**
- Modify: `components/apps/homecare_hub/HomeCareHub.hpp`
- Modify: `components/apps/homecare_hub/HomeCareHub.cpp`

- [ ] Add room, car, weather, and event state structs.
- [ ] Add four demo scenarios: normal patrol, suspected fall, bathroom stay, night leave-bed.
- [ ] Add a mode setter used by both timer and buttons.
- [ ] Keep all data local to the app.

### Task 3: Build the Tablet Dashboard UI

**Files:**
- Modify: `components/apps/homecare_hub/HomeCareHub.cpp`

- [ ] Create the top bar with home name and privacy/local AI indicators.
- [ ] Create left room overview cards.
- [ ] Create center car status panel with route progress.
- [ ] Create right weather/environment panel.
- [ ] Create bottom event strip and quick action buttons.
- [ ] Use stable dimensions for 1024 x 600 landscape.

### Task 4: Wire Dynamic Updates

**Files:**
- Modify: `components/apps/homecare_hub/HomeCareHub.cpp`

- [ ] Bind all labels and bars to simulated state.
- [ ] Add touch callbacks for scenario buttons and quick actions.
- [ ] Add an LVGL timer to rotate demo scenarios.
- [ ] Delete the timer on app close.

### Task 5: Verify

**Files:**
- Check all modified files.

- [ ] Run a build or the closest available ESP-IDF validation command.
- [ ] If build is unavailable or blocked by pre-existing SDK state, run targeted static checks for source registration and symbol references.
- [ ] Report changed files and verification result.
