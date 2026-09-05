#!/usr/bin/env python3
"""
Test script to verify AEGIS backend endpoints.
Run this to test the backend without ESP32 hardware.

Usage:
    python test_endpoints.py
"""

import sys
import json
import requests
from datetime import datetime

# Backend URL - change if running on different host/port
BASE_URL = "http://localhost:8000"

def print_section(title):
    """Print a section header"""
    print("\n" + "=" * 60)
    print(f"  {title}")
    print("=" * 60)

def test_health_check():
    """Test the root endpoint"""
    print_section("Testing Health Check")
    try:
        response = requests.get(f"{BASE_URL}/")
        print(f"Status Code: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return response.status_code == 200
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_telemetry_ingest():
    """Test telemetry ingestion endpoint"""
    print_section("Testing Telemetry Ingestion")
    
    payload = {
        "bot_id": "Guardian",
        "kind": "telemetry",
        "payload": {
            "status": "PATROL",
            "battery_pct": 85,
            "system_info": {
                "free_heap": 120000,
                "psram": 256000
            },
            "wifi_rssi": -60,
            "ip_address": "192.168.0.102",
            "timestamp": datetime.now().isoformat()
        }
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/v1/telemetry/ingest",
            json=payload,
            headers={"Content-Type": "application/json"}
        )
        print(f"Status Code: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return response.status_code == 200
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_vision_detection():
    """Test vision detection endpoint"""
    print_section("Testing Vision Detection")
    
    payload = {
        "bot_id": "Guardian",
        "kind": "vision_detection",
        "payload": {
            "label": "Person",
            "confidence": 92.5,
            "is_threat": False,
            "owner_id": 1,
            "bbox": {
                "x": 12.0,
                "y": 18.0,
                "w": 33.0,
                "h": 42.0
            }
        }
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/v1/telemetry/ingest",
            json=payload,
            headers={"Content-Type": "application/json"}
        )
        print(f"Status Code: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return response.status_code == 200
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_incident_report():
    """Test incident reporting endpoint"""
    print_section("Testing Incident Report")
    
    payload = {
        "bot_id": "Guardian",
        "type": "INTRUDER",
        "message": "Unknown person detected in restricted area",
        "severity": "CRITICAL"
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/v1/incidents/report",
            json=payload,
            headers={"Content-Type": "application/json"}
        )
        print(f"Status Code: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return response.status_code == 200
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_get_bots():
    """Test get bots endpoint"""
    print_section("Testing Get Bots")
    
    try:
        response = requests.get(f"{BASE_URL}/api/v1/bots")
        print(f"Status Code: {response.status_code}")
        if response.status_code == 200:
            bots = response.json()
            print(f"Number of bots: {len(bots)}")
            if bots:
                print(f"First bot: {json.dumps(bots[0], indent=2)}")
            return True
        else:
            print(f"[FAIL] Response: {response.text}")
            return False
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_get_incidents():
    """Test get incidents endpoint"""
    print_section("Testing Get Incidents")
    
    try:
        response = requests.get(f"{BASE_URL}/api/v1/incidents")
        print(f"Status Code: {response.status_code}")
        if response.status_code == 200:
            incidents = response.json()
            print(f"Number of incidents: {len(incidents)}")
            if incidents:
                print(f"First incident: {json.dumps(incidents[0], indent=2)}")
            return True
        else:
            print(f"[FAIL] Response: {response.text}")
            return False
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_video_frame_upload():
    """Test video frame upload endpoint"""
    print_section("Testing Video Frame Upload")
    
    # Create a small dummy JPEG (just for testing the endpoint)
    dummy_jpeg = b'\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9'
    
    try:
        files = {
            'frame': ('frame.jpg', dummy_jpeg, 'image/jpeg')
        }
        data = {
            'bot_id': 'Guardian'
        }
        
        response = requests.post(
            f"{BASE_URL}/api/v1/video/frame",
            files=files,
            data=data
        )
        print(f"Status Code: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return response.status_code == 200
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def test_current_scenario():
    """Test get current scenario endpoint"""
    print_section("Testing Current Scenario")
    
    try:
        response = requests.get(f"{BASE_URL}/api/v1/test/current-scenario")
        print(f"Status Code: {response.status_code}")
        if response.status_code == 200:
            scenario = response.json()
            print(f"Scenario: {scenario['scenario_name']}")
            print(f"Scenario Index: {scenario['scenario_index']}/{scenario['scenario_count']}")
            return True
        else:
            print(f"[FAIL] Response: {response.text}")
            return False
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False

def run_all_tests():
    """Run all endpoint tests"""
    print("\n" + "=" * 60)
    print("  AEGIS Backend API Test Suite")
    print("=" * 60)
    print(f"\nTesting backend at: {BASE_URL}")
    print("Make sure the backend is running: uvicorn app.main:app --reload\n")
    
    results = {
        "Health Check": test_health_check(),
        "Telemetry Ingest": test_telemetry_ingest(),
        "Vision Detection": test_vision_detection(),
        "Incident Report": test_incident_report(),
        "Get Bots": test_get_bots(),
        "Get Incidents": test_get_incidents(),
        "Video Frame Upload": test_video_frame_upload(),
        "Current Scenario": test_current_scenario(),
    }
    
    # Summary
    print_section("Test Summary")
    passed = sum(1 for result in results.values() if result)
    total = len(results)
    
    for test_name, result in results.items():
        status = "[PASS]" if result else "[FAIL]"
        print(f"{status}  {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    if passed == total:
        print("\nAll tests passed! Backend is ready for ESP32 integration.")
    else:
        print(f"\n{total - passed} test(s) failed. Check the backend logs.")
    
    return passed == total

if __name__ == "__main__":
    if len(sys.argv) > 1:
        BASE_URL = sys.argv[1]
    
    success = run_all_tests()
    sys.exit(0 if success else 1)
