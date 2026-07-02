#!/usr/bin/env python3
"""Extract a JPEG photo from the ESP32 person detector over serial.

Sends the PHOTO command, reads the base64 stream between PHOTO_BEGIN/PHOTO_END
markers, decodes it and saves a .jpg into captured_images/.

Usage:
    python scripts/extract_photo.py [--port /dev/ttyUSB0] [--baud 460800] [-n 1]
"""
import argparse
import base64
import os
import sys
import time
from datetime import datetime

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(os.path.dirname(HERE), "captured_images")


def grab_one(ser: serial.Serial) -> bytes | None:
    ser.reset_input_buffer()
    ser.write(b"PHOTO\n")
    ser.flush()

    # Wait for the PHOTO_BEGIN <jpg_len> <b64_len> header.
    jpg_len = None
    b64_len = None
    t0 = time.time()
    while time.time() - t0 < 15:
        line = ser.readline().decode("utf-8", "replace").strip()
        if not line:
            continue
        if line.startswith("PHOTO_ERROR") or line.startswith("PHOTO_ABORT"):
            print(f"  device error: {line}", file=sys.stderr)
            return None
        if line.startswith("PHOTO_BEGIN"):
            parts = line.split()
            jpg_len = int(parts[1]) if len(parts) > 1 else None
            b64_len = int(parts[2]) if len(parts) > 2 else None
            break

    if b64_len is None:
        print("  no PHOTO_BEGIN header received", file=sys.stderr)
        return None

    # ACK handshake: send a byte, read exactly the next chunk, repeat.
    ser.timeout = 5
    buf = bytearray()
    while len(buf) < b64_len:
        ser.write(b"n")
        ser.flush()
        remaining = b64_len - len(buf)
        chunk = ser.read(min(256, remaining))
        if not chunk:
            print(f"  timeout at {len(buf)}/{b64_len} b64 bytes", file=sys.stderr)
            return None
        buf.extend(chunk)

    # final ACK so the device prints PHOTO_END and frees its buffer
    ser.write(b"n")
    ser.flush()
    time.sleep(0.2)

    try:
        data = base64.b64decode(bytes(buf))
    except Exception as exc:  # noqa: BLE001
        print(f"  base64 decode failed: {exc}", file=sys.stderr)
        return None
    if jpg_len is not None and len(data) != jpg_len:
        print(f"  warning: expected {jpg_len} bytes, got {len(data)}", file=sys.stderr)
    return data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("-n", "--count", type=int, default=1, help="number of photos")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)

    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(0.5)

    saved = 0
    for i in range(args.count):
        print(f"[{i + 1}/{args.count}] requesting photo...")
        data = grab_one(ser)
        if not data or not data.startswith(b"\xff\xd8"):
            print("  no valid JPEG received", file=sys.stderr)
            continue
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        path = os.path.join(OUT_DIR, f"esp32_{ts}_{i:02d}.jpg")
        with open(path, "wb") as f:
            f.write(data)
        print(f"  saved {len(data)} bytes -> {path}")
        saved += 1
        if i + 1 < args.count:
            time.sleep(1.0)

    ser.close()
    print(f"Done. {saved}/{args.count} photo(s) saved to {OUT_DIR}")
    return 0 if saved else 1


if __name__ == "__main__":
    raise SystemExit(main())
