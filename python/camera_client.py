import requests
import time
from datetime import datetime
from pathlib import Path

ESP32_IP = "172.20.10.4"

STATUS_URL = f"http://{ESP32_IP}/status"
PHOTO_URL = f"http://{ESP32_IP}/photo.jpg"

SAVE_DIR = Path("captured_images")
SAVE_DIR.mkdir(exist_ok=True)

last_boot_count = None
photo_saved_for_boot = False

print("Starting ESP32 Camera Wakeup client...")
print(f"Status URL: {STATUS_URL}")
print(f"Photo URL: {PHOTO_URL}")
print(f"Saving images to: {SAVE_DIR.resolve()}")

while True:
    try:
        response = requests.get(STATUS_URL, timeout=3)

        if response.status_code == 200:
            data = response.json()

            boot_count = data.get("boot_count")
            reason = data.get("wakeup_reason")
            photo_available = data.get("photo_available")
            photo_size = data.get("photo_size_bytes")
            sleep_in_ms = data.get("sleep_in_ms")

            print(
                f"ESP32 awake | "
                f"boot_count={boot_count} | "
                f"photo_available={photo_available} | "
                f"photo_size={photo_size} | "
                f"sleep_in_ms={sleep_in_ms} | "
                f"reason={reason}"
            )

            if boot_count != last_boot_count:
                last_boot_count = boot_count
                photo_saved_for_boot = False

            if photo_available and not photo_saved_for_boot:
                photo_response = requests.get(PHOTO_URL, timeout=8)

                if photo_response.status_code == 200:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = SAVE_DIR / f"esp32_photo_boot_{boot_count}_{timestamp}.jpg"

                    with open(filename, "wb") as file:
                        file.write(photo_response.content)

                    print(f"Saved photo: {filename}")
                    photo_saved_for_boot = True
                else:
                    print(f"Photo request failed: HTTP {photo_response.status_code}")

        else:
            print(f"HTTP error: {response.status_code}")

    except requests.exceptions.RequestException:
        print("ESP32 is not responding. It may be sleeping.")

    except ValueError:
        print("ESP32 returned invalid JSON")
        print(response.text)

    time.sleep(1)