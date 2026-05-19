import requests
import time

ESP32_IP = "172.20.10.4"
URL = f"http://{ESP32_IP}/status"

print("Starting ESP32 Sleep Mode client...")
print(f"Reading from: {URL}")

while True:
    try:
        response = requests.get(URL, timeout=3)

        if response.status_code == 200:
            data = response.json()

            print(
                f"ESP32 awake | "
                f"boot_count={data.get('boot_count')} | "
                f"awake_for_ms={data.get('awake_for_ms')} | "
                f"sleep_in_ms={data.get('sleep_in_ms')} | "
                f"reason={data.get('wakeup_reason')}"
            )

        else:
            print(f"HTTP error: {response.status_code}")

    except requests.exceptions.RequestException:
        print("ESP32 is not responding. It may be sleeping.")

    except ValueError:
        print("ESP32 returned invalid JSON")
        print(response.text)

    time.sleep(1)