import serial
import struct
import os
from datetime import datetime
from PIL import Image
import io
import time

SERIAL_PORT = "COM4"
BAUD_RATE = 115200

DATASET_DIR = r"C:\Users\Shachar\Desktop\EE- University\Final Project\code\Final-Project\men detection model\Capturing\dataset"

IMAGE_SIZE = 224

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)
time.sleep(2)

print("Connected to ESP32 S3 CAM")
print("Each ENTER press captures one image")
print()

def choose_class():
    print("Choose capture class:")
    print("1 = person")
    print("2 = no_person")

    while True:
        choice = input("Enter 1 or 2: ").strip()

        if choice == "1":
            return "person"

        if choice == "2":
            return "no_person"

        print("Invalid choice. Try again.")

def get_next_index(save_dir, prefix):
    existing_files = [
        name for name in os.listdir(save_dir)
        if name.lower().endswith(".jpg") and name.startswith(prefix)
    ]

    return len(existing_files) + 1

def read_line():
    line = ser.readline()

    if not line:
        return ""

    return line.decode(errors="ignore").strip()

def wait_for_line(expected_line):
    while True:
        decoded = read_line()

        if decoded == "":
            print("No data from ESP32")
            continue

        print("ESP32 says:", decoded)

        if decoded == expected_line:
            return

def capture_image_from_esp32():
    ser.reset_input_buffer()

    ser.write(b"CAPTURE\n")
    ser.flush()

    print("Sent CAPTURE command to ESP32")

    wait_for_line("IMG_BEGIN")

    size_bytes = ser.read(4)

    if len(size_bytes) != 4:
        raise RuntimeError("Failed to read image size")

    image_size = struct.unpack("<I", size_bytes)[0]

    print("Image size:", image_size, "bytes")

    if image_size <= 0 or image_size > 3000000:
        raise RuntimeError("Image size looks wrong")

    image_data = ser.read(image_size)

    if len(image_data) != image_size:
        raise RuntimeError(
            f"Failed to read full image. Expected {image_size}, received {len(image_data)}"
        )

    wait_for_line("IMG_END")

    return image_data

def convert_to_224(image_data):
    image = Image.open(io.BytesIO(image_data)).convert("RGB")

    width, height = image.size
    print("Original camera image size:", width, "x", height)

    min_side = min(width, height)

    left = (width - min_side) // 2
    top = (height - min_side) // 2
    right = left + min_side
    bottom = top + min_side

    image = image.crop((left, top, right, bottom))
    image = image.resize((IMAGE_SIZE, IMAGE_SIZE))

    return image

def save_image(image, save_dir, prefix, index):
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"{prefix}_{index:03d}_{timestamp}.jpg"
    save_path = os.path.join(save_dir, filename)

    image.save(save_path, quality=95)

    print("Saved 224x224 image:")
    print(save_path)
    print()

def main():
    class_name = choose_class()

    save_dir = os.path.join(DATASET_DIR, class_name)
    os.makedirs(save_dir, exist_ok=True)

    prefix = class_name
    image_index = get_next_index(save_dir, prefix)

    print()
    print("Current class:", class_name)
    print("Saving images to:")
    print(save_dir)
    print()
    print("Press ENTER to capture one image")
    print("Type q and press ENTER to quit")
    print("Type c and press ENTER to change class")
    print()

    global ser

    while True:
        user_input = input("ENTER = capture, q = quit, c = change class: ").strip().lower()

        if user_input == "q":
            break

        if user_input == "c":
            class_name = choose_class()
            save_dir = os.path.join(DATASET_DIR, class_name)
            os.makedirs(save_dir, exist_ok=True)
            prefix = class_name
            image_index = get_next_index(save_dir, prefix)

            print()
            print("Changed class to:", class_name)
            print("Saving images to:")
            print(save_dir)
            print()
            continue

        try:
            print()
            print(f"Capturing {class_name} image number {image_index}")

            image_data = capture_image_from_esp32()
            image_224 = convert_to_224(image_data)

            save_image(image_224, save_dir, prefix, image_index)

            image_index += 1

        except Exception as e:
            print("Capture failed:")
            print(e)
            print("Try again.")
            print()

try:
    main()

finally:
    ser.close()
    print("Serial port closed")