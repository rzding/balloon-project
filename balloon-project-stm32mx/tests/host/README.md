# Host unit tests

Host-side tests for **pure firmware logic** that does not need STM32 HAL, SPI, or a physical board.

Examples: IMU LSB→SI scale, MS5611 compensation math, NMEA parsing, telemetry packet CRC/endian packing.

## Status

**Harness not yet added.** When the first host test lands (e.g. F2.3 scale helpers or F3 compensation), add a `Makefile` or `run_tests.sh` here and document the exact run command below.

## Manual execution policy

- **Developers** run host tests on their machine after changes.
- **Cursor agents** may create and update tests and this README, but **must not execute** the test suite unless the user explicitly asks in that message.
- After adding tests, agents must tell you the exact command to run (copied from this file).

## How to run (template — update when harness exists)

When a harness is added, replace this section with real commands. Expected pattern:

```bash
cd balloon-project-stm32mx/tests/host
make          # or: ./run_tests.sh
```

Requirements will be documented here (e.g. host `gcc`, Unity, or Python).

## What belongs here vs on the bench

| Test type | Location |
|---|---|
| Scale, endian, CRC, parse, compensation math | `tests/host/` (this tree) |
| WHO_AM_I, SPI transfers, sensor physics, RF | Roadmap **§21 Bench verification backlog** |
| Full stack soak, fail-soft under stress | Roadmap **§16 Phase F11 (HIL)** |

See [Firmware Development Guidelines.md](../../Software%20Documents/Firmware%20Development%20Guidelines.md) for the full verification workflow.
