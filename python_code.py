import serial
import struct
import os
from datetime import datetime

#HDLK2451 Radar and ESP32 CAM Integration - Python Script to Receive and Save Images
SAVE_DIR = r"C:\Users\Shachar\Desktop\EE- University\Final Project\ESP32\HDLK2451 integration\radar_images"
SERIAL_PORT = "COM4"
BAUD_RATE = 115200

os.makedirs(SAVE_DIR, exist_ok=True)

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=15)

print("Listening for radar triggered images...")

while True:
    line = ser.readline()

    if line == b"IMG_BEGIN\r\n" or line == b"IMG_BEGIN\n":
        size_bytes = ser.read(4)

        if len(size_bytes) != 4:
            print("Failed to read image size")
            continue

        image_size = struct.unpack("<I", size_bytes)[0]
        print(f"Image size: {image_size} bytes")

        image_data = ser.read(image_size)

        if len(image_data) != image_size:
            print("Failed to read full image")
            print(f"Expected {image_size}, got {len(image_data)}")
            continue

        filename = datetime.now().strftime("radar_%Y%m%d_%H%M%S.jpg")
        filepath = os.path.join(SAVE_DIR, filename)

        with open(filepath, "wb") as f:
            f.write(image_data)

        print(f"Saved image: {filepath}")