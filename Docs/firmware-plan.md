# Firmware Plan - Smart Fan Retrofit (Philips CX2550/00)

*Algorithm and staging only - no code. Each stage produces an independently
verifiable result before the next begins. Built on ESPHome, integrated with Home
Assistant over the native API.*

## Conventions used in this document

- **Press** = energise a button's PC817 for a fixed pulse, then release.
  Parameters: `PULSE_MS` (how long the opto is on) and `GAP_MS` (pause between
  consecutive presses). Tuned in Stage 2.
- **`t_idle`** = time since the ESP32's last press (any button).
- **`WAKE_THRESHOLD`** = 5 s if (fan mode == sleep); 60 s otherwise (LED timeout).
- **Awake** = `t_idle < WAKE_THRESHOLD` (LEDs assumed lit → 1 press acts).
- **Asleep** = `t_idle ≥ WAKE_THRESHOLD` (first press only wakes; no action).
- **Model** = the ESP32's assumed fan state: `power_on`, `mode`, `oscillating`.
- **Mode ring** (SW72, forward only): `speed1 → speed2 → speed3 → sleep →
  natural → speed1`. Off is NOT in the ring (off only via SW71).

---

## Stage 0 : Environment & safety baseline

**Goal:** tooling ready; no fan interaction yet.

**Steps:**
1. Install ESPHome (standalone or the Home Assistant add-on).
2. Install the CH340 USB driver for the AZ-Delivery board.
3. Confirm Home Assistant is reachable and the ESPHome dashboard opens.
4. Power rule for all later stages: ESP32 on **USB 5 V** during testing; the fan
   is only mains-powered when observing a press; soldering is done fan-unplugged.

**Independent test / exit criterion:**
- ESPHome dashboard loads; the board's serial port appears when plugged in.

---

## Stage 1 : Bare board bring-up (network only)

**Goal:** ESP32 runs ESPHome, joins Wi-Fi, appears in Home Assistant. No fan
wiring involved.

**Steps:**
1. Create a minimal ESPHome device config: board = generic `esp32`/`esp32dev`,
   Wi-Fi credentials, native API key, OTA enabled.
2. Flash once over USB.
3. Confirm the device connects to Wi-Fi and is discovered by Home Assistant; add
   it.
4. Add one trivial diagnostic entity (e.g. an on-board status indicator or uptime
   sensor) purely to confirm the API link.

**Independent test / exit criterion:**
- Device shows **online** in Home Assistant.
- The diagnostic entity updates in HA.
- A subsequent config change flashes successfully **over-the-air** (no USB).

---

## Stage 2 : One button, one pulse (single-channel actuation)

**Goal:** prove the ESP32 → PC817 → fan-button chain physically presses **one**
button, and tune the pulse timing. Use the **power button (SW71)** as the test
channel.

**Prerequisite hardware:** PC817 #1 wired for SW71 (GPIO→330Ω→opto→button node/
fan GND); ESP32 on USB; fan on mains.

**Steps:**
1. Expose a single manual "Press SW71" action/button entity in HA that fires one
   press with parameters `PULSE_MS`, `GAP_MS`.
2. Fire it once with a starting `PULSE_MS` (e.g. a value in the tens-of-ms range)
   while the fan is **awake** (press a physical button first so LEDs are lit).
3. Observe: exactly one power toggle should occur.
4. Tune `PULSE_MS` up/down until presses are 100% reliable (too short = missed;
   too long = risk of double-register). Record the working `PULSE_MS`.
5. Fire two presses back-to-back to tune `GAP_MS` so they register as two distinct
   presses (fan toggles off then on). Record `GAP_MS`.

**Independent test / exit criterion:**
- One fired press → exactly one power toggle, repeatable 10/10 times.
- Two fired presses → two toggles (ends where it started), repeatable.
- `PULSE_MS` and `GAP_MS` values recorded for reuse.

*Note: at this stage ignore awake/asleep ; always pre-wake manually. The
awake/asleep logic is added in Stage 4.*

---

## Stage 3 : All four buttons as raw momentary actions

**Goal:** each of the four buttons can be individually actuated from HA. 

**Prerequisite hardware:** all 4 PC817s wired (SW71–SW74).

**Steps:**
1. Wire and expose four manual press actions in HA: Press SW71, SW72, SW73, SW74,
   each using the tuned `PULSE_MS`/`GAP_MS`.
2. With the fan **awake**, fire each button once and confirm the expected physical
   effect:
   - SW71 → power toggles.
   - SW72 → mode advances one step in the ring.
   - SW73 → oscillation toggles.
   - SW74 → timer advances one step.
3. Verify no cross-talk: firing one channel never actuates another (checks wiring/
   isolation).

**Independent test / exit criterion:**
- Each of the four actions produces only its own button's effect, repeatable.
- No channel triggers a neighbour.

---

## Stage 4 : State model + awake/asleep wake logic

**Goal:** introduce the internal model and the `t_idle`- based wake handling, so a
single logical "do X" reliably performs X whether the board is awake or asleep.
This is the heart of open-loop control.

**Steps (algorithm):**
1. Add model variables: `power_on`, `mode`, `oscillating`, and a running
   `t_idle` timer that resets to 0 on every press.
2. Define a **`press(button)`** primitive that fires one pulse and resets
   `t_idle = 0`.
3. Define a **`wake_if_needed()`** routine:
   ```
   if t_idle >= WAKE_THRESHOLD:
       press(button_to_wake)   # performs no action when asleep
   ```
   **Wake-button choice** : Same button as action
4. Define an **`act(button, n=1)`** wrapper:
   ```
   wake_if_needed()
   repeat n times: press(button)  (respecting GAP_MS)
   ```
5. Boot behaviour : on boot press nothing; Retain the previous states.

**Independent test / exit criterion:**
- With the board **awake**, `act(SW71)` toggles power once.
- Let the board go **asleep**, then `act(SW71)`: the fan still
  toggles exactly once (the wake press + the action are handled correctly). Test for both wake times (5 s and 60 s)
- Repeat the asleep test several times → reliable single logical action each time.


---

## Stage 5 : Power on/off control (user-facing)

**Goal:** a Home Assistant on/off control that maps to the model + wake logic.

**Steps (algorithm):**
1. Expose a `switch` (or `fan` on/off) entity.
2. On "turn on": if `power_on` is false → `act(SW71)`; set `power_on=true`.
   On "turn off": if `power_on` is true → `act(SW71)`; set `power_on=false`.
   (If model already matches the request, do nothing - avoids redundant toggles.)

**Independent test / exit criterion:**
- From a known off state, HA "on" starts the fan; HA "off" stops it; repeatable
  both when awake and after the asleep timeout.
- Rapid on→off→on sequences behave correctly (chained commands stay awake, so only
  the first pays the wake cost).

---

## Stage 6 : Speed/mode select (5-position ring)

**Goal:** a 5-option selector in HA that lands on the requested mode via computed
SW72 presses.

**Steps (algorithm):**
1. Expose a `select` with options: `speed1, speed2, speed3, sleep, natural`.
2. On selection of `target`:
   ```
   if not power_on:
    turn_on()          # presses SW71 once; fan resumes its last mode on its own
    power_on = true
    # mode is left untouched here, the hardware already remembers it

   steps = forward_distance(mode -> target) in the ring
   act(SW72, n=steps)
   mode = target
   ```
   - `forward_distance` counts steps going forward only around
     `speed1→speed2→speed3→sleep→natural→speed1` (e.g. speed3→speed1 = 2 steps:
     natural, speed1).
   

**Independent test / exit criterion:**
- Turn the fan on from off, off again, then back on repeatedly at different modes, confirming each time that the fan resumes the same mode it was on before it was switched off.
- From each of the five modes, selecting each of the five targets lands correctly (5x5 matrix spot checked), awake and asleep.
---

## Stage 7 : Oscillation toggle ("stop where it is")

**Goal:** an on/off control for oscillation.

**Steps (algorithm):**
1. Expose an oscillation `switch`.
2. On "on": if `oscillating` is false → `act(SW73)`; set `oscillating=true`.
   On "off": if `oscillating` is true → `act(SW73)`; set `oscillating=false`.

**Independent test / exit criterion:**
- HA "oscillation on" starts the sweep; "off" stops it and the head **freezes at
  its current angle** (the core requirement), awake and asleep.
- Model stays consistent across several toggles.

---

## Stage 8 : Sleep timer (Home Assistant automation - primary path)

**Goal:** configurable sleep timer, implemented in Home Assistant rather than the
fan's native timer.

**Steps (algorithm, in HA):**
1. Expose a `number` or `select` in HA for the desired duration (e.g. minutes/
   hours), plus a "start sleep timer" control.
2. HA automation: on start, wait the chosen duration, then call the ESP32 power
   `turn_off` (Stage 5). Optionally support delayed-on (if fan is off, turn on
   after the duration) to mirror the fan's bidirectional native behaviour.
3. Provide a "cancel timer" control that clears the pending automation.

**Optional native path (secondary):** if native timer is ever wanted, implement
`act(SW74, n=steps)` targeting the timer ring `1h→2h→3h→4h→8h→12h→none`, mirroring
Stage 6's ring logic. Not required for the primary design.

**Independent test / exit criterion:**
- Setting a short test duration (e.g. 1–2 min) turns the fan off at the right time.
- Cancel prevents the shutoff.
- (If native path implemented) SW74 lands on the requested hour.

---

## Stage 9 : Consolidation, guard rails, dashboard, install

**Goal:** harden the logic and finish the user experience, then move to the final
install.

**Steps:**
1. **Guard rails:** cap every press loop at the ring length so a bad count can
   never spin indefinitely; add minimum spacing between logical commands so bursts
   don't overrun the fan's own debounce.
2. **Chained-command correctness:** verify that issuing several commands in quick
   succession only pays the wake cost once (first command) and all land correctly.
3. **Threshold tuning:** re-check `WAKE_THRESHOLD` against the fan's real timeout.
4. **Dashboard:** arrange power, mode select, oscillation, and sleep-timer controls
   in Home Assistant.
5. **Resync affordance:** add an HA "resync/known-state" button that drives the fan
   to a defined state and resets the model (useful after someone used the physical
   buttons).
6. **Install:** cut ESP32 power from USB to the fan's 5 V rail; secure the perfboard
   and ESP32 inside the base; strain-relieve; reassemble.

**Independent test / exit criterion (end-to-end):**
- Full run: power on → each of 5 modes → oscillation on/off → sleep timer → power
  off, all from Home Assistant, awake and after asleep timeouts.
- After a deliberate physical-button change, the resync control restores a known,
  correct state.
- Fan runs on its own 5 V rail (USB removed) with everything still functional.

---

## Testing philosophy (why staged)

Each stage adds exactly one new capability on top of a verified base:
network (1) → one press (2) → four presses (3) → wake logic + model (4) →
power (5) → mode (6) → oscillation (7) → timer (8) → hardening/install (9).
If a stage's exit test fails, the fault is isolated to that stage's addition ; no
whole-firmware debugging. Stages 2 and 4 are the highest-value checkpoints:
Stage 2 proves the physical actuation and timing; Stage 4 proves the open-loop
awake/asleep model that everything above depends on.


