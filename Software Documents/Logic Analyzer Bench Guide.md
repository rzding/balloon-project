# Logic Analyzer Bench Guide

> **Project:** SSI High-Altitude Balloon Flight Computer  
> **MCU:** STM32F411CEU6 | **Firmware:** `balloon-project-stm32mx/`

---

## Document control

| Field | Value |
|---|---|
| Title | Logic Analyzer Bench Guide |
| Location | `Software Documents/Logic Analyzer Bench Guide.md` |
| Status | Active |
| Audience | Bench testers, firmware engineers |
| Related | [Main Developmental Roadmap.md](Main%20Developmental%20Roadmap.md) §21; [Firmware Development Guidelines.md](Firmware%20Development%20Guidelines.md); [`balloon-project-stm32mx/README.md`](../balloon-project-stm32mx/README.md) |

### Revision history

| Rev | Date | Author | Notes |
|---|---|---|---|
| 0.1 | 2026-08-19 | Firmware | First branch: IMU, baro, temp (SPI), GPS (UART); `make BENCH=1` |

---

## 1. Purpose

This guide explains how to use a **logic analyzer** to verify bus wiring and live traffic for the sensors that are **software-complete today**:

| Device | Bus | In this guide |
|---|---|---|
| ICM-42688-P IMU | SPI1 | Yes |
| MS5611 barometer | SPI1 | Yes |
| MAX31865 + PT1000 | SPI1 | Yes |
| MAX-M10S GPS | USART1 (UART) | Yes |
| RFM95W LoRa | SPI1 | F7.1 init only (VERSION probe at boot) |
| microSD | SPI1 | Not yet (F6) |
| ArduCAM | SPI1 + I2C1 | Not yet (F9) |
| DRA818V APRS | USART2 | Not yet (F10) |

This is **bench bring-up**, not Phase F8 (mission scheduler). It uses a compile-gated loop in `app_run` so SPI keeps toggling after boot.

---

## 2. SPI vs UART (read this first)

**SPI sensors (IMU, baro, temp)** use four wires on a **shared** bus:

| Wire | MCU pin | Probe point | Role |
|---|---|---|---|
| SCLK | PA5 | **J11** | Clock |
| MOSI | PA7 | **J11** | MCU → sensor |
| MISO | PA6 | **J11** | Sensor → MCU |
| CS (per slave) | see below | Test pads | Chip select, **active low** |

All three SPI sensors share SCLK/MOSI/MISO. Only **one** CS line goes low per transfer.

| CS net | MCU pin | Test pad |
|---|---|---|
| `IMU_CS` | PB0 | TP20 |
| `BARO_CS` | PB2 | TP22 |
| `Temp_CS` | PA8 | TP21 |
| `LoRa_CS` | PB1 | TP23 (one VERSION read at boot; idle after init) |
| `microSD_CS` | PB3 | TP24 (idle — no driver) |
| `Cam_CS` | PA4 | (idle — no driver) |

**GPS uses UART, not SPI.** There is no CS, MOSI, MISO, or SCLK on the GPS link. Clip the UART line and use an **async serial / UART** decoder.

| Signal | MCU pin | Net name | Role |
|---|---|---|---|
| MCU RX | PA10 | `GPS_TX` (module TXD) | GPS → MCU (NMEA stream) |
| MCU TX | PA9 | `GPS_RX` (module RXD) | MCU → GPS (idle in bench firmware) |

Settings: **9600 baud, 8N1**.

---

## 3. Firmware build for analyzer captures

**Default build** (`make` with no options): SPI runs only during `app_init` (short burst at reset), then stays quiet. GPS UART still streams.

**Bench build** (ongoing SPI sample traffic ~1 Hz):

```bash
cd balloon-project-stm32mx
make clean && make BENCH=1
```

Flash `build/balloon-project-stm32mx.elf` or `.bin` per [`balloon-project-stm32mx/README.md`](../balloon-project-stm32mx/README.md) § SWD / flash.

**Do not ship `BENCH=1` as flight firmware.** Phase F8 mission loop replaces this flag for operational builds.

What `BENCH=1` does in `app_run` (every ~1 s, fail-soft):

1. `imu_read`
2. `baro_read` (~20 ms internal conversion waits)
3. `temp_read` (~60 ms internal conversion wait)
4. `gps_poll` every superloop iteration (always, even without `BENCH=1`)

---

## 4. Analyzer setup (all captures)

1. Connect analyzer **GND** to board GND first (TP3, TP5, TP8, or TP13).
2. Logic levels are **3.3 V**. Do not clip 5 V rails with a 3.3 V-only pod.
3. Sample rate: **≥ 8 MS/s** for SPI (~0.78 MHz clock). GPS at 9600 works at lower rates; keep 8 MS/s if SPI and UART share one capture.
4. Typical USB analyzers have **8 channels** — probe **one SPI slave at a time** (shared SCLK/MOSI/MISO + that slave’s CS).
5. Start capture, then **reset the MCU** if you want init + periodic traffic in one file.
6. Triggers: SPI on **CS falling**; GPS on UART **start bit** (line falling).

### SPI decoder settings (IMU, baro, temp)

| Setting | Value |
|---|---|
| Mode | **0** (CPOL=0, CPHA=0) |
| Bit order | MSB first |
| Word size | 8 bits |
| CS / Enable | Active **low**, assign to the CS channel you clipped |

### UART decoder settings (GPS)

| Setting | Value |
|---|---|
| Protocol | Async serial / UART (not SPI) |
| Baud | 9600 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |

---

## 5. Capture procedures

### Capture A — SPI + IMU (start here)

**Clip:** GND, J11 SCLK, J11 MOSI, J11 MISO, **TP20 IMU_CS**.

**Expect:**

- At boot: several CS frames (WHO_AM_I, config). WHO_AM_I read on MISO includes **`0x47`**.
- With `BENCH=1`: about **1 Hz** sample bursts. First MOSI byte of a sample read is **`0x9F`** (`0x1F | 0x80`).
- IMU_CS falls, SCLK runs, MOSI/MISO active, IMU_CS rises. Other CS stay high.

**Then** move only the CS clip to **TP22** (baro), then **TP21** (temp). Keep SCLK/MOSI/MISO on J11; change decoder Enable to the new CS channel.

### Capture B — idle CS check (F1)

**Clip:** J11 SCLK/MOSI/MISO plus multiple CS pads (TP20/21/22; optional TP23 LoRa, TP24 SD).

**Expect:**

- During an IMU transfer, **only** IMU_CS is low.
- Without `BENCH=1`, after boot all CS idle **high** and SCLK quiet except the init burst.
- CS never stuck low after a transfer ends.

### Capture C — GPS UART

**Clip:** GND, **PA10** (MCU RX / `GPS_TX`). Optional: PA9 (MCU TX — should be idle).

**Expect:**

- Idle-high UART on PA10, bit time ~104 µs.
- Repeating ASCII NMEA (`$GNGGA`, `$GNRMC`, …).
- **No** CS, **no** SCLK.
- Indoor fix not required for this capture.

---

## 6. Per-device pass criteria

### IMU (ICM-42688-P)

- CS-framed SPI on IMU_CS with clock activity.
- WHO_AM_I response **`0x47`** at init.
- Periodic `imu_read` bursts with `BENCH=1`.

### Barometer (MS5611)

- CS-framed SPI on BARO_CS.
- MOSI shows command bytes (`0x1E` reset at init; `0x48`/`0x58` conversions; `0x00` ADC read).
- Quiet gaps with CS high during ~10 ms conversions — **normal**.

### Temperature (MAX31865)

- CS-framed SPI on Temp_CS.
- CONFIG read-back **`0x90`** at init.
- With `BENCH=1`: long quiet gap (~60 ms conversion) then RTD data burst.

### GPS (MAX-M10S)

- Continuous UART on PA10 at 9600 8N1.
- NMEA sentences visible in decoder. Fix optional indoors.

---

## 7. Fail signatures (quick debug)

| Symptom | Likely cause |
|---|---|
| SCLK runs but MISO stuck | Wiring, wrong CS, slave not powered, or probe on wrong net |
| CS low and stuck | Short, or `spi_bus_transfer` fault — check F1 |
| Activity on wrong CS | Clip error or shorted CS lines |
| GPS line idle forever | Wrong pin (TX vs RX), baud mismatch, module unpowered |
| SPI quiet after boot only | Expected without `BENCH=1` — rebuild with `make BENCH=1` |

---

## 8. Explicitly not this branch

- **LoRa:** `lora_init` pulses `LoRa_RESET` then reads VERSION on `LoRa_CS` (TP23) once at boot; CS idle high afterward. No `BENCH=1` LoRa loop yet; no TX until F7.3.
- **microSD / ArduCAM:** CS idle high; no driver traffic.
- **APRS USART2 / PTT / PWM:** Initialized idle; no App traffic.
- **I2C1 (ArduCAM):** Bus idle after init.

When F6 (SD) and F7 (LoRa) are software-complete, extend the same `BENCH=1` flag and add capture groups here.

---

## 9. Clearing roadmap §21

Logic analyzer captures support **hardware exit** checklists in roadmap §21 F1–F5. After captures pass, also confirm driver health via SWD/GDB where the §21 procedure requires (`imu_is_ok`, plausible sensor values, etc.). Tick §21 items with date and pass/fail — do not tick from analyzer alone if functional checks fail.

See [Main Developmental Roadmap.md §21](Main%20Developmental%20Roadmap.md#21-bench-verification-backlog).
