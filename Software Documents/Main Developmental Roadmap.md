> **Project:** SSI High-Altitude Balloon Flight Computer  
> **MCU:** STM32F411CEU6 | **Firmware tree:** `balloon-project-stm32mx/`

---

## Document control

| Field | Value |
|---|---|
| Title | Main Developmental Roadmap |
| Location | `Software Documents/Main Developmental Roadmap.md` |
| Status | Draft for team review → Active after approval |
| Audience | Firmware engineers (including first-time embedded), hardware, ops |
| Related | HAB systems plan; KiCad schematics; `balloon-project-stm32mx.ioc`; datasheets |
| Source of truth (HW) | KiCad + CubeMX (not early `Balloon Project.md` part tables) |

### Revision history

| Rev | Date | Author | Notes |
|---|---|---|---|
| 0.1 | 2026-08-05 | Firmware planning | Initial roadmap from as-built HW + team Q&A |
| 0.2 | 2026-08-05 | Firmware | F0.1 complete: `App/` stubs (`app`, `error_flags`) + `main.c` wiring |

### How to use this document

1. Pick a phase whose **entry criteria** are met.  
2. Implement only that phase’s **work packages** (subsections).  
3. Complete **verification** and tick **exit criteria** before starting the next phase.  
4. Do not skip phases that share the SPI bus (F1 before F2–F4, F6, F7, F9).  
5. Record open defects in a simple issue list; blockers stay in §18.

---

## 1. Purpose and scope

### 1.1 Purpose

Provide a **phase-gated, modular firmware development roadmap** so any engineer can implement one work package safely, with industry-standard practices (layered drivers, fail-operational behavior, explicit entry/exit criteria, verification).

### 1.2 In scope

- Application firmware on STM32F411 (HAL + `App/` modules)
- Drivers: SPI bus, IMU, barometer, RTD temp, GPS, microSD/FatFS, LoRa, ArduCAM, APRS
- Mission state machine, telemetry packetizer, SD logging, imaging to SD
- Ground-station compatibility requirements for LoRa
- Integration, HIL, and flight-readiness gates

### 1.3 Out of scope

- PCB redesign / KiCad changes (except documenting needed bodges)
- Raspberry Pi / OpenHD (not the flight imaging path)
- Battery fuel-gauge (no ADC on board)
- Regulatory filings (NOTAM, etc.) except APRS license dependency

### 1.4 Mission success (software view)

Recoverable flight with: valid GPS (when sky visible), baro/temp/IMU logged, LoRa telemetry heard on ground, SD black box intact, photos on SD when camera works, APRS when licensed and configured.

---

## 2. System context (read once)

### 2.1 Boards

| Board | Role |
|---|---|
| Flight computer PCB | STM32 + sensors + LoRa + GPS + microSD + ArduCAM connector + power |
| SSI APRS daughterboard | DRA818V; UART + PTT + PD + audio from flight computer |

### 2.2 Confirmed as-built parts (team Q&A)

| Function | Part | Interface |
|---|---|---|
| MCU | STM32F411CEU6 @ 100 MHz (12 MHz HSE) | — |
| IMU | ICM-42688-P | SPI1 + `IMU_CS` + `IMU_INT1` |
| Barometer | MS5611-01BA03 | SPI1 + `BARO_CS` |
| Temperature | MAX31865 + **PT1000** 3-wire | SPI1 + `Temp_CS` |
| GPS | MAX-M10S | USART1 **9600 8N1** |
| LoRa | RFM95W 915 MHz | SPI1 + CS/RESET/DIO0 |
| Storage | microSD | SPI1 + CS; detect **high = present** |
| Camera | ArduCAM on `CAM_CONN` | SPI1 + I2C1 (SKU TBD — Gabe) |
| APRS | DRA818V (separate PCB) | USART2 + PTT/PD + PWM audio (**wired**) |

### 2.3 Locked electrical software constants

| Item | Value |
|---|---|
| GPS baud | 9600 (CubeMX currently 115200 — **must fix in F0**) |
| SD detect | High = present |
| APRS PTT | Low = TX, High = RX |
| APRS PD | Low = sleep, High = normal |
| APRS cable | Mirrored, pins face-to-face |
| SPI bring-up clock | ~1 MHz shared (raise later only if proven) |
| Photos | Store on microSD |
| LEDs / ARM switch / battery ADC | Not available — software must not depend on them |

### 2.4 Bus map

```text
SPI1 (PA5/PA6/PA7): IMU, MS5611, MAX31865, RFM95W, microSD, ArduCAM
I2C1 (PB6/PB7):     ArduCAM config only
USART1 (PA9/PA10):  GPS @ 9600
USART2 (PA2/PA3):   APRS AT commands
TIM2 CH1 (PA0):     APRS AFSK PWM
```

---

## 3. Architecture and coding standards (mandatory)

### 3.1 Layered architecture

```text
main.c (USER CODE only)  →  app_init / app_run
                              ↓
                         mission + packet
                              ↓
              drivers (imu, baro, temp, gps, sdlog, lora, camera, aprs)
                              ↓
                         spi_bus (CS ownership)
                              ↓
                         STM32 HAL
```

### 3.2 Repository layout

```text
balloon-project-stm32mx/
  Core/          # CubeMX — edit ONLY /* USER CODE BEGIN/END */ regions
  App/
    Inc/         # Public headers per module
    Src/         # One .c per module
  Makefile       # Every App/Src/*.c listed; -IApp/Inc
Software Documents/
  Main Developmental Roadmap.md   # THIS FILE
```

### 3.3 Coding practices (industry baseline for this project)

| Practice | Rule |
|---|---|
| Modularity | One device = one module (`imu.c` / `imu.h`); no cross-calling internals |
| CubeMX safety | Never edit generated code outside USER CODE blocks |
| APIs | `bool *_init(void);` + read/poll functions; return false on failure |
| Fail-operational | Set `error_flags`; **never** infinite-block the mission loop |
| Timeouts | All HAL SPI/I2C/UART calls use finite timeouts |
| Memory | No `malloc` in flight loop; fixed-size buffers only |
| Concurrency | v1 = bare-metal superloop; ISRs only set flags / push bytes |
| SPI | Single `spi_bus` owner; one CS low at a time |
| Types | Fixed-width (`uint8_t`, `int32_t`); explicit endianness in packets |
| Logging | Prefer SD CSV; optional SWD debug builds only |
| Versions | Boot log / packet field includes firmware version string |
| Reviews | PR for each phase exit when possible |

### 3.4 Module API template

Every driver header should follow this pattern:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { /* fields */ } imu_sample_t;

bool imu_init(void);                 /* false => hardware/ID fail */
bool imu_read(imu_sample_t *out);    /* false => comms fail */
bool imu_is_ok(void);                /* last known health */
```

### 3.5 Definition of Done (all phases)

A phase is done only when:

1. Code merged (or committed) under `App/` with Makefile updated  
2. Entry/exit criteria checklist completed  
3. Verification steps executed and noted (pass/fail + date)  
4. No critical defect open that blocks the next phase’s entry criteria  
5. Public header documented enough for another engineer to call the API  

---

## 4. Phase overview

| Phase | Name | Primary deliverable | Depends on |
|---|---|---|---|
| **F0** | Foundation & tooling | `App/` skeleton, standards, GPS baud 9600 | Assembled board or Nucleo path |
| **F1** | SPI bus services | `spi_bus` @ ~1 MHz | F0 |
| **F2** | IMU driver | ICM-42688-P samples | F1 |
| **F3** | Barometer driver | MS5611 pressure/altitude | F1 |
| **F4** | Temperature driver | MAX31865 PT1000 °C | F1 |
| **F5** | GPS driver | NMEA fix @ 9600 | F0 |
| **F6** | SD logging | FatFS append log | F1 |
| **F7** | LoRa telemetry TX | RFM95W packets | F1, packet draft, ground RX |
| **F8** | Mission + packetizer | State machine + 48B packet | F2–F5 (soft), F7 for live TX |
| **F9** | ArduCAM → SD | Images on card | F1, F6, **Gabe SKU** |
| **F10** | APRS | Position beacons | F5, **callsign/license** |
| **F11** | System integration (HIL) | End-to-end on hardware | F0–F10 as available |
| **F12** | Flight readiness | Software freeze + checklist | F11 pass |

**Parallelism:** F5 (GPS/UART) can proceed in parallel with F2–F4 after F0. F9/F10 can be prepared offline but must not block F7/F8 for chase-critical path.

**Chase-critical MVP path:** F0 → F1 → F2 → F3 → F4 → F5 → F6 → F7 → F8  
**Full mission path:** MVP + F9 + F10 + F11 + F12  

---

## 5. Phase F0 — Foundation and tooling

### 5.0 Objective

Establish a safe, regeneratable project structure and correct base peripheral config so later phases only add modules.

### 5.1 Entry criteria

- [ ] Repo builds with existing Makefile (or known build errors documented)
- [ ] SWD programming path identified (ST-Link / Nucleo)
- [ ] Team agrees modular `App/` layout (flexible per Q13; this roadmap mandates it)

### 5.2 Work packages

#### F0.1 — Create `App/` tree

**Status:** complete (2026-08-05)

- [x] Create `App/Inc/` and `App/Src/` (directories pre-existed; populated with stubs)
- [x] Add stub files: `app.c/h`, `error_flags.c/h`
- [x] Wire `main.c` USER CODE to call `app_init()` / `app_run()`

#### F0.2 — Makefile integration

- Add all new `.c` files to `C_SOURCES`
- Add `-IApp/Inc` to includes
- Confirm clean rebuild

#### F0.3 — Fix USART1 baud to 9600

- Update `.ioc` **or** set `huart1.Init.BaudRate = 9600` in USER CODE after `MX_USART1_UART_Init`
- Document: GPS default is 9600 per datasheet/team

#### F0.4 — Error flag framework

- Bitflags or bools: `imu_ok`, `baro_ok`, `temp_ok`, `gps_ok`, `sd_ok`, `lora_ok`, `cam_ok`, `aprs_ok`
- `app_run` must continue even if flags are false

#### F0.5 — Coding standard note in repo (short)

- Point developers to this roadmap §3
- Optional: `.clang-format` later (not required to exit F0)

### 5.3 Deliverables

- Compiling firmware with empty `app_run()` loop
- USART1 @ 9600
- Documented how to add a new module (3 steps: files, Makefile, include)

### 5.4 Verification / exit criteria

- [ ] Project builds without errors
- [ ] Firmware flashes and reaches `while(1)`
- [ ] Baud change present for USART1
- [ ] Another engineer can add `App/Src/foo.c` using the documented steps

---

## 6. Phase F1 — SPI bus services

### 6.0 Objective

Own the shared SPI1 bus safely for six slaves.

### 6.1 Entry criteria

- [ ] F0 complete
- [ ] All CS pins idle high after `MX_GPIO_Init` (already Cube default)

### 6.2 Work packages

#### F1.1 — Clock policy

- Set SPI prescaler for ~0.5–1 MHz (not Cube ÷2 / ~50 MHz)
- Optional helper `spi_bus_set_prescaler()` for later per-device speed

#### F1.2 — Transfer API

- `spi_bus_transfer(GPIO_TypeDef *cs_port, uint16_t cs_pin, const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_ms)`
- Assert CS low → HAL transfer → CS high
- Guarantee CS high on timeout/error paths

#### F1.3 — Register helpers (optional but recommended)

- `spi_read_reg8` / `spi_write_reg8` patterns for IMU/LoRa-style devices

### 6.3 Deliverables

- `spi_bus.c` / `spi_bus.h`
- Comment block listing max clocks per device (MAX31865 ≤5 MHz, etc.)

### 6.4 Verification / exit criteria

- [ ] CS lines idle high (meter or analyzer)
- [ ] Dummy transfer does not leave CS stuck low
- [ ] Timeout path releases CS

---

## 7. Phase F2 — IMU (ICM-42688-P)

### 7.0 Objective

Prove SPI + first sensor; provide motion data for burst detection later.

### 7.1 Entry criteria

- [ ] F1 complete

### 7.2 Work packages

#### F2.1 — Identity

- Read WHO_AM_I; compare to datasheet expected ID
- Fail `imu_init` if mismatch

#### F2.2 — Configuration

- Soft reset if required by datasheet
- Set accel/gyro full-scale and ODR suitable for HAB (e.g. 50–200 Hz ODR for logging; not mandatory 1 kHz)

#### F2.3 — Sample read

- Read accel XYZ + gyro XYZ into `imu_sample_t` (SI or raw + scale helpers)
- Optional: use `IMU_INT1` later; polling OK for v1

#### F2.4 — Health

- Update `imu_ok`; expose `imu_is_ok()`

### 7.3 Verification / exit criteria

- [ ] WHO_AM_I passes on hardware
- [ ] Values change when board is moved/tilted
- [ ] Init failure does not hang MCU

---

## 8. Phase F3 — Barometer (MS5611)

### 8.0 Objective

Pressure and barometric altitude for ascent/float/descent logic.

### 8.1 Entry criteria

- [ ] F1 complete (can parallel F2 after F1)

### 8.2 Work packages

#### F3.1 — Reset and PROM

- SPI reset command
- Read calibration coefficients C1–C6 from PROM
- CRC4 check if implementing datasheet CRC (recommended)

#### F3.2 — Conversion sequence

- Trigger D1 (pressure) and D2 (temp) conversions with chosen OSR
- Wait conversion time (or poll)
- Read ADCs

#### F3.3 — Compensation math

- Implement datasheet first-/second-order compensation
- Output: pressure (Pa or mbar), temperature (°C chip), altitude helper (standard atmosphere)

#### F3.4 — API

- `baro_init`, `baro_read`, altitude helper used by mission later

### 8.3 Verification / exit criteria

- [ ] Indoor pressure ~980–1040 hPa (site-dependent)
- [ ] Raising board ~1–2 m shows altitude change directionally
- [ ] PROM/CRC failure handled cleanly

---

## 9. Phase F4 — Temperature (MAX31865 + PT1000)

### 9.0 Objective

Outside-air temperature via RTD.

### 9.1 Entry criteria

- [ ] F1 complete
- [ ] Confirm PT1000 3-wire (team: yes per PCB; Gabe BOM confirm)

### 9.2 Work packages

#### F4.1 — Config

- Bias on, 3-wire mode, 50/60 Hz filter choice, conversion mode
- Keep SPI ≤ 5 MHz (already true if F1 ~1 MHz)

#### F4.2 — Read RTD

- Read RTD MSB/LSB; detect fault bits (open/short)
- Convert resistance → °C (Callendar-Van Dusen or library-equivalent for PT1000)

#### F4.3 — API + faults

- `temp_read` returns false on fault; set `temp_ok`

### 9.3 Verification / exit criteria

- [ ] Room-temp reading plausible (~15–30 °C)
- [ ] Hand on probe moves reading
- [ ] Disconnect fault (if safe to test) sets fault flag

---

## 10. Phase F5 — GPS (MAX-M10S)

### 10.0 Objective

Non-blocking NMEA parser providing fix for recovery and APRS/LoRa.

### 10.1 Entry criteria

- [ ] F0 complete (USART1 @ 9600)
- [ ] Antenna connected for outdoor fix tests

### 10.2 Work packages

#### F5.1 — RX path

- Interrupt or DMA into **fixed ring buffer**
- `gps_poll()` extracts lines ending in `\n`

#### F5.2 — Sentence parser

- Accept `$GPxxx`, `$GNxxx`, etc.
- Parse at least **GGA** (fix quality, alt, sats) and **RMC** (lat/lon, time, valid)
- Store scaled integers (e.g. lat/lon ×1e7) for packet use

#### F5.3 — Fix validity

- `gps_has_fix()` based on status fields — not merely “bytes received”
- Indoor: sentences OK without fix is expected

#### F5.4 — Optional UBX config (later)

- Raise baud to 115200 only after both sides agreed; not required for exit

### 10.3 Verification / exit criteria

- [ ] Bytes received at 9600 on bench/outdoor
- [ ] Outdoor: valid lat/lon within expected region
- [ ] Main loop never blocks waiting on GPS

---

## 11. Phase F6 — microSD logging (FatFS)

### 11.0 Objective

Black-box telemetry log; foundation for image storage.

### 11.1 Entry criteria

- [ ] F1 complete
- [ ] Industrial microSD available
- [ ] Detect polarity known (high = present)

### 11.2 Work packages

#### F6.1 — Middleware

- Integrate ChaN FatFs or CubeMX FatFS for SPI SD
- Low clock during card init if required by stack

#### F6.2 — Mount / file policy

- Check `microSD_detect` then mount
- Create `FLIGHTxxx.CSV` (or next index)
- Header row once

#### F6.3 — Append API

- `sdlog_write_sample(...)` non-fatal on failure
- Periodic `f_sync` / flush every N seconds

#### F6.4 — Fail-soft

- Missing card ⇒ `sd_ok = false`; mission continues

### 11.3 Verification / exit criteria

- [ ] File visible on PC after flight-sim loop
- [ ] Pull-power mid-write test: document corruption mitigations (flush policy)
- [ ] No hang when card absent

---

## 12. Phase F7 — LoRa telemetry (RFM95W)

### 12.0 Objective

Ground-receivable telemetry (primary recovery link).

### 12.1 Entry criteria

- [ ] F1 complete
- [ ] Second RFM95W + Nucleo (or equiv.) for RX — **schedule with ops (Q20)**
- [ ] Interim RF + packet proposal accepted or frozen (see §12.2.3)

### 12.2 Work packages

#### F7.1 — Reset and probe

- Hardware reset via `LoRa_RESET`
- Read version register (SX1276)

#### F7.2 — Radio config

- Frequency 915.x MHz (US ISM), mode LoRa
- Starting proposal: **SF8, BW125 kHz, CR4/5**, CRC on, sync word documented
- TX power within module/legal limits; thermal/duty awareness

#### F7.3 — Packet TX

- Load FIFO; TX; wait DIO0 or timeout
- Sequence number increment

#### F7.4 — Ground station

- Matching modem settings + decoder for packet layout
- Log RSSI/SNR

### 12.3 Proposed packet v1 (freeze as team Q16)

| Offset | Len | Field |
|---|---|---|
| 0 | 1 | `version = 0x01` |
| 1 | 1 | `mission_state` |
| 2 | 2 | `seq` |
| 4 | 4 | `time_ms` |
| 8 | 4 | `lat_e7` |
| 12 | 4 | `lon_e7` |
| 16 | 2 | `gps_alt_m` |
| 18 | 2 | `baro_alt_m` |
| 20 | 2 | `temp_c_x100` |
| 22 | 2 | `batt = 0xFFFF` (N/A) |
| 24 | 1 | `flags` |
| 25 | 1 | `sats` |
| 26 | 2 | `crc16` |

### 12.4 Verification / exit criteria

- [ ] Bench: RX node receives packets with increasing `seq`
- [ ] CRC validated on ground
- [ ] TX rate limited (e.g. ≤0.5 Hz) during test

---

## 13. Phase F8 — Mission state machine and packetizer

### 13.0 Objective

Autonomous flight behavior and unified telemetry packing.

### 13.1 Entry criteria

- [ ] At least baro **or** GPS providing altitude (prefer both)
- [ ] F7 available for live TX (can unit-test SM without radio)

### 13.2 Work packages

#### F8.1 — State enum and transitions

States: `PAD`, `ARMED`, `ASCENT`, `FLOAT`, `BURST`, `DESCENT`, `LANDED`, `BEACON`

Rules:

- No mag-switch: auto ARMED after healthy init + optional hold
- ASCENT: sustained positive altitude rate
- FLOAT: high altitude + near-zero rate
- BURST: large negative rate and/or IMU freefall — **latched**
- LANDED: altitude stable >60 s
- BEACON: periodic GPS TX

#### F8.2 — Schedulers

| State | LoRa rate | Camera | Notes |
|---|---|---|---|
| ASCENT | ~0.5 Hz | ~30 s | |
| FLOAT | ~0.2 Hz | ~30 s | |
| DESCENT | ~0.5 Hz | optional | |
| BEACON | ~1/60 s | off | |

#### F8.3 — Packetizer

- Fill packet v1 from latest samples + flags
- CRC16

#### F8.4 — Host-side state tests (recommended)

- Feed simulated altitude profiles; assert state sequence

### 13.3 Verification / exit criteria

- [ ] Simulated profile walks PAD→…→BEACON correctly
- [ ] BURST does not clear when descent slows
- [ ] Live: rates match table within tolerance

---

## 14. Phase F9 — ArduCAM imaging → SD

### 14.0 Objective

Capture flight imagery to microSD without breaking telemetry deadlines.

### 14.1 Entry criteria

- [ ] F1 + F6 complete
- [ ] **Gabe confirms ArduCAM SKU** (2MP Mini B0067 vs 5MP OV5642) — **blocker for correct driver**
- [ ] Interim assumption if forced: schematic Mini 2MP

### 14.2 Work packages

#### F9.1 — Sensor bring-up

- I2C ID/probe; sensor init tables for confirmed SKU
- SPI FIFO / frame read path

#### F9.2 — Capture service

- `camera_capture_to_sd(const char *path)`
- Stream in chunks; **do not** allocate full-frame RAM buffers beyond chip capacity

#### F9.3 — Mission integration

- Trigger on mission timer in ASCENT/FLOAT
- Yield SPI between chunks so LoRa/SD can run
- Respect ArduCAM thermal limits (capture earlier if cold risk)

### 14.3 Verification / exit criteria

- [ ] Valid image file opens on PC
- [ ] During capture, LoRa still meets minimum beacon rate (measure)
- [ ] Failure sets `cam_ok = false` without reboot loop

---

## 15. Phase F10 — APRS (DRA818V)

### 15.0 Objective

Backup VHF position beacons.

### 15.1 Entry criteria

- [ ] F5 GPS fix available for payload
- [ ] APRS cable correct (mirrored face-to-face)
- [ ] Audio path connected (team confirmed)
- [ ] **Amateur callsign + license** (Q19) before on-air RF

### 15.2 Work packages

#### F10.1 — Power and AT config

- PD high (normal); PTT high (RX) idle
- UART AT: frequency 144.390 MHz, volume, squelch per datasheet

#### F10.2 — AFSK generator

- TIM2 PWM → 1200/2200 Hz Bell 202
- AX.25 UI frame with callsign, lat/lon, altitude

#### F10.3 — PTT sequencing

- PTT low → send audio → PTT high
- Duty cycle / interval (e.g. aligned with BEACON or slower)

#### F10.4 — Dry-run mode

- Build flag `APRS_RF_ENABLE=0` until license confirmed

### 15.3 Verification / exit criteria

- [ ] AT commands ACK
- [ ] Audio tones verified on scope before RF
- [ ] On-air only with license: received on APRS client / HT

---

## 16. Phase F11 — System integration (HIL)

### 16.0 Objective

Prove combined stack under realistic conditions.

### 16.1 Entry criteria

- [ ] Chase-critical path F0–F8 implemented
- [ ] F9/F10 as far as unblocked

### 16.2 Work packages

#### F11.1 — Full-stack soak

- ≥1 hour continuous run; SD log continuous; LoRa RX count
- Inject GPS loss / SD remove; confirm fail-soft

#### F11.2 — RF range smoke test

- Ground LOS range check (not full 100 km claim)

#### F11.3 — Cold / freezer test (ops + firmware)

- Watch crystal UART, SD writes, brown-out behavior
- Document failures

#### F11.4 — Defect burn-down

- Fix P0/P1 bugs only for flight; defer cosmetics

### 16.3 Exit criteria

- [ ] Written HIL report: pass/fail per subsystem
- [ ] No open critical defects on GPS, SD, LoRa, mission SM

---

## 17. Phase F12 — Flight readiness and software freeze

### 17.0 Objective

Ship a known binary to the pad.

### 17.1 Checklist

- [ ] Firmware version tagged (e.g. `flight-YYYYMMDD-vX.Y`)
- [ ] Packet + LoRa params documented and matched on ground RX
- [ ] Callsign programmed if APRS enabled
- [ ] SD card formatted/empty with known good card
- [ ] Boot shows expected `*_ok` flags on pad (GPS may wait for sky)
- [ ] Go/No-Go: chase-critical path green

### 17.2 Exit criteria

- [ ] Tagged release binary archived
- [ ] Ground station checklist signed
- [ ] Rollback plan: previous tag kept

---

## 18. Open items and external dependencies

| ID | Item | Owner | Blocks |
|---|---|---|---|
| O1 | Exact ArduCAM SKU | Gabe (BOM) | F9 driver correctness |
| O2 | BOM confirm PT1000 | Gabe | F4 confidence |
| O3 | CubeMX `.ioc` single owner | Team | Merge conflicts |
| O4 | RadioLib vs HAL | Team — **roadmap standard = HAL** | F7 style |
| O5 | Freeze packet + SF/BW/freq | Firmware + ground | F7/F8 |
| O6 | Ham license / callsign | Ops | F10 on-air |
| O7 | Ground RX ready date | Ops | F7 validation |

---

## 19. Risk register (software)

| Risk | Mitigation |
|---|---|
| Shared SPI starvation by camera | Chunked capture; mission priority to LoRa |
| GPS baud mismatch | F0 forces 9600 |
| CubeMX regen wipes work | USER CODE only; App/ outside Core |
| APRS illegal TX | `APRS_RF_ENABLE` gate |
| Cold SD corruption | Industrial card; flush; power-loss tests |
| Scope creep | Phase gates; chase-critical path first |

---

## 20. Quick-start for a new implementer

1. Read §2 (system) and §3 (standards).  
2. Find the lowest phase with unmet exit criteria.  
3. Confirm that phase’s entry criteria.  
4. Implement only listed work packages.  
5. Run verification; check exit boxes.  
6. Open PR titled `firmware: complete Phase Fx — <name>`.  

**First coding task for the project:** Phase **F0**, then **F1**, then **F2** (IMU WHO_AM_I).

---


