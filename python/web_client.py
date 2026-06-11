import requests
import time

ESP32_IP = "172.20.10.4"
URL = f"http://{ESP32_IP}/status"

print("Starting ESP32 WiFi client...")
print(f"Reading from: {URL}")

while True:
    try:
        response = requests.get(URL, timeout=3)

        if response.status_code == 200:
            data = response.json()
            print("Detection status:", data["detection"])
        else:
            print(f"HTTP error: {response.status_code}")

    except requests.exceptions.RequestException as e:
        print("Could not connect to ESP32")
        print(e)

    except ValueError:
        print("ESP32 returned invalid JSON")
        print(response.text)

    time.sleep(1)