import serial
import os
from datetime import datetime
from PIL import Image

SERIAL_PORT = "COM4"
BAUD_RATE = 115200

SAVE_DIR = r"C:\Users\Shachar\Desktop\EE- University\Final Project\ESP32\April-May\01.05- model\captured_from_esp32"
os.makedirs(SAVE_DIR, exist_ok=True)

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)

print("Connected to", SERIAL_PORT)
print("Listening for predictions and images...")
print()

while True:
    line = ser.readline()

    if not line:
        continue

    text = line.decode(errors="replace").strip()

    if text:
        print(text)

    if text.startswith("IMG_BEGIN"):
        parts = text.split()

        if len(parts) != 4:
            print("Bad IMG_BEGIN line:", text)
            continue

        width = int(parts[1])
        height = int(parts[2])
        image_size = int(parts[3])

        print(f"Receiving image: {width}x{height}, {image_size} bytes")

        image_data = ser.read(image_size)

        if len(image_data) != image_size:
            print("Failed to read full image")
            print("Expected:", image_size, "Got:", len(image_data))
            continue

        end_line = ser.readline().decode(errors="replace").strip()

        if end_line == "":
            end_line = ser.readline().decode(errors="replace").strip()

        if end_line != "IMG_END":
            print("Warning: expected IMG_END, got:", end_line)

        img = Image.frombytes("L", (width, height), image_data)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        file_path = os.path.join(SAVE_DIR, f"esp32_capture_{timestamp}.png")

        img.save(file_path)

        print("Saved image:", file_path)
        print()