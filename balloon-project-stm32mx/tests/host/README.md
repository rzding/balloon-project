# Host unit tests

Host-side tests for **pure firmware logic** that does not need STM32 HAL, SPI, or a physical board.

Examples: IMU LSB→SI scale, MS5611 compensation math, NMEA parsing, telemetry packet CRC/endian packing.

## Status

**Harness active** — `test_imu_scale` (F2.3 accel/gyro scale + endian unpack).

## Manual execution policy

- **Developers** run host tests on their machine after changes.
- **Cursor agents** may create and update tests and this README, but **must not execute** the test suite unless the user explicitly asks in that message.
- After adding tests, agents must tell you the exact command to run (copied from this file).

## How to run

```bash
cd balloon-project-stm32mx/tests/host
make
./test_imu_scale
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

## What belongs here vs on the bench

| Test type | Location |
|---|---|
| Scale, endian, CRC, parse, compensation math | `tests/host/` (this tree) |
| WHO_AM_I, SPI transfers, sensor physics, RF | Roadmap **§21 Bench verification backlog** |
| Full stack soak, fail-soft under stress | Roadmap **§16 Phase F11 (HIL)** |

See [Firmware Development Guidelines.md](../../Software%20Documents/Firmware%20Development%20Guidelines.md) for the full verification workflow.
