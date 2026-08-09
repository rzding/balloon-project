# STM32 flight-computer firmware

STM32F411CEU6 firmware for the SSI high-altitude balloon flight computer. CubeMX generates `Core/`; application code lives in `App/`.

## Coding standards

Follow [Software Documents/Main Developmental Roadmap.md](../Software%20Documents/Main%20Developmental%20Roadmap.md) **§3** (architecture and coding practices) and [Firmware Development Guidelines.md](../Software%20Documents/Firmware%20Development%20Guidelines.md) (verification, deferred HW, host tests). In short:

- Layered design: `main` → `app_init` / `app_run` → drivers → `spi_bus` → HAL
- Edit CubeMX output only inside `/* USER CODE BEGIN/END */` blocks
- One device = one module (`foo.c` / `foo.h`); public APIs use `bool *_init(void)` and read/poll functions
- Fail-operational: set `error_flags`; never block the mission loop on subsystem faults
- No `malloc` in the flight loop; fixed-size buffers; finite HAL timeouts
- Deferred hardware checks: track in roadmap **§21 Bench verification backlog** until the board is available

**Host unit tests** (pure logic, no HAL): `tests/host/` — run manually; see `tests/host/README.md`. Cursor agents document run commands but do not execute tests by default.

`.clang-format` is deferred (not required for Phase F0).

## Build

From this directory:

```bash
make clean && make
```

Requires `arm-none-eabi-gcc` (e.g. STM32CubeCLT). Artifacts are written to `build/` (gitignored):

- `build/balloon-project-stm32mx.elf`
- `build/balloon-project-stm32mx.hex`
- `build/balloon-project-stm32mx.bin`

## Add a new App module

1. Add `App/Inc/foo.h` and `App/Src/foo.c` (one module = one `.c` / `.h` pair).
2. List `App/Src/foo.c` in Makefile `C_SOURCES` (re-add App entries after CubeMX Makefile regen).
3. `#include "foo.h"` from callers; `-IApp/Inc` is already in `C_INCLUDES`.
4. Regenerate IDE IntelliSense DB: `make compile_commands`.

## IDE IntelliSense (Cursor / VS Code)

Repo-root workspace config (keeps editor diagnostics aligned with the Makefile):

- `compile_commands.json` — same defines/includes as `make`
- `.clangd` — clangd + `arm-none-eabi-gcc` query-driver
- Repo `.vscode/c_cpp_properties.json` + `.vscode/settings.json`

After adding App sources, run `make compile_commands`, then **Developer: Reload Window** if squiggles remain.

## SWD / flash

**Hardware:** ST-Link (or Nucleo as SWD bridge) to the flight-computer SWD header (SWDIO, SWDCLK, GND, 3V3, NRST as wired on the PCB).

**Tooling (any one):**

- STM32CubeProgrammer — flash `build/balloon-project-stm32mx.elf` or `.bin`
- `st-flash write build/balloon-project-stm32mx.bin 0x08000000`
- OpenOCD + GDB — load `build/balloon-project-stm32mx.elf`

**Expected behavior after reset:** `main` → peripheral init → `app_init()` → `while (1) { app_run(); }`

## Layout

```text
balloon-project-stm32mx/
  Core/          # CubeMX — edit ONLY USER CODE regions
  App/
    Inc/         # Public headers per module
    Src/         # One .c per module
  Makefile       # Every App/Src/*.c listed; -IApp/Inc
```
