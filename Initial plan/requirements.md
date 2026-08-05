# Requirements — Smart Fan Retrofit (Philips CX2550/00)

*Status: locked. All open questions resolved from bench measurements and manual.*

## 1. Project goal

Retrofit a Philips 2000 Series pedestal fan (CX2550/00) so all functions can be
controlled from Home Assistant, without altering the fan's mains wiring and
without removing original functionality. The fan's physical buttons keep working;
the modification is fully reversible.

An added ESP32 running ESPHome electronically "presses" the fan's four existing
buttons via optocouplers. It integrates with Home Assistant (on a Raspberry Pi)
over the native ESPHome API. Home Assistant provides the UI, dashboards,
automations and scheduling — there is no custom app/PWA.

**Control is open-loop (no feedback):** LED sensing was evaluated and dropped
(see §5). The ESP32 tracks state internally rather than reading it back.

## 2. Target hardware (the fan) — as measured

| Item | Detail |
| --- | --- |
| Fan model | Philips 2000 Series pedestal fan, CX2550/00 |
| Control board | Midea / GD `FS35-24CR` (silkscreen `FS35-24CR / FS35-24CRW-Y`) |
| Motor | `YSF-12-4-212`, GD Midea Environment Appliances (PSC induction) |
| Oscillation | Separate synchronous gearmotor, board-switched |
| Mains input | `FUSE21 3.15A/250VAC`, terminals `ACL` / `ACN` (DO NOT TOUCH) |
| Logic rail | **5 V** (confirmed: ~5 V across centre electrolytic cap; buttons idle at ~5 V) |
| 5 V source pad | **Positive leg of the centre electrolytic capacitor** near the buzzer |
| Ground | **Negative (stripe) leg of that cap**; `CN3` GND pin confirmed continuous to it |
| CN3 header (`IR/GND/5V`) | 5 V pin **dead** on this variant; GND pin usable as ground tap |
| Buttons (populated) | `SW71` ON/OFF, `SW72` SPEED/MODE, `SW73` oscillation, `SW74` TIMER |
| Indicator LEDs | Populated but **NOT used** — multiplexed, ~0.9 V lit, awkward to read |
| IR receiver | Footprint unpopulated — not used |

## 3. Confirmed control behaviour (manual + bench)

### 3.1 Power — `SW71`
- One press toggles the fan on/off **when the board is "awake"** (LEDs lit).
- See §3.5 for the awake/asleep double-press rule.

### 3.2 Speed / mode — `SW72`
Five-position cycle, **off is NOT in this cycle** (off only via `SW71`):
```
speed 1 → speed 2 → speed 3 → sleep → natural wind → (back to) speed 1
```
- Sleep mode: gradually reduces speed to lowest over ~30 min.
- Natural wind: varies speed to mimic natural breeze.

### 3.3 Oscillation — `SW73`
- Simple on/off toggle. Sweeps up to 90°.
- "Stop where it is" = toggle OFF → head freezes at current angle.

### 3.4 Timer — `SW74`
- Cycles `1h → 2h → 3h → 4h → 8h → 12h` (plus none), bidirectional
  (ends → turns fan off if on, on if off).
- **Primary timing is handled by a Home Assistant automation** (more flexible,
  bidirectional). `SW74` is wired optionally/secondary.

### 3.5 The awake/asleep LED-timeout rule (critical)
- Any state change lights the relevant indicator LED(s).
- LEDs **auto-extinguish ~1 minute** after the last action; the fan keeps
  running unchanged.
- **When "awake" (LEDs lit):** the first press of any button performs its action.
- **When "asleep" (>~1 min idle, LEDs dark):** the first press of ANY button only
  **wakes** the board (no state change); a **second** press performs the action.
- This applies to **all four buttons** (confirmed).

## 4. Functional requirements

| # | Requirement | How met |
| --- | --- | --- |
| R1 | On/off remotely | ESP32 pulses `SW71` (with awake/asleep handling) |
| R2 | Change speed/mode | ESP32 cycles `SW72` to target in the 5-ring; exposed in HA as a 5-option `select` |
| R3 | Oscillate + stop at position | ESP32 toggles `SW73` |
| R4 | Sleep-timer config | Home Assistant automation (delayed on/off). `SW74` optional |
| R5 | Remote UI | Home Assistant app/dashboard |
| R6 | HA integration | ESPHome native API to HA on the Raspberry Pi |
| R7 | Preserve original operation | Taps in parallel with buttons; physical buttons still work |
| R8 | Reversible & safe | No mains rewiring; optically isolated button drive; removable via terminal |

## 5. Why no LED sensing (recorded decision)

The indicator LEDs read ~0.9 V to ground when lit and behave as a multiplexed,
low-side-driven matrix. This is: (a) too low to drive a PC817 input (needs >1.1 V);
(b) too low for a reliable direct 3.3 V GPIO read; (c) a scanned signal needing
time-sampling. The hardware+firmware cost of sensing is high while the benefit for
a fan is marginal (feedback only matters if someone uses the physical buttons, a
rare and self-correcting case). Decision: **open-loop control, no sensing**
(Strategy C — see architecture §7). Sensing the single ON LED remains a documented
future upgrade path if power-toggle reliability ever proves annoying.

## 6. Non-functional requirements

- No mains modification; ESP32 interfaces only at logic level (buttons, 5 V).
- Button drive optically isolated (PC817) between ESP32 and fan board.
- Fallback: if Pi/HA offline, fan fully usable via physical buttons.
- Serviceable: all taps land on a screw terminal block; ESP32 socketed on headers.
- Open-loop accepted limitation: physical-button use causes model drift, corrected
  on subsequent HA commands.

## 7. Bill of materials (specific, final)

### 7.1 Controller
| Qty | Part | Model | Notes |
| --- | --- | --- | --- |
| 1 | ESP32 dev board | **AZ-Delivery ESP32-S DevKit C V4** (ESP32-WROOM-32, 4 MB flash, 512 KB SRAM) | Configured in ESPHome as generic `esp32` / board `esp32dev`. USB chip: **CH340** (install CH340 driver for first flash). "S" in the name = classic ESP32, NOT S2/S3. |
| 2 | Female header strip | 2.54 mm, cut to the board's two pin rows | Socket the ESP32 (removable, no direct soldering). |

### 7.2 Button emulation (4 buttons)
| Qty | Part | Model | Purpose |
| --- | --- | --- | --- |
| 4 (buy ~10) | Optocoupler | **Sharp PC817** (bare 4-pin DIP) | One per button; output transistor across the button, isolated from ESP32. |
| 4 | Resistor | ~330 Ω, 1/4 W | Sets PC817 input-LED current from 3.3 V GPIO. |

Wiring per channel: `ESP32 GPIO → 330Ω → PC817 pin1 (anode)`, `PC817 pin2
(cathode) → ESP32 GND`; `PC817 pin4 (collector) → button MCU-side pad`,
`PC817 pin3 (emitter) → fan GND`.

### 7.3 LED sensing
**None.** (See §5.)

### 7.4 Power tap
| Qty | Part | Model | Purpose |
| --- | --- | --- | --- |
| — | (fan's own 5 V) | +5 V from centre electrolytic cap positive leg; GND from its stripe leg (or CN3 GND pin) | Powers ESP32 via its 5 V/VIN input. |
| 1 (optional) | Electrolytic cap | 470 µF / 16 V | Local decoupling across 5 V/GND for Wi-Fi current spikes. |
| 1 (optional) | Buck/LDO | MP1584EN buck **or** AMS1117-3.3 | Only if the 5 V rail proves weak/noisy. |

### 7.5 Interconnect & assembly
| Qty | Part | Spec | Purpose |
| --- | --- | --- | --- |
| 1 | Perfboard | ~4×6 cm | Hosts PC817s, resistors, terminal, ESP32 socket. |
| 1 | Screw terminal block | 2.54/5.08 mm | Single disconnect point for fan-board taps. |
| reel | Signal wire | **30 AWG solid-core, multi-colour** (Kynar wire-wrap) | Button + GND taps and perfboard runs. |
| short | Power wire | 24–26 AWG (red/black) | 5 V + GND power pair. |
| — | Solder / flux | 0.5–0.8 mm 63/37; no-clean flux | Fine joints. |
| — | Kapton tape + hot glue | — | Insulation + strain relief on taps. |
| — | Heat-shrink | assorted small | Insulate tap wires. |

### 7.6 Tools
- Temp-controlled fine-tip soldering iron (~320–350 °C)
- Multimeter (continuity + DC/AC volts)
- Fine tweezers, PCB holder/helping hands, optional magnifier
- ESPHome (standalone install on the Pi, accessed at its own IP:port) — used to
  write, compile and flash device firmware
- 5 V ≥1 A (ideally 2 A) USB supply/power bank for the testing stages

### 7.7 Existing infrastructure (actual setup)
- **Raspberry Pi 4** running **Home Assistant Container** (Docker), NOT Home
  Assistant OS. HAOS was not used because **AdGuard** already runs on the Pi and
  the Container approach coexists with it.
- **ESPHome runs standalone** (its own container/service) at its own **IP:port**,
  reachable directly — it is NOT the HAOS ESPHome add-on. It still integrates with
  Home Assistant over the device's native API.
- Consequence for device onboarding: HA may **not auto-discover** the ESP32
  (mDNS/discovery is less reliable under Container/Docker networking). If it does
  not appear automatically, add it manually: HA → Settings → Devices & Services →
  Add Integration → ESPHome → host = ESP32 IP, port = **6053**, then paste the
  device's API **encryption key**.
- Recommended: set a **DHCP reservation** for the ESP32 so its IP is stable
  (referenced by IP for manual add and for reliability).
- 2.4 GHz Wi-Fi (ESP32 is 2.4 GHz only)

## 8. Power plan by stage
- **Testing (all stages):** power the ESP32 from a **USB 5 V supply** (independent
  of the fan) while probing/pressing against the fan (fan on mains as normal,
  soldering done with fan unplugged).
- **Final install:** power the ESP32 from the **fan's 5 V rail** (centre cap),
  optionally with the 470 µF local cap.

## 9. Bench values captured (for reference)
- Logic rail: **~5 V** (centre electrolytic cap; buttons idle ~5 V).
- CN3 `5V` pin: **dead**; CN3 `GND` pin: **continuous with board ground** (usable).
- LED driven leg, lit, to ground: **~0.9 V**, multiplexed → sensing dropped.

## 10. Out of scope
- Any mains/motor-tap/triac modification.
- Arbitrary-angle oscillation positioning (only on/off "stop where it is").
- Custom PWA/web UI (superseded by Home Assistant).
- MQTT (native ESPHome API used).
- LED state sensing (documented future upgrade only).
