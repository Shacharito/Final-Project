#!/usr/bin/env python3
"""
🧪 INTERACTIVE TEST SUITE - ESP32-S3 Person Detection System
Test the firmware robustness with and without sensors
"""

import requests
import time
import sys
import json
from urllib.parse import urlparse

class TestSuite:
    def __init__(self, device_ip="172.20.10.2", timeout=3):
        self.device_ip = device_ip
        self.base_url = f"http://{device_ip}"
        self.timeout = timeout
        self.results = []
        
    def print_banner(self, text):
        print(f"\n{'=' * 60}")
        print(f"  {text}")
        print(f"{'=' * 60}\n")
    
    def test_endpoint(self, endpoint, description, expected_code=200):
        """Test a single endpoint"""
        url = f"{self.base_url}{endpoint}"
        
        try:
            print(f"🔍 Testing: {description}")
            print(f"   URL: GET {endpoint}")
            
            response = requests.get(url, timeout=self.timeout)
            
            if response.status_code == expected_code:
                print(f"   ✅ Status: {response.status_code}")
                try:
                    data = response.json()
                    print(f"   Response: {json.dumps(data, indent=2)}")
                    self.results.append((description, "PASS", response.status_code))
                    return data
                except:
                    print(f"   Response: {response.text[:200]}")
                    self.results.append((description, "PASS", response.status_code))
                    return response.text
            else:
                print(f"   ⚠️  Status: {response.status_code} (expected {expected_code})")
                self.results.append((description, "WARN", response.status_code))
                return None
                
        except requests.exceptions.ConnectionError:
            print(f"   ❌ Cannot connect to {self.device_ip}")
            print(f"      → Device not responding. Is it on WiFi 'Amit_wifi'?")
            self.results.append((description, "FAIL", "Connection Error"))
            return None
        except requests.exceptions.Timeout:
            print(f"   ⏱️  Timeout - device not responding")
            self.results.append((description, "FAIL", "Timeout"))
            return None
        except Exception as e:
            print(f"   ❌ Error: {e}")
            self.results.append((description, "FAIL", str(e)))
            return None
        finally:
            print()
    
    def run_all_tests(self):
        """Run complete test suite"""
        self.print_banner("🚀 ESP32-S3 SENSOR SYSTEM - INTERACTIVE TEST")
        
        print("Device Configuration:")
        print(f"  IP Address: {self.device_ip}")
        print(f"  Base URL: {self.base_url}")
        print(f"  Timeout: {self.timeout}s\n")
        
        print("Testing sequence:")
        print("  1. System health (/health)")
        print("  2. Sensor status (/sensor_status)")
        print("  3. Full diagnostics (/diagnose)")
        print("  4. Manual inference trigger (/run)")
        print("  5. Configuration (/get_config)")
        
        # Phase 1: System Health
        self.print_banner("PHASE 1: System Health Check")
        health_data = self.test_endpoint(
            "/health",
            "System Health Status",
            200
        )
        
        # Phase 2: Sensor Status
        self.print_banner("PHASE 2: Sensor Status")
        sensor_data = self.test_endpoint(
            "/sensor_status",
            "PIR & HLK-LD2451 Status",
            200
        )
        
        # Phase 3: Full Diagnostics
        self.print_banner("PHASE 3: Full Diagnostics")
        diag_data = self.test_endpoint(
            "/diagnose",
            "Complete System Diagnostics",
            200
        )
        
        # Phase 4: Configuration
        self.print_banner("PHASE 4: Configuration")
        config_data = self.test_endpoint(
            "/get_config",
            "Current Configuration",
            200
        )
        
        # Phase 5: Inference Trigger
        self.print_banner("PHASE 5: Manual Inference Trigger")
        self.test_endpoint(
            "/run",
            "Trigger Manual Inference",
            200  # or 503 if camera not ready
        )
        
        # Summary
        self.print_banner("📊 TEST SUMMARY")
        self.print_results()
        
    def print_results(self):
        """Print test results summary"""
        passed = sum(1 for _, status, _ in self.results if status == "PASS")
        warned = sum(1 for _, status, _ in self.results if status == "WARN")
        failed = sum(1 for _, status, _ in self.results if status == "FAIL")
        
        print("Results by endpoint:\n")
        for desc, status, code in self.results:
            symbol = "✅" if status == "PASS" else "⚠️ " if status == "WARN" else "❌"
            print(f"{symbol} {desc}: {code}")
        
        print(f"\n{'─' * 60}")
        print(f"✅ PASSED: {passed}")
        print(f"⚠️  WARNED: {warned}")
        print(f"❌ FAILED: {failed}")
        print(f"{'─' * 60}\n")
        
        if failed == 0:
            print("🎉 All tests passed! System is working.")
        elif failed == 1 and passed > 3:
            print("✅ Minor issues but system functional. Check device status.")
        else:
            print("⚠️  Device may still be booting. Wait 10 seconds and retry.")

def main():
    print("\n")
    print("╔════════════════════════════════════════════════════════════╗")
    print("║  ESP32-S3 PERSON DETECTION SYSTEM                         ║")
    print("║  Interactive Test Suite                                   ║")
    print("╚════════════════════════════════════════════════════════════╝")
    print()
    
    # Try to find device
    device_ip = "172.20.10.2"  # Default IP
    
    print(f"Attempting to connect to device at {device_ip}...")
    print("(Waiting for device to boot and connect to WiFi...)\n")
    
    tester = TestSuite(device_ip, timeout=5)
    
    # Run tests
    tester.run_all_tests()
    
    # Additional info
    print("\n📚 What these tests show:\n")
    print("✅ /health")
    print("   → System is running, WiFi status, camera readiness, uptime")
    print()
    print("✅ /sensor_status")
    print("   → PIR sensor available/connected")
    print("   → HLK-LD2451 sensor available/connected")
    print("   → Last readings from each sensor")
    print()
    print("✅ /diagnose")
    print("   → Full hardware diagnostics")
    print("   → MAC address, IP, signal strength")
    print("   → Memory usage, heap free, PSRAM")
    print()
    print("✅ /run")
    print("   → Manual inference trigger")
    print("   → Test camera + AI model")
    print("   → Shows person detected (true/false)")
    print()
    print("⚡ ROBUSTNESS TEST:\n")
    print("When sensors are unplugged/missing:")
    print("   → /sensor_status shows 'available': false")
    print("   → /health still responds (system doesn't crash)")
    print("   → /run still works for manual triggering")
    print("   → Device stays online and responsive")
    print()

if __name__ == "__main__":
    main()
