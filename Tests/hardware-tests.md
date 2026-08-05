# Hardware Tests — Smart Fan Retrofit (Philips CX2550/00)

Running log of bench and integration tests. Each entry records what was tested,
the setup, the expected result, the actual result, and pass/fail. Newest stages
are appended as they are completed.

Legend: ✅ pass · ⚠️ pass with note · ❌ fail

---

## Environment / setup reference

- **ESP32:** AZ-Delivery ESP32-S DevKit C V4 (classic ESP32-WROOM-32). USB: CH340.
- **ESPHome board definition:** Generic ESP32 Board (`esp32dev`), arduino framework.
- **Home Assistant:** HA **Container** on Raspberry Pi 4 (alongside AdGuard).
- **ESPHome:** standalone install on the Pi, own IP:port (not a HAOS add-on).
- **Opto test resistor to GPIO:** 220 Ω or 470 Ω acceptable (either used for bench).
- **Fake button node (bench):** 5 V → 10 kΩ pull-up → node → PC817 pin 4;
  PC817 pin 3 → GND. Node read with multimeter and/or LED+~1 kΩ indicator.

---

## Test 1 — ESP32 GPIO → PC817 drive (bench, no Home Assistant)

**Goal:** confirm the ESP32 can drive a PC817 optocoupler and that the opto output
pulls a pulled-up "fake button" node from ~5 V down to ~ground — i.e. the button-
press topology works, with correct polarity.

**Setup:**
- Firmware: standalone Arduino sketch toggling one GPIO (GPIO25) HIGH/LOW on a
  timer (no ESPHome, no HA).
- Input side: GPIO25 → 220/470 Ω → PC817 pin 1 (anode); PC817 pin 2 → ESP32 GND.
- Output side: 5 V → 10 kΩ → fake node → PC817 pin 4 (collector); PC817 pin 3
  (emitter) → GND.
- Indicator: LED (anode → 5 V) in series with ~1 kΩ to the fake node, to visualise
  the node being pulled low. Multimeter also on the node.

**Expected:**
- GPIO LOW (opto off) → node ~5 V, indicator LED off.
- GPIO HIGH (opto on) → node ~0.1–0.3 V, indicator LED on.

**Actual:** Indicator LED blinks in step with the GPIO toggle — node is being
pulled low when the opto is driven and released when it isn't. Behaviour matches
the button-press topology.

**Result:** ✅ **PASS** — ESP32→PC817→node drive verified; polarity correct.

**Notes:**
- This exercises switching behaviour. True galvanic isolation is not exercised on
  the bench because the output side shares the ESP32's 5 V/GND; it will be isolated
  in the real build (ESP32 GND vs fan GND).
- Pulse-width (`PULSE_MS`) / gap (`GAP_MS`) tuning deliberately deferred to Stage 2
  against the real fan, where the fan MCU is the arbiter of a registered press.

---

## Test 2 — Stage 1 ESPHome bring-up + Home Assistant control

**Goal:** ESP32 runs ESPHome, joins Wi-Fi, connects to Home Assistant over the
native API, and HA can both read data from and send commands to the device.

**Setup:**
- Firmware: Stage 1 ESPHome config (Wi-Fi, native API, OTA, diagnostics:
  Uptime, WiFi Signal, IP Address; plus a GPIO switch on GPIO25 — "Opto Test 1").
- First flash: **Factory image** downloaded from standalone ESPHome (compiled on
  the Pi) and flashed to the ESP32 via **web.esphome.io** over USB from the laptop.
- Device added to HA (Container) via the ESPHome integration (host = ESP32 IP,
  port 6053, API encryption key).

**Expected:**
- Device shows online in ESPHome logs; Wi-Fi connects; API starts.
- In HA: device online; Uptime climbs; IP/WiFi entities populate (ESP32 → HA).
- Toggling the GPIO switch from HA drives the hardware pin (HA → ESP32).

**Actual:** Device flashed and came online. From Home Assistant, an LED connected
to the GPIO pin can be switched **on and off**, confirming HA → ESP32 command flow.

**Result:** ✅ **PASS** — HA ↔ ESP32 link verified in both directions;
HA → ESP32 command control (the critical direction for button presses) confirmed.

**Notes:**
- HA is Container-based, so device onboarding may require the **manual** ESPHome
  integration add (IP + 6053 + key) rather than auto-discovery.
- After this first USB flash, subsequent firmware updates use **OTA** (wireless).
- Corresponds to **Stage 1** exit criteria in firmware-plan.md.

---

## Pending / upcoming tests

Tracked against firmware-plan.md stages (to be logged as completed):

- **Stage 2** — single button (SW71) against the real fan; tune `PULSE_MS`/`GAP_MS`;
  confirm one fired press = exactly one power toggle (awake).
- **Stage 3** — all four buttons individually actuate; no cross-talk.
- **Stage 4** — state model + awake/asleep (`t_idle`) wake logic; single logical
  action reliable when awake and after the ~1 min asleep timeout.
- **Stage 5** — HA power on/off via the model.
- **Stage 6** — 5-mode select via SW72 ring; confirm power-on default (anchor).
- **Stage 7** — oscillation on/off ("stop where it is").
- **Stage 8** — sleep timer via HA automation.
- **Stage 9** — guard rails, dashboard, resync control, final install on fan 5 V.
