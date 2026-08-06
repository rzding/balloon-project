#!/usr/bin/env python3
"""Regenerate balloon-project-stm32mx/compile_commands.json from the Makefile C_SOURCES list.

Run from repo root or from balloon-project-stm32mx/:
  python3 scripts/gen_compile_commands.py
  make -C balloon-project-stm32mx compile_commands
"""

from __future__ import annotations

import json
import re
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FW = REPO / "balloon-project-stm32mx"
MAKEFILE = FW / "Makefile"
OUT = FW / "compile_commands.json"

# Local CubeCLT install (optional). Prefer PATH, then this fallback.
_CUBEclt_CC = "/opt/ST/STM32CubeCLT_1.21.0/GNU-tools-for-STM32/bin/arm-none-eabi-gcc"


def resolve_cc() -> str:
    found = shutil.which("arm-none-eabi-gcc")
    if found:
        return found
    if Path(_CUBEclt_CC).exists():
        return _CUBEclt_CC
    return "arm-none-eabi-gcc"


def parse_makefile_list(text: str, var: str) -> list[str]:
    """Parse a simple `VAR = \\` continued list of paths from the Cube Makefile."""
    m = re.search(rf"^{re.escape(var)}\s*=\s*\\?\s*\n((?:.*\\\n)*.*)$", text, re.M)
    if not m:
        raise SystemExit(f"Could not find {var} in {MAKEFILE}")
    block = m.group(1)
    items: list[str] = []
    for line in block.splitlines():
        line = line.strip().rstrip("\\").strip()
        if not line or line.startswith("#"):
            break
        items.append(line)
    return items


def main() -> int:
    text = MAKEFILE.read_text(encoding="utf-8")
    sources = parse_makefile_list(text, "C_SOURCES")

    includes = [
        f"-I{FW / 'Core/Inc'}",
        f"-I{FW / 'App/Inc'}",
        f"-I{FW / 'Drivers/STM32F4xx_HAL_Driver/Inc'}",
        f"-I{FW / 'Drivers/STM32F4xx_HAL_Driver/Inc/Legacy'}",
        f"-I{FW / 'Drivers/CMSIS/Device/ST/STM32F4xx/Include'}",
        f"-I{FW / 'Drivers/CMSIS/Include'}",
    ]
    defs = ["-DUSE_HAL_DRIVER", "-DSTM32F411xE"]
    cc = resolve_cc()

    base = [
        cc,
        "-c",
        "-mcpu=cortex-m4",
        "-mthumb",
        "-mfpu=fpv4-sp-d16",
        "-mfloat-abi=hard",
        *defs,
        *includes,
        "-Og",
        "-Wall",
        "-fdata-sections",
        "-ffunction-sections",
        "-g",
        "-gdwarf-2",
    ]

    entries = []
    for src in sources:
        abs_src = FW / src
        if not abs_src.is_file():
            print(f"warning: missing source {abs_src}", file=sys.stderr)
        obj = f"build/{Path(src).stem}.o"
        entries.append(
            {
                "directory": str(FW),
                "file": str(abs_src),
                "arguments": base + [src, "-o", obj],
            }
        )

    OUT.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(entries)} entries to {OUT.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
