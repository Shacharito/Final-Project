#!/bin/bash
# Quick firmware upload script - Skip compilation, use pre-compiled binaries

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT=${1:-/dev/ttyUSB0}
FIRMWARE=${2:-main}

echo "🚀 Quick Firmware Upload (No Compilation)"
echo "========================================"
echo "Port: $PORT"
echo "Firmware: $FIRMWARE"
echo ""

if [ "$FIRMWARE" = "main" ]; then
    BIN_DIR="$SCRIPT_DIR/main_firmware"
    NAME="main.ino"
    echo "📤 Uploading main.ino (robust firmware)..."
elif [ "$FIRMWARE" = "reference" ]; then
    BIN_DIR="$SCRIPT_DIR/reference_firmware"
    NAME="men_ai_model.ino"
    echo "📤 Uploading men_ai_model.ino (reference firmware)..."
else
    echo "❌ Unknown firmware: $FIRMWARE"
    echo "Usage: $0 [port] [firmware]"
    echo "  port: /dev/ttyUSB0 (default)"
    echo "  firmware: main | reference (default: main)"
    exit 1
fi

if [ ! -f "$BIN_DIR/${NAME}.merged.bin" ]; then
    echo "❌ Binary not found: $BIN_DIR/${NAME}.merged.bin"
    exit 1
fi

echo "📦 Using merged binary: $BIN_DIR/${NAME}.merged.bin"
echo ""

# Check if esptool is available
if ! command -v esptool.py &> /dev/null; then
    echo "⚠️  esptool.py not found. Install it:"
    echo "   pip install esptool"
    echo ""
    echo "Falling back to arduino-cli..."
    echo "Note: This will still use pre-compiled binaries if available"
    exit 1
fi

echo "🔌 Connecting to device..."
if ! esptool.py -p "$PORT" chip_id &> /dev/null; then
    echo "❌ Failed to connect to device on $PORT"
    echo "   Check USB connection and port"
    exit 1
fi

echo "✅ Device detected"
echo ""
echo "⏳ Erasing flash (this takes ~10 seconds)..."
esptool.py -p "$PORT" erase_flash

echo ""
echo "📥 Writing firmware..."
esptool.py -p "$PORT" write_flash 0x0 "$BIN_DIR/${NAME}.merged.bin"

echo ""
echo "🔄 Resetting device..."
esptool.py -p "$PORT" read_mac

echo ""
echo "✅ Upload complete!"
echo ""
echo "🔍 To view serial output:"
echo "   miniterm.py $PORT 460800"
echo "   or"
echo "   picocom -b 460800 $PORT"
