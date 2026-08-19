# Firmware Development Guidelines

> **Project:** SSI High-Altitude Balloon Flight Computer  
> **MCU:** STM32F411CEU6 | **Firmware tree:** `balloon-project-stm32mx/`

---

## Document control

| Field | Value |
|---|---|
| Title | Firmware Development Guidelines |
| Location | `Software Documents/Firmware Development Guidelines.md` |
| Status | Active |
| Audience | Firmware engineers, Cursor agents, reviewers |
| Related | [Main Developmental Roadmap.md](Main%20Developmental%20Roadmap.md) §3, §21; [`balloon-project-stm32mx/README.md`](../balloon-project-stm32mx/README.md); [Logic Analyzer Bench Guide.md](Logic%20Analyzer%20Bench%20Guide.md) |

### Revision history

| Rev | Date | Author | Notes |
|---|---|---|---|
| 0.1 | 2026-08-06 | Firmware | Initial guidelines: verification gates, deferred HW, host tests, bench backlog, agent policy |
| 0.2 | 2026-08-15 | Firmware | Next-phase coding unblocked rule; FAQ rows; §21 does not block F2–F7 driver work |
| 0.3 | 2026-08-15 | Firmware | F4.1 host-test note (SPI config = bench); F4.2 CVD host-test target |
| 0.4 | 2026-08-15 | Firmware | F4.2 `test_max31865_cvd` authored (manual run pending) |
| 0.5 | 2026-08-15 | Firmware | F4.2 CVD inverse root fix; `test_max31865_cvd` manual pass |
| 0.6 | 2026-08-15 | Firmware | F4.3 `temp_sample_from_raw` in `test_max31865_cvd`; `temp_read` on bench only |
| 0.7 | 2026-08-15 | Firmware | F4.3 host test manual pass |
| 0.8 | 2026-08-19 | Firmware | F5.1 `test_gps_rx` authored (ring + LF line extract; manual run pending) |
| 0.9 | 2026-08-19 | Firmware | F5.1 `test_gps_rx` manual pass (test harness fix: sequential drain, wrap trailing line) |
| 0.10 | 2026-08-19 | Firmware | F5.2 `test_gps_nmea` authored (GGA/RMC parse; manual pass 2026-08-19) |
| 0.11 | 2026-08-19 | Firmware | F5.3 `gps_sample_has_fix` / `gps_has_fix`; `test_gps_nmea` fix-validity vectors (manual pass 2026-08-19) |
| 0.12 | 2026-08-19 | Firmware | Logic analyzer bench: `make BENCH=1` / `APP_BENCH_BUS_EXERCISE`; [Logic Analyzer Bench Guide.md](Logic%20Analyzer%20Bench%20Guide.md) |

---

## 1. Purpose

Complement the roadmap’s phase gates with **how to verify and proceed** when hardware is not available. This document is the reference for:

- Continuing driver work without blocking on a physical board or Renode
- Where to put deferred hardware checks
- How host-side tests are written and run
- What Cursor agents should and should not do

**Coding architecture and module APIs** remain in the roadmap **§3**. This file covers **verification workflow** only.

---

## 2. Verification gates

| Gate | What it proves | Blocks next *work package*? | Blocks *phase exit / F12*? |
|---|---|---|---|
| **Build + code review** | Compiles; APIs match roadmap; fail-soft paths | Soft — usually required | Soft |
| **Host unit tests** | Pure logic (scale, endian, compensation math) | Recommended for math/protocol | Soft |
| **Bench verification** | Real SPI, pins, power, sensor physics | No for sequential driver coding | Yes for phase HW exit |
| **HIL (F11)** | Full stack on real hardware | No until F11 | **Yes** for flight readiness |

**Work package complete (software):** code merged under `App/`, Makefile updated, software verification noted, roadmap work-package checklist ticked.

**Next-phase coding unblocked:** current phase is software-complete **and** the next phase’s entry criteria are met (software-complete of listed dependencies). Bench verification does **not** block sequential driver work F2–F7.

**Phase exit complete:** all work packages done **and** hardware exit criteria cleared (or explicitly deferred to §21 with a bring-up plan).

**Flight readiness (F12):** requires F11 HIL pass on real hardware. Deferred bench items do not substitute for HIL.

---

## 3. Deferred hardware (no board yet)

It is **correct and expected** to implement F2.3, F3, F4, F5, etc. without a physical board or simulator.

When hardware is unavailable:

1. Implement the work package per roadmap §3 standards.
2. Run `make clean && make` (build gate).
3. Note software verification in the roadmap (date + pass/fail).
4. Add or update **hardware checks** in roadmap **§21 Bench verification backlog**.
5. Do **not** tick phase hardware exit criteria until the board is exercised.
6. Do **not** claim Phase F2 (or any phase) fully complete if HW exit criteria remain open — unless all work packages are done and HW is explicitly deferred in §21.
7. **Do** start the next sequential driver phase (e.g. F4 after F3 software-complete) when entry criteria are met; open §21 items are not a coding blocker for F2–F7.

This matches how F0 and F1 are already tracked: software complete, bench pending.

---

## 4. Renode and simulation

**Renode** is useful later for MCU + bus regression CI when peripheral models exist. For this project today:

- There is no drop-in ICM-42688-P, MS5611, MAX31865, or RFM95W model for this PCB.
- Building those models is a real project; it should **not** block driver implementation.
- **Do not** pause F2–F4 to set up Renode unless a team member owns peripheral models.

**Revisit Renode when:**

- The board is back and you want automated regression, or
- Someone can maintain thin SPI slave stubs (WHO_AM_I + fixed sample data) shared across sensors.

No Renode install guide lives in this repo — avoids stale tooling docs. Link external Renode docs when models are added.

---

## 5. Host-side unit tests

### 5.1 Split driver code

| Layer | Examples | Testable without hardware? |
|---|---|---|
| **SPI / HAL glue** | `spi_bus_transfer`, register read/write in `imu.c` | No — needs bench or model |
| **Pure conversion** | LSB→SI scale, endian pack/unpack, MS5611 PROM math, NMEA parse | **Yes** — host `gcc` |

Prefer extracting or isolating pure functions so they can be tested on the developer machine (macOS/Linux) without STM32 HAL.

### 5.2 Location

Host tests live under:

```text
balloon-project-stm32mx/tests/host/
```

See [`balloon-project-stm32mx/tests/host/README.md`](../balloon-project-stm32mx/tests/host/README.md) for run instructions when a harness exists.

### 5.3 What to test first (examples)

| Phase | Good host-test targets |
|---|---|
| F2.3 | Accel/gyro raw → SI scale at ±16 g / ±2000 dps — `test_imu_scale` verified pass 2026-08-09 (manual) |
| F3.1 | MS5611 PROM CRC4 — `test_ms5611_crc` verified pass 2026-08-09 (manual; AN520 vector) |
| F3.2 | MS5611 24-bit ADC big-endian unpack — `test_ms5611_adc` (manual run pending) |
| F3.3 | MS5611 compensation from known PROM + ADC values — `test_ms5611_comp` (manual run pending; datasheet B3) |
| F3.4 | `baro_sample_from_raw` composition (compensate + ISA) in `test_ms5611_comp`; `baro_read` on bench only |
| F4.1 | MAX31865 CONFIG write/read-back — bench only (SPI glue; no host test) |
| F4.2 | PT1000 resistance → °C (Callendar–Van Dusen) — `test_max31865_cvd` verified pass 2026-08-15 (manual) |
| F4.3 | `temp_sample_from_raw` composition in `test_max31865_cvd`; `temp_read` on bench only — verified pass 2026-08-15 (manual) |
| F5.1 | USART RX ring + LF line extract — `test_gps_rx` verified pass 2026-08-19 (manual) |
| F5.2 | NMEA GGA/RMC parse, `lat_e7`/`lon_e7`/alt/sats — `test_gps_nmea` verified pass 2026-08-19 (manual) |
| F5.3 | `gps_sample_has_fix` / `gps_has_fix` from GGA quality + RMC status — `test_gps_nmea` fix-validity vectors verified pass 2026-08-19 (manual) |
| F8 | Packetizer CRC16, field packing |

---

## 6. Running tests (human / developer)

**Policy:** Cursor agents **author** tests and **document** how to run them. Agents **do not execute** host tests, HIL scripts, Renode, or flash-and-verify loops unless the user explicitly asks in that message.

When a harness exists, the developer runs tests manually:

```bash
cd balloon-project-stm32mx/tests/host
# Exact command documented in tests/host/README.md (e.g. make, or ./run_tests.sh)
```

After adding or changing tests, the agent must tell you the exact command to run and where it is documented.

**Firmware build** (always safe for agents and humans):

```bash
cd balloon-project-stm32mx
make clean && make
```

**Flash** (human or explicit user request only): see `balloon-project-stm32mx/README.md` § SWD / flash.

---

## 7. Bench verification backlog

All deferred hardware checks are tracked in one place:

**[Main Developmental Roadmap.md §21 — Bench verification backlog](Main%20Developmental%20Roadmap.md#21-bench-verification-backlog)**

After each sensor or subsystem work package:

1. Add the hardware checks needed to validate that work (WHO_AM_I, register read-back, motion, pressure range, etc.).
2. Clear ticks during **bring-up week** when the board is available.
3. Clearing §21 is required for **full phase exit**, not for starting the next driver phase.
4. F11 (system HIL) remains the full-stack gate — §21 is per-subsystem bench, not a substitute for F11.

---

## 8. Cursor agent expectations

When implementing firmware in this repo:

1. Follow roadmap **§3** (architecture, APIs, fail-operational).
2. Follow **this document** for verification and blocking rules.
3. Continue work packages without inventing Renode or “need hardware first” blockers.
4. Update roadmap work-package checklists and §21 when completing software.
5. Write host tests for pure logic when practical; document run commands in `tests/host/README.md`.
6. **Do not** run host unit tests, Renode, HIL, or flash verification unless the user explicitly requests it in that message.
7. After adding tests, provide the user the exact manual run command.

Project rules in `.cursor/rules/` reinforce these points for every session.

---

## 9. Quick reference

| Question | Answer |
|---|---|
| Can I code F2.3 without a board? | Yes |
| Can I tick F2 HW exit without a board? | No — use §21 backlog |
| Can I start F4 after F3 software-complete with F3 HW still open? | Yes |
| Can I start F11 without a board? | No |
| What does “F1 complete” mean in F4 entry? | F1 software-complete |
| When is bring-up week? | When PCB + ST-LINK are available; tick §21 then |
| Can I claim F12 without F11? | No |
| Should I set up Renode now? | No — defer unless models are owned |
| Who runs tests? | Developer manually; not the agent by default |
| Where is deferred HW tracked? | Roadmap §21 |
| How do I get live SPI for a logic analyzer? | `make BENCH=1` in `balloon-project-stm32mx/` — see [Logic Analyzer Bench Guide.md](Logic%20Analyzer%20Bench%20Guide.md). Default `make` keeps SPI quiet after boot; GPS UART still runs. Do not ship `BENCH=1` as flight firmware. |
