# Host unit tests

Host-side tests for **pure firmware logic** that does not need STM32 HAL, SPI, or a physical board.

Examples: IMU LSB→SI scale, MS5611 compensation math, NMEA parsing, telemetry packet CRC/endian packing.

## Status

**Harness active** — `test_imu_scale` (F2.3), `test_ms5611_crc` (F3.1), `test_ms5611_adc` (F3.2), `test_ms5611_comp` (F3.3 compensation + ISA + F3.4 `baro_sample_from_raw`), `test_max31865_cvd` (F4.2 RTD unpack + CVD + F4.3 `temp_sample_from_raw`), `test_gps_rx` (F5.1 ring + LF line extract), `test_gps_nmea` (F5.2 GGA/RMC parse + F5.3 fix validity), `test_sdlog_name` (F6.2 `FLIGHT%03u.CSV` format + next index), `test_lora_frf` (F7.2 Hz → Frf), `test_packet_v1` (F7.4 pack/unpack/CRC-16/CCITT-FALSE), `test_mission_sm` (F8.1/F8.4 state walk + BURST latch + edge profiles), `test_schedule` (F8.2 LoRa/camera periods + due), `test_packetizer` (F8.3 fill + pack golden), `test_aprs_at` (F10.1 AT format / ACK parse).

## Manual execution policy

- **Developers** run host tests on their machine after changes.
- **Cursor agents** may create and update tests and this README, but **must not execute** the test suite unless the user explicitly asks in that message.
- After adding tests, agents must tell you the exact command to run (copied from this file).

## How to run

```bash
cd balloon-project-stm32mx/tests/host
make
./test_imu_scale
./test_ms5611_crc
./test_ms5611_adc
./test_ms5611_comp
./test_max31865_cvd
./test_gps_rx
./test_gps_nmea
./test_lora_frf
./test_packet_v1
./test_sdlog_name
./test_mission_sm
./test_schedule
./test_packetizer
./test_aprs_at
```

Ground decoder (F7.4 — host CLI, not in this Makefile):

```bash
cd ground
make
./decode_packet 0102002a00bc614e1683fed0f8b4073805dc05c8092effff05086ecf --rssi -72 --snr 9.5
```

Clean:

```bash
make clean
```

Requirements: host `cc` (clang/gcc) and `libm`.

## Verification log

| Test | Result | Date | Notes |
|---|---|---|---|
| `test_imu_scale` | pass | 2026-08-09 | Manual run by developer (`make` + `./test_imu_scale`) |
| `test_ms5611_crc` | pass | 2026-08-09 | Manual run (`make` + `./test_ms5611_crc`); AN520 golden vector |
| `test_ms5611_adc` | pending | — | Run `make && ./test_ms5611_adc` after F3.2 changes |
| `test_ms5611_comp` | pending | — | Run `make && ./test_ms5611_comp` after F3.3/F3.4 changes (comp + ISA + sample-from-raw) |
| `test_max31865_cvd` | pass | 2026-08-15 | Manual run (`make` + `./test_max31865_cvd`); F4.2 CVD + F4.3 `temp_sample_from_raw` composition; clean build (no warnings) |
| `test_gps_rx` | pass | 2026-08-19 | Manual run (`make` + `./test_gps_rx`); sequential two-line drain + last-line-wins; wrap uses valid trailing line |
| `test_gps_nmea` | pass | 2026-08-19 | Manual run (`make` + `./test_gps_nmea`); F5.2 GGA/RMC parse + F5.3 fix validity (52 checks) |
| `test_lora_frf` | pass | 2026-08-20 | Manual run (`make` + `./test_lora_frf`); golden Frf 915 MHz `0xE4C000`, 868 MHz `0xD90000` |
| `test_packet_v1` | pending | — | Run `make clean && make && ./test_packet_v1` after F7.4 changes; golden rich hex `0102002a00bc614e1683fed0f8b4073805dc05c8092effff05086ecf`; minimal CRC `0x18EF` (26-byte payload `0100…ffff0000`) |
| `test_sdlog_name` | pending | — | Run `make && ./test_sdlog_name` after F6.2 name/index helper changes |
| `test_mission_sm` | pass | 2026-09-05 | F8.1/F8.4: PAD→…→BEACON + BURST latch + freefall + edge gates (no-arm, no false ASCENT/FLOAT/LANDED, short freefall); re-run `make && ./test_mission_sm` after F8.4 |
| `test_schedule` | pass | 2026-09-05 | F8.2 period table + due timing + state-change / cam-off (`make && ./test_schedule`) |
| `test_packetizer` | pass | 2026-09-05 | F8.3 fill fields + temp override + fill→pack golden CRC (`make && ./test_packetizer`) |
| `test_aprs_at` | pending | — | Run `make && ./test_aprs_at` after F10.1 changes; locked SETGROUP/VOLUME/FILTER strings + ACK `:0` parse |

## What belongs here vs on the bench

| Test type | Location |
|---|---|
| Scale, endian, CRC, parse, compensation math | `tests/host/` (this tree) — **software gate** for pure logic; not a substitute for bench |
| WHO_AM_I, SPI transfers, sensor physics, RF | Roadmap **§21 Bench verification backlog** |
| Full stack soak, fail-soft under stress | Roadmap **§16 Phase F11 (HIL)** |

See [Firmware Development Guidelines.md](../../Software%20Documents/Firmware%20Development%20Guidelines.md) for the full verification workflow.
