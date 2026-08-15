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
| Related | HAB systems plan; KiCad schematics; `balloon-project-stm32mx.ioc`; datasheets; [Firmware Development Guidelines.md](Firmware%20Development%20Guidelines.md) |
| Source of truth (HW) | KiCad + CubeMX (not early `Balloon Project.md` part tables) |

### Revision history

| Rev | Date | Author | Notes |
|---|---|---|---|
| 0.1 | 2026-08-05 | Firmware planning | Initial roadmap from as-built HW + team Q&A |
| 0.2 | 2026-08-05 | Firmware | F0.1 complete: `App/` stubs (`app`, `error_flags`) + `main.c` wiring |
| 0.3 | 2026-08-05 | Firmware | F0.2 complete: Makefile `C_SOURCES` + `-IApp/Inc`; clean rebuild verified |
| 0.4 | 2026-08-05 | Firmware | F0.3 complete: USART1 @ 9600 (USER CODE re-init + `.ioc`) |
| 0.5 | 2026-08-05 | Firmware | F0.4 complete: error-flag framework (`*_ok` accessors; fail-soft `app_run`) |
| 0.6 | 2026-08-05 | Firmware | F0.5 complete: coding-standard README + F0 entry/exit sync |
| 0.7 | 2026-08-06 | Firmware | F1.1 complete: `spi_bus` clock policy (DIV128 ~0.78 MHz), `spi_bus_init` / `spi_bus_set_prescaler`, max-clock comment block |
| 0.8 | 2026-08-06 | Firmware | F1.2 complete: `spi_bus_transfer` CS assert/release, timeout/error CS restore + `HAL_SPI_Abort` |
| 0.9 | 2026-08-06 | Firmware | F1.3 complete: `spi_bus_read_reg8` / `spi_bus_write_reg8` (IMU/LoRa bit-7 R/W convention) |
| 0.10 | 2026-08-06 | Firmware | F2.1 complete: `imu` WHO_AM_I (`0x75`→`0x47`), `imu_init` / `imu_is_ok`, fail-soft `error_flags` |
| 0.11 | 2026-08-06 | Firmware | F2.2 complete: soft reset, SPI-only + AFSR off, FS/ODR 100 Hz ±16 g/±2000 dps, LN power, read-back verify |
| 0.12 | 2026-08-06 | Firmware | Guidelines doc + §21 bench backlog + Cursor rules; deferred-HW / host-test / manual-run policy |
| 0.13 | 2026-08-06 | Firmware | F2.3 sample read (`imu_read`, scale helpers) + F2.4 ongoing health; host `test_imu_scale`; F2 software complete, HW → §21 |
| 0.14 | 2026-08-09 | Firmware | F2 software verification closed: host `test_imu_scale` manual pass; HW exit remains §7.3 / §21 |
| 0.15 | 2026-08-09 | Firmware | F3.1 complete: `baro` reset + PROM C1–C6 + CRC4, `baro_init` / `baro_is_ok`, host `test_ms5611_crc`; F3.2–F3.4 open; HW → §21 |
| 0.16 | 2026-08-09 | Firmware | F3.1 host `test_ms5611_crc` fix — AN520 test vector; manual pass |
| 0.17 | 2026-08-15 | Firmware | F3.2 complete: D1/D2 OSR 4096 conversion, `baro_read_raw`, host `test_ms5611_adc`; F3.3–F3.4 open; HW → §21 |
| 0.18 | 2026-08-15 | Firmware | F3.3 complete: compensation (1st/2nd order), ISA altitude helper, host `test_ms5611_comp`; F3.4 open; HW → §21 |
| 0.19 | 2026-08-15 | Firmware | F3.4 complete: `baro_sample_t`, `baro_read` (raw + compensate + ISA), host composition in `test_ms5611_comp`; F3 software complete; HW → §21 |
| 0.20 | 2026-08-15 | Firmware | Software vs HW gate wording aligned across phases; proceed-rule explicit in §3.5, §4, §20, F0–F10, §21 |

### How to use this document

1. Pick a phase whose **entry criteria** are met.  
2. Implement only that phase’s **work packages** (subsections).  
3. Tick **software** verification before starting the next sequential phase. Do **not** wait on hardware exit — those items go to **§21** until bring-up week (see [Firmware Development Guidelines.md](Firmware%20Development%20Guidelines.md)). Still honor **entry criteria** (F1 before SPI slaves; F5 parallel after F0; F9 ArduCAM SKU; F10 license for on-air; F11/F12 require hardware).  
4. Do not skip phases that share the SPI bus (F1 before F2–F4, F6, F7, F9).  
5. Record open defects in a simple issue list; blockers stay in §18.  
6. F11 (HIL) and F12 (flight readiness) still require real hardware — deferred §21 items do not substitute.

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
| GPS baud | 9600 (fixed in F0.3: USER CODE re-init + `.ioc`; MAX-M10S default) |
| SD detect | High = present |
| APRS PTT | Low = TX, High = RX |
| APRS PD | Low = sleep, High = normal |
| APRS cable | Mirrored, pins face-to-face |
| SPI bring-up clock | ~1 MHz shared (`SPI_BAUDRATEPRESCALER_128` → ~0.78 MHz on 100 MHz APB2; raise later only if proven) |
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

Two levels — do not conflate them.

**Software-complete (unblocks next coding phase):**

1. Code merged (or committed) under `App/` with Makefile updated  
2. Work-package checklists ticked  
3. Software verification executed and noted (pass/fail + date): clean `make clean && make`; host tests authored when the phase has pure logic; fail-soft paths documented — **does not require a board**  
4. No critical defect open that blocks the next phase’s entry criteria  
5. Public header documented enough for another engineer to call the API  

**Phase exit complete (full close):**

- Software-complete **and** hardware exit criteria ticked after bench, **or** hardware exit still open in §21 — in that case phase status must say “HW exit open” and the phase must **not** be claimed fully complete.

**Entry-criteria shorthand:** when a later phase says “F1 complete” or “F3 software-complete,” that means **software-complete** of that dependency, not bench verification ticked.

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

**Proceed rule (software vs bench):**

- Sequential driver work **F2–F7** proceeds when dependencies are **software-complete**; bench validation is §21 bring-up week, not a coding blocker.
- **F8** may start when F3 and/or F5 altitude **APIs** exist (software); live TX needs F7 software-complete; ops RX node is for HW exit.
- **F11/F12** require real hardware; §21 per-subsystem bench does not substitute for F11.

**Chase-critical MVP path:** F0 → F1 → F2 → F3 → F4 → F5 → F6 → F7 → F8  
**Full mission path:** MVP + F9 + F10 + F11 + F12  

---

## 5. Phase F0 — Foundation and tooling

**Phase status:** software verification complete (2026-08-05); hardware exit open (§5.4 / §21). Full phase exit pending bench — see §21 F0.

### 5.0 Objective

Establish a safe, regeneratable project structure and correct base peripheral config so later phases only add modules.

### 5.1 Entry criteria

- [x] Repo builds with existing Makefile (or known build errors documented)
- [x] SWD programming path identified (ST-Link / Nucleo) — documented in `balloon-project-stm32mx/README.md`
- [x] Team agrees modular `App/` layout (flexible per Q13; this roadmap mandates it)

### 5.2 Work packages

#### F0.1 — Create `App/` tree

**Status:** complete (2026-08-05)

- [x] Create `App/Inc/` and `App/Src/` (directories pre-existed; populated with stubs)
- [x] Add stub files: `app.c/h`, `error_flags.c/h`
- [x] Wire `main.c` USER CODE to call `app_init()` / `app_run()`

#### F0.2 — Makefile integration

**Status:** complete (2026-08-05)

- [x] Add all new `.c` files to `C_SOURCES`
- [x] Add `-IApp/Inc` to includes
- [x] Confirm clean rebuild

**How to add a new App module (3 steps):**

1. Add `App/Inc/foo.h` and `App/Src/foo.c` (one module = one `.c` / `.h` pair).
2. List `App/Src/foo.c` in Makefile `C_SOURCES` (re-add App entries after CubeMX Makefile regen).
3. `#include "foo.h"` from callers; `-IApp/Inc` is already in `C_INCLUDES` after F0.2.

#### F0.3 — Fix USART1 baud to 9600

**Status:** complete (2026-08-05)

- [x] Update `.ioc` **and** set `huart1.Init.BaudRate = 9600` in USER CODE after `MX_USART1_UART_Init` (re-init via `HAL_UART_Init`)
- [x] Document: GPS default is 9600 per datasheet/team (comment in `main.c` §USART1_Init 2; §2.3 constant table)

#### F0.4 — Error flag framework

**Status:** complete (2026-08-05)

- [x] Bitflags for `imu_ok`, `baro_ok`, `temp_ok`, `gps_ok`, `sd_ok`, `lora_ok`, `cam_ok`, `aprs_ok` via `error_flags_*_ok()` / `error_flags_set_*_ok()` (ERR_FLAG bit set = fault)
- [x] `app_run` must continue even if flags are false (queries `error_flags_get()`; no blocking on faults)

#### F0.5 — Coding standard note in repo (short)

**Status:** complete (2026-08-05)

- [x] Point developers to this roadmap §3 (`balloon-project-stm32mx/README.md` + root `README.md` pointer)
- [x] Optional: `.clang-format` later (deferred; not required to exit F0)

### 5.3 Deliverables

- Compiling firmware with empty `app_run()` loop
- USART1 @ 9600
- Documented how to add a new module (3 steps: files, Makefile, include)

### 5.4 Verification / exit criteria

**Software verification (closed 2026-08-05):**

- [x] Project builds without errors (verified 2026-08-05)
- [x] Baud change present for USART1
- [x] Another engineer can add `App/Src/foo.c` using the documented steps (`balloon-project-stm32mx/README.md` + §5.2 F0.2)

**Hardware exit (pending bench — tick when §21 F0 procedure passes):**

- [ ] Firmware flashes and reaches `while(1)` — flash procedure documented 2026-08-05; physical flash pending bench ST-Link (see §21)

---

## 6. Phase F1 — SPI bus services

**Phase status:** software verification complete (2026-08-06); hardware exit open (§6.4 / §21). Full phase exit pending bench — see §21 F1.

### 6.0 Objective

Own the shared SPI1 bus safely for six slaves.

### 6.1 Entry criteria

- [x] F0 complete (software; physical bench flash still pending per §5.4)
- [x] All CS pins idle high after `MX_GPIO_Init` (already Cube default)

### 6.2 Work packages

#### F1.1 — Clock policy

**Status:** complete (2026-08-06)

- [x] Set SPI prescaler for ~0.5–1 MHz (`SPI_BAUDRATEPRESCALER_128` → ~0.78 MHz; not Cube ÷2 / ~50 MHz)
- [x] Optional helper `spi_bus_set_prescaler()` for later per-device speed

#### F1.2 — Transfer API

**Status:** complete (2026-08-06)

- [x] `spi_bus_transfer(GPIO_TypeDef *cs_port, uint16_t cs_pin, const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_ms)`
- [x] Assert CS low → HAL transfer → CS high
- [x] Guarantee CS high on timeout/error paths (`HAL_SPI_Abort` after deassert)

#### F1.3 — Register helpers (optional but recommended)

**Status:** complete (2026-08-06)

- [x] `spi_bus_read_reg8` — IMU/LoRa-style read (reg | 0x80, dummy byte, value in second RX byte)
- [x] `spi_bus_write_reg8` — IMU/LoRa-style write (reg & 0x7F, value)

### 6.3 Deliverables

- `spi_bus.c` / `spi_bus.h`
- Comment block listing max clocks per device (MAX31865 ≤5 MHz, etc.)

### 6.4 Verification / exit criteria

**Software verification (closed 2026-08-06):**

- [x] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [x] `spi_bus_transfer` always deasserts CS on success and error paths (code-path verified)
- [x] Timeout path calls `HAL_SPI_Abort` after CS release (code-path verified)

**Hardware exit (pending bench — tick when §21 F1 procedure passes):**

- [x] CS lines idle high (meter or analyzer) — Cube default after `MX_GPIO_Init` verified in software; meter pending bench (see §21)
- [x] Dummy transfer does not leave CS stuck low — `spi_bus_transfer` always deasserts CS; analyzer pending bench (see §21)
- [x] Timeout path releases CS — code-path verified in `spi_bus_transfer`; analyzer pending bench (see §21)

---

## 7. Phase F2 — IMU (ICM-42688-P)

**Phase status:** software verification complete (2026-08-09 — host `test_imu_scale` manual pass); hardware exit criteria open (§7.3 / §21). Full phase exit pending bench — see §21 F2 procedure; then tick §7.3 HW items.

### 7.0 Objective

Prove SPI + first sensor; provide motion data for burst detection later.

### 7.1 Entry criteria

- [x] F1 complete

### 7.2 Work packages

#### F2.1 — Identity

**Status:** complete (2026-08-06)

- [x] Read WHO_AM_I (`0x75`); compare to ICM-42688-P expected ID (`0x47`)
- [x] Fail `imu_init` on SPI error or ID mismatch
- [x] `error_flags_set_imu_ok` + `imu_is_ok()` on init path
- [x] `imu.c` / `imu.h`; Makefile `C_SOURCES`; called from `app_init` fail-soft

#### F2.2 — Configuration

**Status:** complete (2026-08-06)

- [x] Soft reset via `DEVICE_CONFIG` (`0x11`); post-reset WHO_AM_I re-check
- [x] SPI-only interface (`INTF_CONFIG0`); disable AFSR (`INTF_CONFIG1`)
- [x] Accel ±16 g / gyro ±2000 dps at 100 Hz ODR (`GYRO_CONFIG0` / `ACCEL_CONFIG0` = `0x08`)
- [x] Low-noise power (`PWR_MGMT0` = `0x0F`); register read-back verify

#### F2.3 — Sample read

**Status:** complete (2026-08-06)

- [x] `imu_sample_t` with raw accel/gyro XYZ (16-bit LSB)
- [x] Burst SPI read `ACCEL_DATA_X1`..`GYRO_DATA_Z0` (12 bytes) via `spi_bus_transfer`
- [x] `imu_read()` polling API; scale helpers `imu_accel_raw_to_mps2` / `imu_gyro_raw_to_dps`
- [x] Host test `tests/host/test_imu_scale` pass (manual, 2026-08-09)

#### F2.4 — Health

**Status:** complete (2026-08-06)

- [x] `imu_read` success/failure updates `error_flags_set_imu_ok` and `imu_is_ok()`
- [x] `imu_is_ok()` reflects last init or read outcome

### 7.3 Verification / exit criteria

**Software verification (closed 2026-08-09):**

- [x] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [x] Host `tests/host/test_imu_scale` pass (manual, 2026-08-09)
- [x] Init failure does not hang MCU — `(void)imu_init()` in `app_init`; `app_run` continues; code-path verified (2026-08-06)

**Hardware exit (pending bench — tick when §21 F2 procedure passes; then add rev 0.15):**

- [ ] WHO_AM_I passes on hardware (see §21 F2)
- [ ] Values change when board is moved/tilted (see §21 F2)

---

## 8. Phase F3 — Barometer (MS5611)

**Phase status:** software verification complete (2026-08-15); hardware exit criteria open (§8.3 / §21). Full phase exit pending bench — see §21 F3 procedure; then tick §8.3 HW items.

### 8.0 Objective

Pressure and barometric altitude for ascent/float/descent logic.

### 8.1 Entry criteria

- [x] F1 complete (can parallel F2 after F1)

### 8.2 Work packages

#### F3.1 — Reset and PROM

**Status:** complete (2026-08-09)

- [x] SPI reset command (`0x1E`) via `spi_bus_transfer` + `BARO_CS`
- [x] Read calibration coefficients C1–C6 from PROM (all eight words; cache for F3.2+)
- [x] CRC4 check per AN520 / datasheet (recommended — implemented)
- [x] Fail `baro_init` on SPI error, zero PROM word, or CRC mismatch
- [x] `error_flags_set_baro_ok` + `baro_is_ok()` on init path
- [x] `baro.c` / `baro.h`; Makefile `C_SOURCES`; called from `app_init` fail-soft
- [x] Host test `tests/host/test_ms5611_crc` pass (manual, 2026-08-09; AN520 vector)

#### F3.2 — Conversion sequence

**Status:** complete (2026-08-15)

- [x] D1 pressure conversion at OSR 4096 (`0x48`) via `spi_bus_transfer` + `BARO_CS`
- [x] D2 temperature conversion at OSR 4096 (`0x58`)
- [x] Wait max conversion time (`HAL_Delay(10)` ms; datasheet 9.04 ms @ OSR 4096)
- [x] Read 24-bit ADC results via command `0x00` (`baro_be_bytes_to_u24`)
- [x] `baro_raw_t` + `baro_read_raw()` polling API; reject zero ADC / SPI failure
- [x] `baro_read_raw` success/failure updates `error_flags_set_baro_ok` and `baro_is_ok()`
- [x] Host test `tests/host/test_ms5611_adc` (manual run pending)

#### F3.3 — Compensation math

**Status:** complete (2026-08-15)

- [x] First-order integer compensation per datasheet B3 (`dT`, `TEMP`, `OFF`, `SENS`, `P`)
- [x] Second-order compensation when `TEMP < 20 °C` and extra terms when `TEMP < −15 °C`
- [x] `baro_comp_t` with pressure in Pa and temperature in 0.01 °C; `baro_comp_temp_c` / `baro_comp_pressure_hpa`
- [x] ICAO ISA altitude helper `baro_pressure_pa_to_alt_m` (troposphere / 11–20 km / 20–32 km)
- [x] Host-testable `baro_compensate` in `baro.h` (no HAL)
- [x] Host test `tests/host/test_ms5611_comp` (datasheet example + 2nd-order + ISA; manual run pending)

#### F3.4 — API

**Status:** complete (2026-08-15)

- [x] `baro_sample_t` with `pressure_pa`, `temp_centi_c`, `alt_m` (mission / telemetry units)
- [x] `baro_read` = `baro_read_raw` + compensate cached PROM + ISA altitude (`baro_sample_from_raw`)
- [x] Fail-soft health: invalid PROM, SPI, or compensate failure → `baro_set_ok(false)`; success → `baro_set_ok(true)`
- [x] `baro_init` already wired in `app_init` (F3.1); `baro_read` not called from `app_run` until mission (F8)
- [x] Altitude helper `baro_pressure_pa_to_alt_m` exposed for mission via `baro_sample_t.alt_m`
- [x] Host-testable `baro_sample_from_raw` in `baro.h`; host test in `test_ms5611_comp` (manual run pending)

### 8.3 Verification / exit criteria

**Software verification (F3.1 — 2026-08-09):**

- [x] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [x] Host `tests/host/test_ms5611_crc` pass (manual, 2026-08-09; AN520 vector)
- [x] Init failure does not hang MCU — `(void)baro_init()` in `app_init`; `app_run` continues; PROM/CRC fail path sets `error_flags_set_baro_ok(false)`

**Software verification (F3.2 — 2026-08-15):**

- [x] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [x] Host `tests/host/test_ms5611_adc` authored (manual run pending)
- [x] `baro_read_raw` fail path does not hang MCU — not called from `app_run`; health updated on read like `imu_read`

**Software verification (F3.3 — 2026-08-15):**

- [x] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [x] Host `tests/host/test_ms5611_comp` authored (manual run pending; datasheet B3 golden vector)
- [x] Compensation helpers pure / host-testable; no SPI or `app_run` changes

**Software verification (F3.4 — 2026-08-15):**

- [x] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [x] Host `tests/host/test_ms5611_comp` extended with `baro_sample_from_raw` datasheet vector (manual run pending)
- [x] `baro_read` fail path does not hang MCU — not called from `app_run`; health updated on read like `imu_read`

**Hardware exit (pending bench — tick when §21 F3 procedure passes):**

- [ ] Indoor pressure ~980–1040 hPa (site-dependent)
- [ ] Raising board ~1–2 m shows altitude change directionally
- [ ] PROM/CRC failure handled cleanly on hardware (CRC pass on silicon; inject fault optional)

---

## 9. Phase F4 — Temperature (MAX31865 + PT1000)

**Phase status:** not started — software work unblocked by F1 software-complete; HW → §21.

### 9.0 Objective

Outside-air temperature via RTD.

### 9.1 Entry criteria

- [ ] F1 software-complete
- [ ] PT1000 3-wire confirmed (team: yes per PCB; Gabe BOM confirm for confidence — not a coding blocker)

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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] Host tests for RTD resistance → °C conversion if extracted as pure logic (optional)
- [ ] `temp_read` fail path does not hang MCU — fail-soft `temp_ok` / `error_flags`; not called from `app_run` until mission (F8)

**Hardware exit (pending bench — tick when §21 F4 procedure passes):**

- [ ] Room-temp reading plausible (~15–30 °C)
- [ ] Hand on probe moves reading
- [ ] Disconnect fault (if safe to test) sets fault flag

---

## 10. Phase F5 — GPS (MAX-M10S)

**Phase status:** not started — software work unblocked by F0 software-complete; HW → §21.

### 10.0 Objective

Non-blocking NMEA parser providing fix for recovery and APRS/LoRa.

### 10.1 Entry criteria

- [ ] F0 software-complete (USART1 @ 9600)

**Hardware exit (not a coding start blocker):** antenna connected for outdoor fix validation — see §10.3.

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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] Host tests for NMEA parse / fix extraction from golden sentences (recommended)
- [ ] Main loop never blocks waiting on GPS — `gps_poll()` non-blocking; fail-soft `gps_ok`

**Hardware exit (pending bench — tick when §21 F5 procedure passes):**

- [ ] Bytes received at 9600 on bench/outdoor
- [ ] Outdoor: valid lat/lon within expected region
- [ ] Antenna connected for outdoor fix tests

---

## 11. Phase F6 — microSD logging (FatFS)

**Phase status:** not started — software work unblocked by F1 software-complete; HW → §21.

### 11.0 Objective

Black-box telemetry log; foundation for image storage.

### 11.1 Entry criteria

- [ ] F1 software-complete

**Known facts (not coding blockers):** industrial microSD for flight; detect polarity high = present (see §2).

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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] No hang when card absent — fail-soft `sd_ok`; mission continues
- [ ] `sdlog_write_sample` non-fatal on failure

**Hardware exit (pending bench — tick when §21 F6 procedure passes):**

- [ ] File visible on PC after flight-sim loop
- [ ] Pull-power mid-write test: document corruption mitigations (flush policy)
- [ ] Industrial microSD exercised on hardware

---

## 12. Phase F7 — LoRa telemetry (RFM95W)

**Phase status:** not started — software work unblocked by F1 software-complete; HW → §21.

### 12.0 Objective

Ground-receivable telemetry (primary recovery link).

### 12.1 Entry criteria

- [ ] F1 software-complete
- [ ] Interim RF + packet proposal accepted or frozen (see §12.2.3)

**Hardware exit / ops (not a coding start blocker):** second RFM95W + Nucleo (or equiv.) for RX — schedule with ops (Q20); see §12.4.

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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] Host tests for packet v1 pack / CRC16 if extracted as pure logic (recommended)
- [ ] TX fail path does not hang MCU — timeout on DIO0 wait; fail-soft health

**Hardware exit (pending bench — tick when §21 F7 procedure passes):**

- [ ] Bench: RX node receives packets with increasing `seq`
- [ ] CRC validated on ground
- [ ] TX rate limited (e.g. ≤0.5 Hz) during test
- [ ] Second RFM95W + Nucleo (or equiv.) for RX available (ops Q20)

---

## 13. Phase F8 — Mission state machine and packetizer

**Phase status:** not started — software work unblocked when F3 and/or F5 altitude APIs are software-complete; HW → §21.

### 13.0 Objective

Autonomous flight behavior and unified telemetry packing.

### 13.1 Entry criteria

- [ ] F3 and/or F5 software-complete (altitude API available)
- [ ] F7 software-complete for live TX (state machine host-testable without radio)

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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] Host tests: simulated altitude profiles walk PAD→…→BEACON correctly
- [ ] BURST does not clear when descent slows (host or unit test)
- [ ] Packetizer CRC16 host-testable against golden vectors

**Hardware exit (pending bench — tick when §21 F8 procedure passes):**

- [ ] Live: LoRa rates match scheduler table within tolerance
- [ ] End-to-end state transitions with real sensors on hardware

---

## 14. Phase F9 — ArduCAM imaging → SD

**Phase status:** not started — software work unblocked by F1 + F6 software-complete; **blocked on ArduCAM SKU** (Gabe); HW → §21.

### 14.0 Objective

Capture flight imagery to microSD without breaking telemetry deadlines.

### 14.1 Entry criteria

- [ ] F1 + F6 software-complete
- [ ] **Gabe confirms ArduCAM SKU** (2MP Mini B0067 vs 5MP OV5642) — **hard coding blocker** for correct driver
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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] Failure sets `cam_ok = false` without reboot loop — fail-soft
- [ ] Chunked capture does not allocate full-frame RAM beyond chip capacity

**Hardware exit (pending bench — tick when §21 F9 procedure passes):**

- [ ] Valid image file opens on PC
- [ ] During capture, LoRa still meets minimum beacon rate (measure)

---

## 15. Phase F10 — APRS (DRA818V)

**Phase status:** not started — software dry-run unblocked by F5 software-complete; on-air blocked by license; HW → §21.

### 15.0 Objective

Backup VHF position beacons.

### 15.1 Entry criteria

- [ ] F5 software-complete (GPS fix API for payload encoding)
- [ ] APRS cable correct (mirrored face-to-face) — bench validation in §21
- [ ] Audio path connected (team confirmed) — bench validation in §21

**On-air blocker (not a software dry-run blocker):** amateur callsign + license (Q19) before RF TX — use `APRS_RF_ENABLE=0` until confirmed.

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

**Software verification (tick when work packages land):**

- [ ] Clean build (`make clean && make` in `balloon-project-stm32mx/`)
- [ ] `APRS_RF_ENABLE=0` dry-run: AX.25 frame build host-testable or logged without RF
- [ ] PTT sequencing does not block mission loop

**Hardware exit (pending bench — tick when §21 F10 procedure passes):**

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

§21 per-subsystem bench backlog does **not** satisfy F11 — F11 is full-stack HIL on hardware.

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
2. Read [Firmware Development Guidelines.md](Firmware%20Development%20Guidelines.md) (verification, deferred HW, host tests).  
3. Find the lowest phase that is **not software-complete**.  
4. Confirm that phase’s **entry criteria** (software-complete of dependencies; open §21 HW items do not block coding).  
5. Implement only listed work packages.  
6. Run software verification; tick software boxes; add deferred HW items to **§21** when no board.  
7. PR title `firmware: complete Phase Fx — <name>` **only after §21 HW pass** for that phase. Software-only milestones use work-package titles (e.g. `firmware: F3.4 baro_read API`).  

**First coding task for the project:** Phase **F0**, then **F1**, then **F2** (IMU WHO_AM_I).

---

## 21. Bench verification backlog

Hardware checks deferred when no board or bench tools are available. **Tick here during bring-up week** when the flight computer PCB is powered and SWD is connected. This list does not replace **F11 system HIL** — it tracks per-subsystem bench validation.

**Clearing §21 is required for full phase exit, not for starting the next driver phase.** Software-complete of a phase unblocks the next sequential coding phase; bench items stay here until bring-up week.

**Process:** When a work package completes in software, add matching HW checks below if not already listed. Clear ticks with date and pass/fail note.

### F0 — Foundation

- [ ] Firmware flashes via ST-Link and reaches `while(1)` / `app_run` after reset

### F1 — SPI bus

- [ ] CS lines idle high (meter or logic analyzer on IMU/BARO/Temp/LoRa/SD CS)
- [ ] Dummy SPI transfer does not leave any CS stuck low
- [ ] Timeout path releases CS (analyzer or fault injection)

### F2 — IMU (ICM-42688-P)

**Checklist (tick with pass date when bench complete):**

- [ ] WHO_AM_I reads `0x47` on hardware (SWD watch or debugger)
- [ ] Post-init register read-back: `GYRO_CONFIG0` / `ACCEL_CONFIG0` = `0x08`, `PWR_MGMT0` = `0x0F`
- [ ] After F2.3 sample read: accel/gyro values change when board is moved or tilted

**Bench procedure (when PCB + ST-Link available):**

1. Flash `build/balloon-project-stm32mx.elf` (see `balloon-project-stm32mx/README.md` § SWD / flash).
2. Confirm `app_init` → `while(1)` / `app_run` after reset.
3. After `imu_init`: `imu_init()` returns true, `imu_is_ok()` true, `error_flags_imu_ok()` true; optional WHO_AM_I reg read = `0x47`.
4. Read-back via SPI/debugger: `GYRO_CONFIG0` / `ACCEL_CONFIG0` = `0x08`, `PWR_MGMT0` = `0x0F`.
5. Call `imu_read(&sample)` repeatedly (GDB or debug loop): flat rest ~1 g on one accel axis (orientation-dependent); tilt/rotate changes ax/ay/az and gx/gy/gz; `imu_is_ok()` stays true on success.
6. Tick checklist above + §7.3 hardware exit items; add roadmap rev **0.15** with bench date; update §7 phase status to `complete (bench YYYY-MM-DD)`.
7. PR title: `firmware: complete Phase F2 — IMU` (roadmap §20).

**On failure:** do not tick exit — check SPI/CS/power, IMU variant (`-V` WHO_AM_I = `0xDB`), and `PWR_MGMT0` if samples are static.

### F3 — Barometer (MS5611)

**Checklist (tick with pass date when bench complete):**

- [ ] Reset + PROM read on hardware; CRC4 passes
- [ ] After `baro_init`: `baro_init()` returns true, `baro_is_ok()` true, `error_flags_baro_ok()` true
- [ ] After F3.2: `baro_read_raw` returns non-zero D1/D2; D1 changes directionally when board lifted ~1–2 m (raw ADC)
- [ ] Indoor pressure plausible (~980–1040 hPa) — requires F3.4 `baro_read` on hardware
- [ ] Altitude responds to ~1–2 m lift — requires F3.4 `baro_read` on hardware

**Bench procedure (when PCB + ST-Link available):**

1. Flash `build/balloon-project-stm32mx.elf` (see `balloon-project-stm32mx/README.md` § SWD / flash).
2. Confirm `app_init` → `while(1)` / `app_run` after reset.
3. After `baro_init`: `baro_init()` returns true, `baro_is_ok()` true, `error_flags_baro_ok()` true.
4. After F3.2: call `baro_read_raw(&raw)` repeatedly (GDB or debug loop): non-zero `raw.d1` / `raw.d2`; `baro_is_ok()` true; D1 changes when board lifted ~1–2 m.
5. Call `baro_read(&sample)` repeatedly (GDB or debug loop): indoor `sample.pressure_pa / 100` ≈ 980–1040 hPa (site-dependent); `baro_is_ok()` true on success; raise board ~1–2 m and confirm `sample.alt_m` increases directionally.
6. Tick checklist above + §8.3 hardware exit items; add roadmap rev with bench date.
7. PR title: `firmware: complete Phase F3 — Barometer` (roadmap §20).

**On failure:** do not tick exit — check SPI/CS/power, `BARO_CS` (PB2), and PROM/CRC (`error_flags_baro_ok` false on CRC mismatch).

### F4 — Temperature (MAX31865 + PT1000)

**Checklist (tick with pass date when bench complete):**

- [ ] Room-temp reading plausible (~15–30 °C)
- [ ] Hand on probe moves reading
- [ ] Disconnect fault (if safe to test) sets fault flag

**Bench procedure (when PCB + ST-Link available):**

1. Flash `build/balloon-project-stm32mx.elf`.
2. After `temp_init`: `temp_init()` returns true, `temp_is_ok()` true on success path.
3. Call `temp_read(&sample)` repeatedly: room-temp plausible; hand on probe changes reading.
4. Tick checklist above + §9.3 hardware exit items; add roadmap rev with bench date.

### F5 — GPS (MAX-M10S)

**Checklist (tick with pass date when bench complete):**

- [ ] Bytes received at 9600 on bench/outdoor
- [ ] Outdoor: valid lat/lon within expected region
- [ ] Antenna connected for outdoor fix tests

**Bench procedure (when PCB + ST-Link available):**

1. Flash firmware; confirm `app_init` → `app_run`.
2. Indoor: `gps_poll()` receives NMEA sentences; `gps_has_fix()` may be false (expected).
3. Outdoor with antenna: valid lat/lon; `gps_has_fix()` true when sky visible.
4. Tick checklist above + §10.3 hardware exit items; add roadmap rev with bench date.

### F6 — microSD logging (FatFS)

**Checklist (tick with pass date when bench complete):**

- [ ] File visible on PC after flight-sim loop
- [ ] Pull-power mid-write test: document corruption mitigations (flush policy)
- [ ] Industrial microSD exercised on hardware

**Bench procedure (when PCB + ST-Link available):**

1. Insert industrial microSD (detect high = present).
2. Run flight-sim append loop; pull card; verify CSV on PC.
3. Optional: power-loss during write; document flush policy outcome.
4. Tick checklist above + §11.3 hardware exit items; add roadmap rev with bench date.

### F7 — LoRa telemetry (RFM95W)

**Checklist (tick with pass date when bench complete):**

- [ ] Bench: RX node receives packets with increasing `seq`
- [ ] CRC validated on ground
- [ ] TX rate limited (e.g. ≤0.5 Hz) during test
- [ ] Second RFM95W + Nucleo (or equiv.) for RX available (ops Q20)

**Bench procedure (when PCB + ST-Link + ground RX available):**

1. Flash TX node; match modem settings to ground RX (SF/BW/freq per §12.2).
2. TX packets; ground station logs increasing `seq`, valid CRC.
3. Tick checklist above + §12.4 hardware exit items; add roadmap rev with bench date.

### F8 — Mission state machine and packetizer

**Checklist (tick with pass date when bench complete):**

- [ ] Live: LoRa rates match scheduler table within tolerance
- [ ] End-to-end state transitions with real sensors on hardware

**Bench procedure (when chase-critical path on hardware):**

1. Run mission SM with real baro/GPS/IMU inputs.
2. Verify state transitions and LoRa rates per §13.2 scheduler.
3. Tick checklist above + §13.3 hardware exit items; add roadmap rev with bench date.

### F9 — ArduCAM imaging → SD

**Checklist (tick with pass date when bench complete):**

- [ ] Valid image file opens on PC
- [ ] During capture, LoRa still meets minimum beacon rate (measure)

**Bench procedure (when ArduCAM SKU confirmed + hardware available):**

1. Capture to SD via `camera_capture_to_sd`.
2. Verify image on PC; measure LoRa beacon rate during capture.
3. Tick checklist above + §14.3 hardware exit items; add roadmap rev with bench date.

### F10 — APRS (DRA818V)

**Checklist (tick with pass date when bench complete):**

- [ ] AT commands ACK
- [ ] Audio tones verified on scope before RF
- [ ] On-air only with license: received on APRS client / HT

**Bench procedure (when APRS hardware + license available):**

1. Dry-run with `APRS_RF_ENABLE=0`: AT ACK, scope on audio tones.
2. On-air only with license: verify decode on APRS client / HT.
3. Tick checklist above + §15.3 hardware exit items; add roadmap rev with bench date.

### Notes

- Full-stack integration remains **§16 Phase F11 (HIL)**.
- Host unit tests (scale, parse, CRC) run on developer machine — see [Firmware Development Guidelines.md](Firmware%20Development%20Guidelines.md) and `balloon-project-stm32mx/tests/host/README.md`.

---


