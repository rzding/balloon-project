# Host unit tests

Host-side tests for **pure firmware logic** that does not need STM32 HAL, SPI, or a physical board.

Examples: IMU LSB→SI scale, MS5611 compensation math, NMEA parsing, telemetry packet CRC/endian packing.

## Status

**Harness active** — `test_imu_scale` (F2.3), `test_ms5611_crc` (F3.1), `test_ms5611_adc` (F3.2), `test_ms5611_comp` (F3.3 compensation + ISA + F3.4 `baro_sample_from_raw`), `test_max31865_cvd` (F4.2 RTD unpack + CVD + F4.3 `temp_sample_from_raw`), `test_gps_rx` (F5.1 ring + LF line extract), `test_gps_nmea` (F5.2 GGA/RMC parse + F5.3 fix validity).

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

## What belongs here vs on the bench

| Test type | Location |
|---|---|
| Scale, endian, CRC, parse, compensation math | `tests/host/` (this tree) — **software gate** for pure logic; not a substitute for bench |
| WHO_AM_I, SPI transfers, sensor physics, RF | Roadmap **§21 Bench verification backlog** |
| Full stack soak, fail-soft under stress | Roadmap **§16 Phase F11 (HIL)** |

See [Firmware Development Guidelines.md](../../Software%20Documents/Firmware%20Development%20Guidelines.md) for the full verification workflow.
