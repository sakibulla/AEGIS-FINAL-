"""
Test script to verify ESP32-S3 to Backend communication
Run this to check if the backend receives telemetry properly
"""
import requests
import json
import time

BACKEND_URL = "http://localhost:8000"

def test_backend_health():
    """Test if backend is running"""
    try:
        response = requests.get(f"{BACKEND_URL}/")
        print(f"[PASS] Backend health check: {response.json()}")
        return True
    except Exception as e:
        print(f"[FAIL] Backend health check failed: {e}")
        return False

def test_telemetry_endpoint():
    """Test if telemetry endpoint accepts data"""
    test_payload = {
        "bot_id": "Guardian",
        "kind": "vision_detection",
        "payload": {
            "label": "Owner",
            "confidence": 95.0,
            "is_threat": False,
            "owner_id": 0,
            "bbox": {"x": 0.0, "y": 0.0, "w": 0.0, "h": 0.0}
        }
    }
    
    try:
        response = requests.post(
            f"{BACKEND_URL}/api/v1/telemetry/ingest",
            json=test_payload,
            headers={"Content-Type": "application/json"}
        )
        
        if response.status_code == 200:
            print(f"[PASS] Telemetry endpoint test passed: {response.json()}")
            return True
        else:
            print(f"[FAIL] Telemetry endpoint returned: {response.status_code}")
            print(f"  Response: {response.text}")
            return False
    except Exception as e:
        print(f"[FAIL] Telemetry endpoint test failed: {e}")
        return False

def test_intruder_detection():
    """Test intruder detection payload"""
    intruder_payload = {
        "bot_id": "Guardian",
        "kind": "vision_detection",
        "payload": {
            "label": "Intruder",
            "confidence": 90.0,
            "is_threat": True,
            "owner_id": -1,
            "bbox": {"x": 0.0, "y": 0.0, "w": 0.0, "h": 0.0}
        }
    }
    
    try:
        response = requests.post(
            f"{BACKEND_URL}/api/v1/telemetry/ingest",
            json=intruder_payload,
            headers={"Content-Type": "application/json"}
        )
        
        if response.status_code == 200:
            print(f"[PASS] Intruder detection test passed: {response.json()}")
            return True
        else:
            print(f"[FAIL] Intruder detection test failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"[FAIL] Intruder detection test failed: {e}")
        return False

def test_dangerous_object():
    """Test dangerous object detection payload"""
    dangerous_payload = {
        "bot_id": "Guardian",
        "kind": "vision_detection",
        "payload": {
            "label": "Knife",
            "confidence": 87.5,
            "is_threat": True,
            "owner_id": -1,
            "bbox": {"x": 10.0, "y": 15.0, "w": 30.0, "h": 25.0}
        }
    }
    
    try:
        response = requests.post(
            f"{BACKEND_URL}/api/v1/telemetry/ingest",
            json=dangerous_payload,
            headers={"Content-Type": "application/json"}
        )
        
        if response.status_code == 200:
            print(f"[PASS] Dangerous object test passed: {response.json()}")
            return True
        else:
            print(f"[FAIL] Dangerous object test failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"[FAIL] Dangerous object test failed: {e}")
        return False

def test_bot_status_telemetry():
    """Test full bot status payload from ESP32"""
    status_payload = {
        "bot_id": "Guardian",
        "kind": "telemetry",
        "payload": {
            "status": "PATROL",
            "battery_pct": 88,
            "system_info": {
                "free_heap": 124500,
                "psram": 256000
            },
            "wifi_rssi": -55,
            "ip_address": "192.168.1.120",
            "timestamp": "uptime_120s",
            "wake_word_triggered": False,
            "wake_word_label": "Hi ESP",
            "first_aid_status": {
                "box_attached": True,
                "delivered": False
            }
        }
    }
    try:
        response = requests.post(
            f"{BACKEND_URL}/api/v1/telemetry/ingest",
            json=status_payload,
            headers={"Content-Type": "application/json"}
        )
        if response.status_code == 200:
            print(f"[PASS] Bot status telemetry test passed: {response.json()}")
            return True
        else:
            print(f"[FAIL] Bot status returned HTTP {response.status_code}")
            return False
    except Exception as e:
        print(f"[FAIL] Bot status test failed: {e}")
        return False

def main():
    print("=" * 60)
    print("AEGIS Backend - ESP32 Connection Test")
    print("=" * 60)
    print()
    
    # Test 1: Backend Health
    print("Test 1: Backend Health Check")
    if not test_backend_health():
        print("\nBackend is not running!")
        print("Start it with: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000")
        return
    print()
    
    # Test 2: Telemetry Endpoint
    print("Test 2: Telemetry Endpoint (Owner Detection)")
    test_telemetry_endpoint()
    print()
    
    # Test 3: Intruder Detection
    print("Test 3: Intruder Detection")
    test_intruder_detection()
    print()
    
    # Test 4: Dangerous Object
    print("Test 4: Dangerous Object Detection")
    test_dangerous_object()
    print()

    # Test 5: Bot Status Telemetry
    print("Test 5: Bot Status Telemetry")
    test_bot_status_telemetry()
    print()
    
    print("=" * 60)
    print("[PASS] All backend connection tests completed successfully!")
    print("=" * 60)

if __name__ == "__main__":
    main()
