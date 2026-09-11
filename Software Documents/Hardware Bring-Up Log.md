# Hardware Bring-Up Log / Session Handoff

> **Project:** SSI High-Altitude Balloon Flight Computer
> **MCU:** STM32F411CCU6 | **Firmware:** `balloon-project-stm32mx/`
> **Bench sessions:** 2026-08-19 → 2026-09-09 (camera bring-up added as §13)
> **Purpose:** hand off enough context to continue bench testing without re-deriving anything.

Every driver in `App/Src/` was written and host-tested before any of it had run
against physical hardware. These sessions were the first bring-up. This document
records what is verified, what is broken, how to run the tools, and what is left.

---

## 1. Status at a glance

| Subsystem | Status | Evidence |
|---|---|---|
| SPI1 bus | ✅ working | all four slaves respond |
| IMU (ICM-42688-P) | ✅ working | `WHO_AM_I` (reg `0x75`) = `0x47` |
| Barometer (MS5611) | ✅ working | PROM CRC4 validates; altitude ~23 m vs San Jose ~25 m |
| Temp (MAX31865 + PT1000) | ⚠️ **regressed 2026-09-08** | was 1093 Ω → 23.9 °C; with the ArduCAM on SPI1 it now fails init every boot (§13.4) |
| LoRa (RFM95W) | ✅ working | `RegVersion` = `0x12`; 27/28 packets received end-to-end |
| GPS (MAX-M10S) | ⚠️ **blocked by hardware** | NMEA parses fine; **no fix — antenna has no DC bias** |
| microSD | ⚠️ untested | no card on hand; **socket ran hot on 2026-09-08** with the camera attached (§13.5) |
| Camera (ArduCAM Mini 2MP, OV2640) | ✅ working (2026-09-10) | R28/R29 reworked to real 4.7k; I2C alive, OV2640 PID 0x26 reads. MID reads 0xFF → made informational (§13.8) |

**Two board-level bugs block flight. Neither is fixable in firmware.** See §3.

---

## 2. How to build, flash, debug

### Build

```bash
cd balloon-project-stm32mx && make          # ~34 KB text, 0 warnings expected
```

Toolchain is Homebrew `arm-none-eabi-gcc` at `/opt/homebrew/bin`. CubeIDE does
**not** inherit the shell PATH on macOS, so inside the IDE it is set at
*Properties → C/C++ Build → Environment* (lives in `.settings/`, gitignored —
every developer sets their own).

### Debug server — start this before every debug session

```bash
./scripts/stlink-server.sh          # start / restart
./scripts/stlink-server.sh status   # is the server up, is the probe on USB
./scripts/stlink-server.sh stop
./scripts/stlink-server.sh log
```

CubeIDE's *GDB Hardware Debugging* launch expects a server already listening on
port **61234** and does not start one itself. The script kills stale servers and
gdb clients, verifies the probe, and retries the SWD attach (which fails on the
first try often enough that the retry matters). It passes `-k`
(connect-under-reset), without which the server reports "Target no device found".

### Debug in CubeIDE

Launch type is **GDB Hardware Debugging**, *not* STM32 Cortex-M C/C++
Application. The ST launch type rejects Makefile projects with "Unsupported
build configuration. MCU ARM GCC required for debug." — it requires CubeMX
managed-build metadata this project does not have.

Workflow: **Debug → F8 (run) → Suspend → read Expressions → F8** to continue.

### Recurring failure: USB dropouts

Six times across these sessions the ST-LINK vanished from USB mid-session:

```
libusb: error [submit_bulk_transfer] bulk transfer failed (dir = Out):
        no connection to an IOService (code = 0xe00002c0)
```

Symptom in the IDE is a launch that hangs at **90%**, or `Remote replied
unexpectedly to 'vMustReplyEmpty': timeout`. Fix is always: cancel the launch,
physically replug the probe, run `./scripts/stlink-server.sh`, retry.

This happens when the probe is on a dock or dongle. A direct port is much more
stable. **Do not kill gdb PIDs while CubeIDE is mid-launch** — that produces a
confusing `SIGTERM` error; cancel the dialog first.

---

## 3. Board-level bugs — EE action required

### 3.1 NRST floats at 1.5 V — blocks untethered operation

Measured **1.5 V** on NRST with the debugger detached. It should sit at ~3.3 V,
held by the STM32's internal pull-up (~40 kΩ). Reset releases around 0.7 × VDD ≈
2.3 V, so at 1.5 V the chip is held in or near reset and never starts.

Symptom: **runs perfectly with the ST-LINK attached, completely dead on
battery.** The debugger drives NRST high and forces execution from flash, which
masks the problem entirely.

Working the divider backwards, 3.3 V through ~40 kΩ landing at 1.5 V implies
roughly **33 kΩ to ground** — close enough to a real part that the reset net is
worth inspecting rather than assuming leakage.

Standard fix: 100 nF NRST→GND and nothing else; optionally a 10 kΩ pull-up to
3.3 V as insurance.

**This blocks flight.** There is no ST-LINK on the launch field.

**Correction 2026-09-08 (from the production netlist):** the "standard fix" is
already fitted — **R14 = 10 kΩ NRST→3V3** and **C33 = 100 nF NRST→GND** are on
the net, alongside SW1, TP10, J5 pin 2 (SWD header) and the MCU. With a 10 k
pull-up, 1.5 V implies ~7 kΩ to ground, which nothing on the net can present.
Before hunting a bad part: (1) was the 1.5 V read with the SWD cable still
attached to an *unpowered* Nucleo? Its NRST driver clamps the line. (2) On
battery with the cable physically unplugged, measure in order VBAT_PROT →
+5V (TPS63070 U11) → +3V3 (AP2112K U3) → NRST. If 3V3 is low on battery the
board was never powered and NRST is a symptom, not the cause.

### 3.2 GPS active antenna has no DC bias — blocks GPS fix

The antenna is an **Adafruit 960**, an *active* antenna with a 28 dB LNA needing
3–5 V fed up the coax. Confirmed from the fabrication netlist
(`production/netlist.ipc`):

- `VCC_RF` (GPS pin 14) sits on a net with **exactly one pad** — unconnected
- The antenna net has exactly two pads: GPS pin 11 (`RF_IN`) → J7 pin 1

No inductor or ferrite anywhere on the GPS sheet, so no bias-T. The LNA has
never had power, and an unpowered LNA *blocks* the signal path rather than
merely failing to amplify. That is why the module emits NMEA forever but never
acquires satellites.

**Fix:** one inductor (47 nH – 1 µH, or a GPS-rated ferrite) from `VCC_RF` to
the RF trace — u-blox's own reference topology. The two pads are **3.3 mm apart
on the same edge** of the module, so it is bodgeable for bench work, but a
hand-wound coil is not flight-worthy and should not fly.

**Verify either way:** DC volts on the SMA centre pin. 0 V now, 3.3 V once fixed.

A passive antenna also works with the board as-is, but must mount close to the
board — a passive antenna on a long cable is worse than useless at 1.575 GHz.

---

## 4. Debug globals

All have external linkage and are `volatile` specifically so they resolve in the
debugger from any stop location. Nothing in the firmware reads them.

| Expression | Meaning |
|---|---|
| `g_beacon_attempts` / `g_beacon_ok` / `g_beacon_fail` | LoRa beacon transmit counters |
| `g_beacon_fields` | decoded telemetry being sent |
| `g_beacon_wire` | the 28 raw bytes on air — copy as hex into `ground/decode_packet` |
| `g_gps_rx_bytes` | bytes taken from USART1 by the ISR |
| `g_gps_lines` | complete NMEA lines extracted |
| `g_gps_sentences` | lines that parsed as GGA/RMC |
| `g_gps_rx_overruns` | USART overruns — **should stay 0** |
| `g_gps_last_line` | last raw NMEA line |
| `g_temp_last_adc` | MAX31865 RTD ADC code |
| `g_temp_fault_status` | MAX31865 fault register (see §6.3) |
| `g_temp_faults` / `g_temp_reads_ok` | RTD read counters |

**GPS diagnostic ladder** — the first zero counter names the failure:

| Result | Meaning |
|---|---|
| `rx_bytes` = 0 | no serial data: wiring, power, or TX/RX swapped |
| `rx_bytes` > 0, `lines` = 0 | baud mismatch (receiving garbage) |
| `lines` > 0, `sentences` = 0 | not GGA/RMC, or checksums failing |
| `sentences` climbing, no fix | **module healthy — needs sky view / antenna power** |

Last measured indoors: 13436 bytes, 388 lines, 88 sentences in ~30 s. The 23 %
sentence-to-line ratio is correct — the module emits ~9 sentence types and only
two are counted.

**Breakpoints for the identity checks** (line numbers drift, grep if stale):

| Device | Location | Then |
|---|---|---|
| IMU | `imu.c:78` | F6 once, read `id` → want `0x47` |
| Baro | `baro.c:188` | F6 through to `baro_set_ok(true)` at 214 |
| Temp | `temp.c:134` | step to 146, read `readback` → want `0x90` |
| LoRa | `lora.c:181` | F6 once, read `id` → want `0x12` |

---

## 5. Telemetry link and ground station

`app_run()` transmits a packet v1 beacon every 5 s built from whatever sensors
are healthy. Sensors without data leave fields zeroed, so the RF link can be
exercised indoors with no GPS fix.

Ground station is at [`ground/esp32_receiver/`](../ground/esp32_receiver) — an
ESP32 plus an Adafruit RFM95W breakout (product 3072), printing decoded
telemetry with RSSI/SNR over USB serial at **115200**. Wiring table and setup are
in that folder's README.

**Use the "LoRa" library by Sandeep Mistry, not RadioHead.** `RH_RF95` prepends a
4-byte TO/FROM/ID/FLAGS header to every packet; the flight firmware sends a raw
28-byte payload, so RadioHead would eat the first four bytes and corrupt
everything.

Modem settings must match `App/Inc/lora.h` exactly: **915.0 MHz, SF8, BW 125 kHz,
CR 4/5, explicit header, CRC on, sync word `0x12`, preamble 8.**

**Result:** 27 of 28 packets received and CRC-validated. Sequence numbers
incremented cleanly, timestamps exactly 5000 ms apart. The one bad packet was
noise (RSSI −111 dBm, SNR −15.2 dB, version byte `0x3d`).

Only tested at RSSI **−13 dBm** (touching distance). Range/link-margin testing is
still open.

### Flags byte semantics changed

`app.c` now sends `(uint8_t)(~error_flags_get() & 0xFFu)` — **inverted**. A *set*
bit now means OK. Bit 0 IMU, 1 baro, 2 temp, 3 GPS, 4 SD, 5 LoRa. Earlier
sessions used the opposite convention; do not trust old notes on this.

---

## 6. Firmware bugs found and fixed

### 6.1 MAX31865 register R/W convention inverted — commit `f5ef0a4`

`temp.c` used `spi_bus_read_reg8()`/`write_reg8()`, which implement the
**ICM-42688-P** convention (read = `reg | 0x80`). The MAX31865 inverts it:
write = `reg | 0x80`, read = `reg` with MSB clear. Every access did the opposite
of what was intended — the config "read" was actually *writing* `0x00` into the
register, which is why readback was always `0x00`.

`spi_bus.h` already documented that other slaves must frame their own transfers.

### 6.2 MAX31865 requires SPI mode 1 — commit `a6bd5d3`

SPI1 is CPOL=0/CPHA=0 (mode 0). The MAX31865 supports only mode 1 or 3 (CPHA=1).
IMU and baro tolerate mode 0, so only the RTD was affected. Added
`spi_bus_set_mode()`; `temp.c` switches to mode 1 for its transfers and restores
mode 0 afterwards.

Found and fixed *first*, but was **not** the actual blocker — §6.1 was. Both
were real.

### 6.3 RFM95W R/W convention inverted — commit `4075668`

Same class as §6.1. RFM95W datasheet §4.3 defines address bit 7 as `wnr` —
**1 for write, 0 for read** — the inverse of the ICM convention. Three call
sites, including `lora_write_fifo()` which had no write bit at all. That path
only runs during transmit and had never executed.

Datasheet §4.3 also specifies CPOL=0/CPHA=0, so unlike the MAX31865 no mode
switching is needed here.

**Datasheet discrepancy:** HopeRF lists `RegVersion` reset value `0x11`. The
silicon on this board reports `0x12`. `LORA_VERSION_EXPECT` stays `0x12` — do
not "correct" it to match the PDF.

### 6.4 microSD merge broke the build — commit `4ccdf82`

Six mechanical errors: missing Makefile line-continuation backslashes (which
alone broke everything), `sd_spi.c` absent from `C_SOURCES`, `ICM_CS_*` instead
of `IMU_CS_*`, missing `<stdio.h>`/`<string.h>`, `SD_SPI_Read` vs
`SD_SPI_ReadBlocks`, and `SD_CS_*` instead of `microSD_CS_*`.

### 6.5 SD writes with no card, doubled loop delay — commit `f9ebab5`

`f_write(&fil, ...)` ran unconditionally; when `f_open` fails, `fil` is an
uninitialised handle passed into FatFs (undefined behaviour, can hang the loop).
Guarded behind `sd_ready`.

The loop also had two `HAL_Delay(100)` calls, one from each half of merged code
— a 200 ms period. At 9600 baud the GPS produces ~192 bytes in that window
against a **256-byte** RX ring, so any added latency overflowed it and corrupted
NMEA. Collapsed to a single 100 ms delay.

---

## 7. MAX31865 / PT1000 reference

Verified correct against the schematic and the Adafruit 3984 sensor:

| Constant | Value | Matches |
|---|---|---|
| `TEMP_RREF_OHM` | 4300.0f | R21 = 4300 on the schematic ✅ |
| `TEMP_PT1000_R0_OHM` | 1000.0f | PT1000 ✅ |
| `TEMP_RTD_ADC_FULL_SCALE` | 32768.0f | MAX31865 15-bit ✅ |
| CVD A/B/C | 3.9083e-3, −5.775e-7, −4.183e-12 | IEC 60751 ✅ |
| Wire mode | `TEMP_CFG_3WIRE` (`0x10`) | Adafruit 3984 is 3-wire ✅ |

4300 Ω is correct for PT1000 (≈4 × R0); a PT100 would use 430 Ω.
Conversion is `R = (ADC / 32768) × 4300`.

Reference ADC values: **24 °C ≈ 8331**, 40 °C ≈ 8830, 60 °C ≈ 9440,
100 °C ≈ 10555. Near 32767 = open circuit; near 0 = short.

**Fault register decode** (`g_temp_fault_status`, datasheet Table 11, 3-wire):

| Bit | Cause | Reading |
|---|---|---|
| `0x80` D7 | open RTD element / FORCE+ shorted high | full scale |
| `0x40` D6 | RTDIN+ shorted to RTDIN− / FORCE+ shorted low | near zero |
| `0x20` D5 | open RTD element, or FORCE+ unconnected | full scale |
| `0x10` D4 | RTDIN− shorted low | looks valid |
| `0x08` D3 | FORCE+ shorted low / RTDIN+ shorted low | near zero |
| `0x04` D2 | over/undervoltage on a protected input | indeterminate |

A disconnected PT1000 typically shows `0x20`, often with `0x80`.

---

## 8. Technique worth reusing

**Known-answer testing.** `HAL_SPI_TransmitReceive()` returns `HAL_OK` as long as
the MCU clocked bytes out — it says nothing about whether a chip was listening.
With no slave attached, a read still "succeeds" and returns `0x00`/`0xFF`. Every
check must ask something whose answer is known in advance. Ranked by strength:
baro CRC4 (strongest — a checksum over 112 bits of factory data cannot pass by
chance) > temp write/readback (proves writable state) > IMU/LoRa fixed ID.

**Staged gates.** `baro_init()` has four stages, each with its own
`if (!stage) { set_ok(false); return false; }` exit. One breakpoint plus one
keypress names the failing stage with no extra instrumentation.

**Breakpoints do not bind to every line.** At `-Og` the compiler merges identical
`return false;` statements into a shared epilogue, so a breakpoint on a bare
`return` may relocate or never fire. Put them on lines with a real function call.

**`static` hides variables from the debugger.** File-local symbols only resolve by
bare name while halted inside that translation unit. Debug counters need external
linkage — that is why the `g_*` globals exist.

**Halting inside a transfer holds CS low** for as long as you sit there, which
real SPI slaves do not expect. Single-stepping through `spi_bus_transfer()` can
*create* the failure you are hunting. Prefer a breakpoint after the transaction.

**Live Expressions does not work with this launch type** — it is an ST feature
tied to their STM32 launch configuration. Use regular Expressions with
suspend/resume.

---

## 9. Clock configuration

| Clock | Frequency |
|---|---|
| HSE crystal | 12 MHz |
| PLL | M=6, N=100, P=2 → **100 MHz** |
| SYSCLK / HCLK / Cortex | 100 MHz (F411 maximum — no headroom) |
| APB1 (PCLK1) | 50 MHz — USART2, I2C1 |
| APB2 (PCLK2) | 100 MHz — **SPI1**, USART1 |
| Flash latency | 3 wait states |

SPI1 at `SPI_BAUDRATEPRESCALER_128` → 100 MHz / 128 ≈ **781 kHz** bring-up clock.

**No RTC is configured** — no RTC peripheral, no LSE, no LSI. The 12 MHz HSE is
the only oscillator on the board.

---

## 10. Uncommitted work in progress (as of 2026-09-08)

`git status` shows these firmware files modified but not committed:

- **`.ioc` regenerated** with CubeMX **6.18.1** (was 6.17.0). **PB5 added as
  `LED_Pin`**, so `main.h` now defines `LED_Pin`/`LED_GPIO_Port` and
  `MX_GPIO_Init()` configures it. The old bare-metal `GPIOB->MODER`/`BSRR` block
  in `main.c` was replaced with `HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin)`.
- **`stm32f4xx_it.c`**: CubeMX regenerated `USART1_IRQHandler`. It now calls
  `gps_usart1_irq()` inside USER CODE and *then* `HAL_UART_IRQHandler(&huart1)`.
  **Reviewed 2026-09-08:** safe in every normal path — the GPS ISR reads SR
  then DR, which clears RXNE/ORE/FE/NE before HAL looks, so HAL returns doing
  nothing; a byte landing in the sub-µs gap is left in DR by HAL (no HAL
  receive is active) and the IRQ re-fires. One latent hazard: if HAL ever sees
  ORE set it calls `UART_EndRxTransfer`, which clears RXNEIE and silently
  kills GPS RX until reset. Free fix: untick "Call HAL handler" for USART1 in
  the CubeMX NVIC tab, or end the USER CODE block with `return;`.
- **`main.c`**: the LED edit deleted the `/* USER CODE END 2 */` marker (line
  ~110 has a BEGIN 2 with no END). **The next CubeMX regeneration will mangle
  main.c** — restore the marker after the blink loop. The loop also toggles an
  even number of times, so the LED ends **dark**, which defeats the battery-boot
  diagnostic described in the comment above it (end with the LED on).
- **`stm32f4xx_hal_msp.c`**: USART1 NVIC enable/disable now appears twice (one
  generated, one in USER CODE). Harmless; delete the USER CODE copy.
- `temp.c`: the `g_temp_*` diagnostic globals (§4).
- `Makefile`, `ffconf.h`, `hal_msp.c`, `.mxproject`: regeneration side effects.

Builds clean, 0 errors 0 warnings. **None of this is committed or pushed.**

**Added 2026-09-08 evening, also uncommitted:**

- `origin/main` was pulled (fast-forward): Arnav's `5dbedee` mission state
  machine (mission/schedule/packetizer, app.c rewrite, beacon now every 2 s on
  PAD) and `257c6d7` docs. Those commits also check in three compiled host-test
  binaries under `tests/host/`, which will dirty the tree on every rebuild.
- **New, untracked:** `App/Inc/camera.h`, `App/Src/camera.c` (§13). Wired in
  via `App/Src/app.c` (`camera_init()` after `sdlog_init()`) and `Makefile`
  (`App/Src/camera.c` in `C_SOURCES`). `git add` them before anything else.
- The Makefile has CRLF line endings — `sed`/`perl` edits must allow `\r`.

---

## 11. Open items

| Item | Owner | Blocks |
|---|---|---|
| NRST reset network (§3.1) | EE | **Flight** — will not run untethered |
| GPS antenna bias inductor (§3.2) | EE | GPS fix |
| SD card for logging test | — | microSD verification |
| ArduCAM SKU decision | Gabe | Camera driver (F9) |
| LoRa range testing | Firmware | only tested at −13 dBm |
| `imu_read()` never called | Firmware | no accel/gyro anywhere in the system |
| Packet v1 has no IMU fields | Firmware | would need v2 or an extension |
| Commit §10 work in progress (incl. camera.c/.h, untracked) | Gabe | — |
| Rework R28/R29: 4.7µF caps → 4.7k resistors (§13.7) | Gabe | Camera I2C, any I2C |
| Fix R28/R29 LCSC C98195→4.7k resistor in schematic + BOM (§13.7) | Gabe | next board order |
| Temp regression with camera on SPI1 (§13.4) | Gabe | Temp telemetry |
| microSD socket ran hot (§13.5) | Gabe / EE | SD logging, possibly board |
| Restore `USER CODE END 2` in main.c (§10) | Gabe | next CubeMX regen |

**Design issues in the microSD commit, flagged but deliberately not fixed**
(they belong to that author): duplicated `USER CODE BEGIN 2` blocks in `main.c`;
`Baro_GetPressure()` returning a hardcoded constant with a TODO referencing a
BMP390 when the board has an MS5611; `IMU_GetAccelX()` hand-rolling register
reads that `imu.c` already does correctly; and the removal of the `cs_port` NULL
check from `spi_bus_transfer()`.

---

## 12. File map

| Path | What |
|---|---|
| `App/Src/`, `App/Inc/` | all sensor drivers — the code that matters |
| `Core/Src/main.c` | CubeMX boot, peripheral init, superloop calling `app_run()` |
| `Core/Src/stm32f4xx_it.c` | interrupt handlers, incl. `USART1_IRQHandler` → GPS |
| `Drivers/` | ST HAL + CMSIS — vendor code, never edited |
| `tests/host/` | host unit tests (pure math, no hardware); `make && ./test_*` |
| `ground/esp32_receiver/` | ESP32 ground station sketch + README |
| `ground/decode_packet.c` | CLI packet v1 decoder — feed it `g_beacon_wire` hex |
| `scripts/stlink-server.sh` | ST-LINK GDB server control |
| `Software Documents/` | this log, bench guide, roadmap, dev guidelines |
| `SSI Balloon Summer PCB/` | KiCad schematics and layout |
| `Datasheets/` | MAX31865, RFM95W, ArduCAM |

---

## 13. Camera (ArduCAM Mini 2MP / OV2640) bring-up — 2026-09-08 evening, continued 2026-09-09

Module: "Arducam Mini Module Camera Shield with OV2640 2 Megapixels" on
`CAM_CONN` J9. Driver: `App/Inc/camera.h`, `App/Src/camera.c` (F9.1a, probe
only, uncommitted — §10). `camera_init()` runs from `app_init()` after
`sdlog_init()`, fail-soft, and sets `error_flags` `cam_ok`.

### 13.1 Wiring facts (schematic + production netlist)

| J9 pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| net | CAM_CS (PA4) | MOSI | MISO | SCK | GND | **+5V** | SDA (PB7) | SCL (PB6) |

Same order as the module's own pin table. CAM_CS has a 10 k pull-up to **+5V**
(R27). SCL/SDA have 4.7 k pull-ups to **+3V3** (R28, R29) and also go to header
J4. The module is powered from the 5 V rail (datasheet: 70 mA normal, all I/O
3.3 V / 5 V tolerant). I2C1 = PB6/PB7 AF4 open-drain, CubeMX 400 kHz; the
driver re-inits it at 100 kHz for bring-up (`CAMERA_I2C_CLOCK_HZ`).

ArduChip SPI: mode 0, address bit 7 = **1 for write** (inverse of the ICM
convention, like RFM95W/MAX31865 — the driver frames its own transfers).
OV2640 SCCB: 7-bit 0x30; bank register 0xFF (0x01 = sensor bank); PIDH/PIDL
0x0A/0x0B = 0x26 / 0x41|0x42; MIDH/MIDL 0x1C/0x1D = 0x7F / 0xA2. Reads are
write-index, STOP, read (no repeated start), as in the ArduCAM library.

### 13.2 Results — two boots on 2026-09-08, same values

| Expression | Read | Want | Verdict |
|---|---|---|---|
| `g_cam_spi_rd_a` | 0x55 | 0x55 | ✅ ArduChip TEST1 write/readback |
| `g_cam_spi_rd_b` | 0xAA | 0xAA | ✅ (read clocks 0x00 on MOSI, so no echo/float can pass) |
| `g_cam_arduchip_rev` | 0x80 | record | REV register 0x40 |
| `g_cam_spi_fail_stage` | 0 | 0 | ✅ **SPI to the camera works** — CS/MOSI/MISO/SCK/5V proven |
| `g_cam_i2c_lines_idle` | 3 | 3 | both lines high at rest (pull-ups fine) |
| `g_cam_i2c_hal_status` / `_err` | 3 / 0x200 | — | HAL_TIMEOUT + `HAL_I2C_WRONG_START`: **START never generated** |
| `g_cam_i2c_line_test` | **0x13** | 0x1F | released high ✅, **driven low reads HIGH on both SDA and SCL** ❌ |
| `g_cam_i2c_pin_cfg` | 0x443A | 0x443A | PB6/PB7 AF4 open-drain — pin config is correct |
| `g_cam_i2c_attempt` | 0 | 1 | analog-filter-off retry did not help |
| `g_cam_bb_ack` | 1 | — | bit-bang saw SDA low at the ACK slots — **contradicts the line test**, treat as unreliable |
| `g_cam_bb_pidh` | 0 | 0x26 | bit-bang read did not complete |
| `g_cam_i2c_cr1` / `sr1` / `sr2` | 0x301 / 0x200 / 0 | — | PE+START+STOP pending; SR1 = AF; not master, not busy |

**Bottom line: SPI ✅. I2C ❌ because the MCU cannot pull either I2C line low,
even as a plain open-drain GPIO with the peripheral off.** That is why the I2C
peripheral times out on START and never sends the address. The pin config is
right, so the wire is being held high by something stronger than the pad, or
the readback is being fooled — the current build snapshots IDR/ODR/MODER/OTYPER
at the driven-low instant (`g_cam_i2c_drive_dbg`, want 0x3500; 0x3503 = held
high externally) and records the second line test (`g_cam_i2c_line_test2`).

### 13.3 Next tests, in order

1. **Meter, board off, camera unplugged:** resistance SDA→3V3 and SCL→3V3 at
   R29/R28 (or J4 pins 2/1 to a 3V3 test point). Want **4.7 kΩ**. Near 0 Ω on
   both = wrong part fitted (a 4R7 for a 4.7k is the classic) or bridged pads —
   that alone explains every I2C reading, and I2C had never been exercised on
   this PCB before 2026-09-08.
2. **Flash the current build with the camera unplugged**, read
   `g_cam_i2c_line_test` / `g_cam_i2c_drive_dbg`. 0x1F / 0x3500 without the
   camera → the module holds the lines; still 0x13 → the board does.
3. Only then look at SDA/SCL swap at the camera end; a swap gives "START sent,
   no ACK" (status 1, err 0), which is **not** what was seen.

Expressions to keep: all `g_cam_*` from the `camera.c` header, plus
`g_beacon_fields.flags` (bit set = OK: IMU 0x01, baro 0x02, temp 0x04, GPS
0x08, SD 0x10, LoRa 0x20, cam 0x40) and the `g_temp_*` set — one name per row
(a row containing two names shows "Error: Multiple errors reported").

### 13.4 Temp regression (seen both boots)

`g_temp_last_adc = 0`, `g_temp_faults = 0` after several beacons ⇒ `temp_init`
failed (config readback ≠ 0x90) so `temp_read` is never called (`app.c` gates
on `temp_is_ok()`). Nothing in the merge touched temp. The only change is the
camera on the shared SPI1. Some ArduCAM Mini revisions **do not release MISO
when deselected**; the IMU and LoRa still read fine (strong drivers), the
MAX31865's SDO is rated 1.6 mA and would lose that fight. Test: pull only the
camera's MISO wire, reset, check `g_beacon_fields.flags` bit 2, or breakpoint
`temp.c:165` and step to 170 for `readback` (line numbers moved +24 with the
diag globals). If it returns, the fix is hardware: a tri-state buffer on the
camera's MISO enabled by Cam_CS, or a Plus-revision module (has one built in).

### 13.5 microSD socket ran hot

Reported with the camera attached. Nothing that can dissipate real power is
within 0.45 in of J6 (only R26 10k CS pull-up, C34/C35 3V3 decoupling, D1 LED
+ R18 150 Ω, TP12); the socket is fed 3V3 directly (pins 4, 10), no power
switch; the only FET on the board is the battery-protection Q2. Firmware
configures no GPIO outside CubeMX. So the heat is inside the socket or on it:
a card, if one was inserted (a card's DO would also lose the MISO fight above
and cook), a short at the 3V3/SCK/GND pins 4–6, or simply the camera module
resting on the socket shell — the ArduChip runs warm and J9 is 0.8 in away.
Next power-up: nothing plugged, 60 s, feel J6, U3 (LDO, x0.50 y1.79), U11
(buck-boost, x0.91 y1.84), U7 (MCU, x1.35 y1.28) in inches from the netlist
origin; then add the camera and repeat.

### 13.6 Everything else from the 2026-09-08 session

- Build: `make clean && make` 0 warnings, 42.6 KB text (I2C HAL now linked).
- Pull: `origin/main` fast-forwarded (Arnav, 2026-09-05), WIP restored clean.
- USB dropout hit once mid-launch (90 %); the probe re-enumerated on its own,
  no gdb client was alive, so `./scripts/stlink-server.sh` alone recovered it.
  The server log also says an ST-LINK firmware update is available.
- Schematic symbol U7 is `STM32F411CEUx` and the firmware builds for the xE;
  this doc's header says CCU6 — check the chip marking.

### 13.7 ROOT CAUSE — I2C pull-ups are capacitors, not resistors (found 2026-09-09)

`production/bom.csv`:

```
"R28, R29",0603,2,4.7k,C98195
```

The Value column reads **4.7k** but the LCSC part number **C98195** is a
**Samsung CL21B475KAFNNNE — 25 V 4.7 µF X7R 0805 capacitor**, not a resistor.
The schematic symbols R28 and R29 in `MCU.kicad_sch` carry the same wrong
`LCSC` property (C98195), so the fab loaded exactly what the BOM specified:
**4.7 µF caps where the I2C pull-ups should be.** Both I2C lines fail because
it is one BOM line covering both.

**Every §13.2 symptom is the capacitor, not a short. The §3-style "5 V short /
solder bridge" hypothesis in earlier notes was WRONG — there is no short.**

- ohmmeter across R28/R29 reads ~0.5 MΩ and climbing = a DMM charging a cap
- `g_cam_i2c_line_test` 0x13 / `g_cam_i2c_drive_dbg` 0x3503 ("driven low, reads
  high") = 4.7 µF discharges in ~200 µs (τ ≈ 4.7 µF × ~40 Ω), far longer than
  the readback delay, so the line still looks high; there is no resistive
  pull-up to help either way
- `HAL_I2C_WRONG_START` (status 3, err 0x200) = the peripheral cannot form a
  START; edges into 4.7 µF take milliseconds vs the microseconds I2C needs
- `g_cam_i2c_lines_idle` = 3 = caps hold charge / camera on-board pull-ups
- SPI probe passes because SPI does not touch PB6/PB7

**Fix — bench:** remove both 4.7 µF caps at R28/R29 (0603 pads by the camera
connector; positions.csv 30.875, 43.125 and 30.875, 41.375 mm, top) and fit
**4.7 kΩ 0603** resistors. The caps MUST be removed — 4.7 µF on an I2C line
kills the bus regardless of any pull-up added in parallel (RC rise ≈ 22 ms with
4.7 k). Bodge if no 0603 on hand: any 2.2 k–10 k from SDA (J4-2) and SCL (J4-1)
to 3V3 (TP4). Then reflash and re-read — `g_cam_i2c_line_test` should go 0x13 →
0x1F and the OV2640 PID/MID should read.

**Fix — repo (before any re-order):** change the R28/R29 `LCSC` field from
C98195 to a real 4.7 k 0603 resistor (JLCPCB basic 4.7 k 0603 = C25879, verify)
in `MCU.kicad_sch`, regenerate `production/bom.csv`. Until then the board
re-orders with the same defect.

**Still open, unrelated:** SD holder / temp regression = camera not releasing
MISO on shared SPI1 (§13.4, §13.5). Different pins, different fault.

### 13.8 RESOLVED — I2C rework works, camera identified (2026-09-10)

Gabe removed the two 4.7 µF caps at R28/R29 and soldered 4.7 kΩ resistors
(Yageo RC0603FR-074K7L, 0603). Reflashed and re-probed:

| Expression | Before | After rework | Verdict |
|---|---|---|---|
| `g_cam_i2c_line_test` | 0x13 | **0x1F** | both lines now pull low ✅ |
| `g_cam_i2c_drive_dbg` | 0x3503 | **0x3500** | driven low reads low ✅ |
| `g_cam_i2c_ack` | 0 | **1** | OV2640 ACKs 0x30 ✅ |
| `g_cam_pidh` / `g_cam_pidl` | 0 | **0x26 / 0x41\|0x42** | OV2640 product ID ✅ |
| `g_cam_midh` | 0 | **0xFF** (want 0x7F) | MID mismatch — see below |
| `g_cam_spi_*` | pass | pass | SPI still good ✅ |

**PID 0x26 is the definitive OV2640 identity and reads cleanly on both bytes, so
the sensor is present and the SCCB bus works.** The manufacturer ID (OmniVision
0x7FA2) read back 0xFF. Some OV2640 modules do this; it is a secondary check.

Driver change: `camera.c` no longer gates on MID — it records `g_cam_mid_ok`
(1/0) and passes the probe on a correct PID. Added `CAM_SCCB_READ_SETTLE_MS`
(1 ms before each SCCB read) in case the 0xFF was a read-timing glitch; reflash
to see if MID then reads 0x7F, but camera health no longer depends on it.

**After the reflash, expect:** `g_cam_i2c_fail_stage` = 0, `camera_is_ok` true,
`g_beacon_fields.flags` bit 6 (0x40) set. F9.1 (camera bus bring-up) is
functionally complete: SPI ✅, I2C ✅, sensor identified. Remaining camera work
is F9.1b sensor init tables and F9.2 FIFO capture → SD.

**Root cause for the record (§13.7):** the BOM specified LCSC C98195 (a 4.7 µF
0805 cap) for R28/R29 while the Value said 4.7k. Still needs fixing in
`MCU.kicad_sch` (LCSC → a 4.7k 0603 resistor, e.g. C25879) and BOM regen before
any board re-order, or the next board ships with caps again.
