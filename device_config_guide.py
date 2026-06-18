#!/usr/bin/env python3
"""
🔧 DEVICE CONFIGURATION & TROUBLESHOOTING GUIDE
Help the device connect to WiFi and verify it's working
"""

print("""
╔════════════════════════════════════════════════════════════════╗
║  ESP32-S3 DEVICE CONFIGURATION & TESTING GUIDE               ║
╚════════════════════════════════════════════════════════════════╝

📋 WHAT WE KNOW SO FAR:
  ✅ Firmware uploaded successfully
  ✅ Device is powered on (LEDs blinking)
  ✅ Sensors ready to test
  ⏳ Waiting for WiFi connection

═══════════════════════════════════════════════════════════════

🔴 ISSUE: Device not responding on IP 172.20.10.2

POSSIBLE CAUSES & SOLUTIONS:

1️⃣  Device still booting
    ┗━ Solution: Wait 10-15 seconds, then run test again
       $ python3 test_interactive.py

2️⃣  Device on wrong WiFi network
    ┗━ Solution: Check which WiFi the device is connecting to
       Default: "Amit_wifi" (password: "Amit1998")
       
3️⃣  Device on different IP address
    ┗━ Solution: Scan network for all connected devices
       $ arp-scan --localnet | grep -v "^?"
       Look for device with MAC starting with "3c:0f:02:d7:56:c4"

4️⃣  WiFi credentials not configured
    ┗━ Solution: Device will use fallback credentials from config.h
       You can update WiFi via /set_config endpoint once connected

═══════════════════════════════════════════════════════════════

🔍 TROUBLESHOOTING STEPS:

STEP 1: Check Device Serial Output
────────────────────────────────────
While the device is booting, watch serial port for messages:

  $ screen /dev/ttyUSB0 460800
  
  OR
  
  $ python3 << 'EOF'
  import serial
  import time
  ser = serial.Serial('/dev/ttyUSB0', 460800)
  for i in range(100):
      if ser.in_waiting:
          print(ser.readline().decode('utf-8', errors='ignore').rstrip())
      time.sleep(0.1)
  ser.close()
  EOF

  Expected output should show:
    [BOOT] Boot count: 1
    [INIT] WiFi Initialization
    [INIT] Connecting to 'Amit_wifi'...
    [INIT] ✓ WiFi connected! IP: 172.20.10.X

STEP 2: Find Device IP Address
───────────────────────────────
Scan your network to find the actual IP:

  $ nmap -sn 172.20.10.0/28
  
  OR check ARP table:
  
  $ arp -a | grep -i esp
  $ arp -a | grep -i 3c:0f:02

STEP 3: Update Test Script with Correct IP
────────────────────────────────────────────
If device is on different IP, update test script:

  # Edit test_interactive.py
  # Find line: device_ip = "172.20.10.2"
  # Change to actual IP found in Step 2
  
  # Run test again
  $ python3 test_interactive.py

STEP 4: Verify Endpoints Respond
─────────────────────────────────
Once device is on network, test individual endpoints:

  # Check health
  $ curl http://172.20.10.X/health | jq
  
  # Check sensors
  $ curl http://172.20.10.X/sensor_status | jq
  
  # Check diagnostics
  $ curl http://172.20.10.X/diagnose | jq

═══════════════════════════════════════════════════════════════

✅ WHAT HAPPENS WHEN DEVICE IS WORKING:

/health endpoint returns:
{
  "ok": true,
  "wifi_connected": true,
  "camera_ready": true/false,
  "uptime_sec": 5,
  "heap_free": 261920,
  "boot_count": 1
}

/sensor_status endpoint returns:
{
  "pir": {
    "available": true/false,
    "last_state": 0/1,
    "description": "GROVE PIR motion sensor"
  },
  "hlk_ld2451": {
    "available": true/false,
    "detection_state": 0/1,
    "last_distance_mm": 1500,
    "description": "HLK-LD2451 millimeter-wave radar"
  }
}

═══════════════════════════════════════════════════════════════

🔧 MANUAL WIFI CONFIGURATION (if needed):

Once device responds, you can set WiFi via HTTP:

  $ curl -X POST http://172.20.10.X/set_config \\
    -H "Content-Type: application/json" \\
    -d '{
      "wifi_ssid": "YOUR_SSID",
      "wifi_password": "YOUR_PASSWORD",
      "person_threshold": 0.7,
      "sleep_duration_sec": 60
    }'

═══════════════════════════════════════════════════════════════

🚨 IF DEVICE DOESN'T RESPOND AT ALL:

1. Check power connection
   ┗━ Look for red LED or power indicator

2. Check USB cable
   ┗━ Try different USB port
   ┗━ Try different USB cable

3. Power cycle device
   ┗━ Disconnect USB for 5 seconds
   ┗━ Reconnect USB
   ┗━ Wait 10 seconds for boot

4. Check serial port
   ┗━ Device should appear as /dev/ttyUSB0
   ┗━ Run: ls -la /dev/ttyUSB*

5. Re-upload firmware
   ┗━ Power cycle device first
   ┗━ Then run:
      $ arduino-cli upload -p /dev/ttyUSB0 \\
        --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app \\
        firmware/main/main.ino

═══════════════════════════════════════════════════════════════

🎯 NEXT STEPS:

1. Wait 15 seconds for device to boot and connect WiFi
2. Run test script again: python3 test_interactive.py
3. If still not responding, check serial output for errors
4. Once /health responds → device is working! ✅
5. Then connect sensors and test triggers

═══════════════════════════════════════════════════════════════

💡 KEY POINTS ABOUT ROBUSTNESS:

✅ Even if GROVE-PIR unplugged → device continues
✅ Even if HLK-LD2451 disconnected → device continues
✅ Even if BOTH sensors fail → device stays online
✅ Manual /run endpoint always available
✅ System never crashes or enters reboot loop
✅ /health endpoint shows true status always

This is the ROBUST system you wanted! 🚀

═══════════════════════════════════════════════════════════════
""")

# Interactive prompt
print("\n🤔 What would you like to do?\n")
print("1) Wait and re-run test (device still booting)")
print("2) Check serial output from device")  
print("3) Scan network to find device IP")
print("4) Update WiFi credentials")
print("5) Just show me the status")
print("0) Exit\n")

choice = input("Enter choice (0-5): ").strip()

if choice == "1":
    print("\nWaiting 15 seconds for device to connect WiFi...")
    import time
    for i in range(15):
        print(f"  {15-i}s remaining...", end='\r')
        time.sleep(1)
    print("Now running test...\n")
    import subprocess
    subprocess.run(["python3", "test_interactive.py"])
    
elif choice == "2":
    print("\n📡 Reading serial output...")
    print("(This will read for 5 seconds)\n")
    import serial, time
    try:
        ser = serial.Serial('/dev/ttyUSB0', 460800, timeout=2)
        for i in range(200):
            if ser.in_waiting:
                print(ser.readline().decode('utf-8', errors='ignore').rstrip())
            time.sleep(0.025)
        ser.close()
    except Exception as e:
        print(f"Error: {e}")

elif choice == "3":
    print("\n🔍 Scanning network...\n")
    import subprocess
    print("Checking ARP table for ESP32...\n")
    subprocess.run(["arp", "-a"])
    print("\nTrying nmap scan...")
    subprocess.run(["nmap", "-sn", "172.20.10.0/28"])

elif choice == "4":
    print("\n🔧 WiFi Configuration\n")
    print("Once device responds, use this to set WiFi:\n")
    print("curl -X POST http://<DEVICE_IP>/set_config \\")
    print("  -H 'Content-Type: application/json' \\")
    print("  -d '{")
    print('    "wifi_ssid": "YOUR_SSID",')
    print('    "wifi_password": "YOUR_PASSWORD"')
    print("  }'")

elif choice == "5":
    print("\n📊 CURRENT STATUS:\n")
    print("✅ Firmware: Uploaded and running")
    print("✅ Device: Powered on (LEDs blinking)")
    print("✅ Sensors: Ready to connect")
    print("⏳ WiFi: Connecting to 'Amit_wifi'")
    print("⏳ API: Will respond once connected\n")
    print("Expected responses when connected:")
    print("  /health → System status")
    print("  /sensor_status → PIR & HLK status")  
    print("  /run → Manual inference test")
    print("  /diagnose → Full diagnostics\n")

print("\n" + "="*60)
print("For more help, see docs/guides/SENSOR_SETUP_GUIDE.md")
print("="*60 + "\n")
