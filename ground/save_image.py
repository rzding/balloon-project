#!/usr/bin/env python3
"""
save_image.py — extract JPEG downlinks from the ESP32 ground station.

The ESP32 (ground/esp32_receiver) prints each reassembled image as base64
between markers:

    ===IMG BEGIN id=N w=W h=H len=L===
    <base64...>
    ===IMG END===

This reads those blocks and writes img<N>.jpg. Robust to a leading dummy byte
from the ArduChip FIFO: the payload is trimmed to the JPEG SOI (FF D8) .. EOI
(FF D9) markers when present.

Usage:
    # from a saved Serial Monitor capture (File > Save in the monitor, or a log):
    python3 save_image.py capture.txt

    # live from the serial port (needs: pip install pyserial):
    python3 save_image.py --port /dev/tty.usbserial-XXXX --baud 115200

    # or pipe:
    cat capture.txt | python3 save_image.py -
"""
import base64
import re
import sys

BEGIN = re.compile(r"===IMG BEGIN id=(\d+) w=(\d+) h=(\d+) len=(\d+)===")
END = "===IMG END==="


def trim_jpeg(data: bytes) -> bytes:
    soi = data.find(b"\xff\xd8")
    if soi < 0:
        return data  # no SOI found; write raw so the bytes aren't lost
    eoi = data.rfind(b"\xff\xd9")
    return data[soi:eoi + 2] if eoi > soi else data[soi:]


def flush(meta, b64_lines):
    if meta is None:
        return
    ident, w, h, ln = meta
    try:
        raw = base64.b64decode("".join(b64_lines))
    except Exception as e:
        print(f"  ! image {ident}: base64 decode failed: {e}", file=sys.stderr)
        return
    img = trim_jpeg(raw)
    name = f"img{ident}.jpg"
    with open(name, "wb") as f:
        f.write(img)
    tag = "" if img[:2] == b"\xff\xd8" else "  (no JPEG SOI — raw bytes)"
    print(f"  -> wrote {name}  {w}x{h}  {len(img)} bytes{tag}")


def process(lines):
    meta = None
    buf = []
    count = 0
    for line in lines:
        line = line.rstrip("\r\n")
        m = BEGIN.search(line)
        if m:
            flush(meta, buf)
            meta = (int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)))
            buf = []
            count += 1
            continue
        if line.strip() == END:
            flush(meta, buf)
            meta = None
            buf = []
            continue
        if meta is not None and line.strip():
            buf.append(line.strip())
    flush(meta, buf)  # in case END was cut off
    if count == 0:
        print("No image blocks found.", file=sys.stderr)


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return
    if args[0] == "--port":
        try:
            import serial  # pyserial
        except ImportError:
            print("pip install pyserial for --port mode", file=sys.stderr)
            sys.exit(1)
        port = args[1]
        baud = int(args[args.index("--baud") + 1]) if "--baud" in args else 115200
        print(f"reading {port} @ {baud} — Ctrl-C to stop")
        with serial.Serial(port, baud, timeout=1) as ser:
            def gen():
                while True:
                    yield ser.readline().decode("utf-8", "replace")
            try:
                process(gen())
            except KeyboardInterrupt:
                pass
    elif args[0] == "-":
        process(sys.stdin)
    else:
        with open(args[0], "r", encoding="utf-8", errors="replace") as f:
            process(f)


if __name__ == "__main__":
    main()
