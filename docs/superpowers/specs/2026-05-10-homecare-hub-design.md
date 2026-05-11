# HomeCare Hub Design

## Goal

Build a household control-tablet experience for the ESP32-P4 Brookesia phone demo. The screen should feel like a home safety hub placed in a living room or entryway, showing room status, patrol-car status, weather, environment, privacy state, and recent safety events.

## Product Direction

The feature is based on the WiFi-CSI, edge vision, and obstacle-avoidance patrol car product analysis in `wifi-csi-vision-car-product-analysis.md`.

The interface should emphasize:

- Home-first framing rather than a phone utility or nursing-station dashboard.
- Privacy-friendly care: camera off by default, local AI confirmation only after an event.
- A clear closed loop: CSI anomaly, car dispatch, edge vision check, voice inquiry, alert or resolution.
- Demo-friendly local simulation so the UI can be shown without real car, weather, or MQTT services.

## Screen Layout

Target resolution is the existing ESP32-P4 board layout, 1024 x 600 landscape.

- Top bar: home name, date/time placeholder, connection state, local AI state, privacy mode.
- Left panel: room overview for Living Room, Bedroom, Bathroom, and Corridor. Each room shows occupancy/activity, CSI confidence, risk level, and privacy zone marker where relevant.
- Center panel: patrol car status with battery, position, destination, mission phase, route progress, obstacle, camera, and voice inquiry status.
- Right panel: weather and home environment with weather, outdoor temperature, humidity, air quality, indoor temperature/humidity, and night-risk hint.
- Bottom strip: recent events with level colors and compact summaries.
- Quick actions: start patrol, recall to charge, privacy mode, and contact family/caregiver.

## Data Model

Use a local in-memory simulated state for the first version. Keep the data grouped so a later MQTT, WebSocket, or HTTP data source can update the same fields without redesigning the UI.

The first version needs four demo modes:

- Normal patrol
- Suspected fall
- Bathroom stay
- Night leave-bed

The mode can rotate with a timer and can also be switched by touch buttons.

## Implementation

Create a new Brookesia app named `HomeCareHub` under `components/apps/homecare_hub`. Register it through the existing `apps.h` aggregation and install it in `main/main.cpp`.

The app should use direct LVGL object creation, matching the style of existing sample apps. It should avoid external network calls and avoid adding heavy dependencies.

## Verification

At minimum:

- The project should configure and compile far enough to validate new source registration and C++ symbols.
- Static file checks should verify the new app is included by `components/apps/CMakeLists.txt`, exposed through `apps.h`, and installed from `main/main.cpp`.
- The code should be readable and keep UI construction helpers scoped to the new app.

## Constraints

- Keep existing demo apps unless a compile conflict requires a small include/order change.
- Do not implement medical diagnosis claims.
- Do not upload video or imply always-on camera monitoring.
- Keep the first implementation self-contained and demo-ready.
