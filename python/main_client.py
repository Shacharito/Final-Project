import requests
import time
import os
import serial
import re
from datetime import datetime

def find_esp32_ip_from_serial(serial_port, baud_rate=460800, timeout=20):
    """Monitors a serial port for the IP address reported by the ESP32."""
    print(f"Attempting to find ESP32 IP address on {serial_port}...")
    try:
        with serial.Serial(serial_port, baud_rate, timeout=timeout) as ser:
            start_time = time.time()
            while time.time() - start_time < timeout:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"  [SERIAL] {line}")
                ip_match = re.search(r'IP: (\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})', line)
                if ip_match:
                    ip = ip_match.group(1)
                    print(f"*** IP Address Found: {ip} ***")
                    return ip
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {serial_port}. {e}")
        print("Please ensure the ESP32 is connected and you have the correct port.")
    print("IP address not found in serial output.")
    return None

class ESP32Client:
    def __init__(self, ip_address):
        self.ip = ip_address
        self.base_url = f"http://{self.ip}"
        self.save_dir = "captured_images"
        os.makedirs(self.save_dir, exist_ok=True)
        print(f"Client initialized for ESP32 at {self.ip}")
        print(f"Images will be saved to '{self.save_dir}/'")

    def is_alive(self):
        """Check if the ESP32 is reachable."""
        try:
            response = requests.get(f"{self.base_url}/get_config", timeout=2)
            return response.status_code == 200
        except requests.exceptions.RequestException:
            return False

    def get_config(self):
        """Reads the current configuration from the ESP32."""
        try:
            response = requests.get(f"{self.base_url}/get_config", timeout=5)
            if response.status_code == 200:
                return response.json()
            else:
                print(f"Failed to get config: HTTP {response.status_code}")
                return None
        except requests.exceptions.RequestException as e:
            print(f"Error getting config: {e}")
            return None

    def set_config(self, ssid=None, password=None, sleep_sec=None, person_thresh=None):
        """Updates the configuration on the ESP32."""
        payload = {}
        if ssid: payload['ssid'] = ssid
        if password: payload['password'] = password
        if sleep_sec is not None: payload['sleep_sec'] = sleep_sec
        if person_thresh is not None: payload['person_thresh'] = person_thresh

        if not payload:
            print("No configuration to set.")
            return None

        try:
            response = requests.post(f"{self.base_url}/set_config", data=payload, timeout=10)
            if response.status_code == 200:
                print("Configuration updated successfully.")
                return response.json()
            else:
                print(f"Failed to set config: HTTP {response.status_code}")
                return None
        except requests.exceptions.RequestException as e:
            print(f"Error setting config: {e}")
            return None

    def run_inference(self):
        """Triggers an inference cycle on the ESP32."""
        print("Requesting inference from ESP32...")
        try:
            response = requests.get(f"{self.base_url}/run", timeout=30) # Long timeout for inference
            if response.status_code == 200:
                return response.json()
            else:
                print(f"Inference request failed: HTTP {response.status_code}")
                return None
        except requests.exceptions.RequestException as e:
            print(f"Error running inference: {e}")
            return None

    def save_image(self, results):
        """Downloads and saves the image associated with the inference results."""
        if not results or not results.get('ok'):
            return

        try:
            response = requests.get(f"{self.base_url}/image.jpg", timeout=10)
            if response.status_code == 200:
                label = results.get('top_label', 'unknown')
                score = int(results.get('top_score', 0) * 100)
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"{timestamp}_{label}_{score}pct.jpg"
                filepath = os.path.join(self.save_dir, filename)
                with open(filepath, "wb") as f:
                    f.write(response.content)
                print(f"Image saved: {filepath}")
            else:
                print(f"Failed to download image: HTTP {response.status_code}")
        except requests.exceptions.RequestException as e:
            print(f"Error downloading image: {e}")

def main_loop():
    # The ESP32's IP address, found via serial monitoring.
    esp_ip = "172.20.10.2"
    client = ESP32Client(esp_ip)

    print("\n--- ESP32 Person Detection Client ---")

    # --- Wait for ESP32 to come online ---
    print(f"\n1. Waiting for ESP32 to come online at {esp_ip}...")
    wait_time = 30
    is_online = False
    for i in range(wait_time):
        if client.is_alive():
            print("\nESP32 is online!")
            is_online = True
            break
        print(f"   Waiting... ({i+1}/{wait_time})", end="\r")
        time.sleep(1)

    if not is_online:
        print(f"\nError: ESP32 did not come online after {wait_time} seconds.")
        return

    # 2. Initial configuration
    print("\n2. Checking and setting configuration...")
    config = client.get_config()
    if config:
        print(f"   Current Config: Sleep={config.get('sleep_sec')}s, Threshold={config.get('person_thresh')}, Boot Count={config.get('boot_count')}")
        print(f"   Wakeup Reason: {config.get('wakeup_reason')}")

    # Set new configuration if needed
    print("   Updating configuration: Sleep duration 60s, Person threshold 0.7")
    client.set_config(sleep_sec=60, person_thresh=0.7)

    print("\n3. Running main detection loop (Press Ctrl+C to stop)...")
    while True:
        try:
            # The ESP32 will go to sleep on its own. We just wait for it to reappear.
            if not client.is_alive():
                print("   ESP32 is sleeping. Waiting for motion trigger to wake it up...", end="\r")
                time.sleep(2)
                continue

            # If we are here, the ESP32 is awake. Let's check why.
            config = client.get_config()
            if config and config.get('wakeup_reason') == "Wakeup by external trigger on GPIO13":
                 print("\n   Wakeup from motion detected! Running inference...")
                 results = client.run_inference()

                 if results and results.get('ok'):
                     print(f"   Inference complete in {results.get('inference_ms')}ms.")
                     print(f"   Result: {results.get('top_label')} ({results.get('top_score'):.2f})")
                     client.save_image(results)

                     # The ESP32 now handles the sleep logic internally.
                     # We just report what will happen.
                     is_person = results.get('top_label') == 'person' and results.get('top_score') >= config.get('person_thresh', 0.7)
                     if is_person:
                         print("   PERSON DETECTED! ESP32 will sleep for 2 minutes.")
                     else:
                         print("   No person detected. ESP32 will sleep for the default duration.")
                     
                     # Wait a moment for the ESP32 to go to sleep before we start polling again.
                     print("   Waiting for ESP32 to go to sleep...")
                     time.sleep(5)

            else:
                # The ESP32 is awake but not from a motion trigger (e.g. power on).
                # We can just wait for it to sleep or for a motion trigger.
                print("   ESP32 is awake but idle. Waiting for motion trigger...", end="\r")
                time.sleep(2)

        except KeyboardInterrupt:
            print("\nStopping client.")
            break
        except Exception as e:
            print(f"\nAn error occurred: {e}")
            time.sleep(10)


if __name__ == "__main__":
    main_loop()