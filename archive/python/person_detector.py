import os
import json
import subprocess
import urllib.request
from datetime import datetime


def _ensure_route():
    """Re-add the route to the ESP32 subnet if it got dropped (needs sudo)."""
    try:
        subprocess.run(
            ["sudo", "-S", "ip", "addr", "add", "172.20.10.3/28", "dev", "enp0s8"],
            input=b"Amit1998\n", capture_output=True
        )
        subprocess.run(
            ["sudo", "-S", "ip", "route", "add", "172.20.10.0/28",
             "dev", "enp0s8", "src", "172.20.10.3"],
            input=b"Amit1998\n", capture_output=True
        )
    except Exception:
        pass

# Set this to the IP printed by the ESP32 on serial at boot.
# Look for the line:  "WiFi connected!  IP: x.x.x.x"
# Quick check command:
#   python3 -c "import serial; s=serial.Serial('/dev/ttyUSB0',460800,timeout=20); [print(s.readline().decode(errors='replace'),end='') for _ in range(80)]"
ESP32_IP = "172.20.10.2"  # update if board gets a new IP (check serial at boot)

SAVE_DIR = os.path.join(os.path.dirname(__file__), "captured_images")
os.makedirs(SAVE_DIR, exist_ok=True)


def get_ip():
    global ESP32_IP
    if not ESP32_IP:
        ESP32_IP = input("Enter ESP32 IP (shown on serial at boot): ").strip()
    return ESP32_IP


def read_inference():
    ip = get_ip()
    base = f"http://{ip}"

    # 1. Trigger capture + inference (auto-restore route on first failure)
    for attempt in range(2):
        try:
            with urllib.request.urlopen(f"{base}/run", timeout=30) as resp:
                results = json.loads(resp.read().decode())
            break
        except Exception as e:
            if attempt == 0:
                print("  [route lost — restoring...]")
                _ensure_route()
            else:
                print(f"  [/run error: {e}]")
                return {}, None

    # 2. Fetch the JPEG image that was analyzed
    image_path = None
    try:
        with urllib.request.urlopen(f"{base}/image.jpg", timeout=20) as resp:
            jpg_data = resp.read()

        best_label = results.get("top", "unknown")
        confidence = int(results.get("top_score", 0) * 100)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{timestamp}_{best_label}_{confidence}pct.jpg"
        image_path = os.path.join(SAVE_DIR, filename)
        with open(image_path, "wb") as f:
            f.write(jpg_data)
    except Exception as e:
        print(f"  [/image.jpg error: {e}]")

    return results, image_path


print(f"Images will be saved to: {SAVE_DIR}")
_ensure_route()  # make sure bridged adapter route is active
print("Press Enter to capture and analyze. Press Ctrl+C to quit.")
print()

try:
    while True:
        input("[ Press Enter to capture ]")
        print("Capturing...")

        results, image_path = read_inference()

        if not results:
            print("No result received.\n")
            continue

        print("\n--- Results ---")
        for label, score in results.get("labels", results).items():
            bar = "#" * int(score * 20)
            print(f"  {label:12s}: {score:.1%}  {bar}")
        if "inference_ms" in results:
            print(f"  Inference time: {results['inference_ms']} ms")
        if image_path:
            print(f"  Image saved:    {image_path}")
        print()

except KeyboardInterrupt:
    print("\nStopped.")
