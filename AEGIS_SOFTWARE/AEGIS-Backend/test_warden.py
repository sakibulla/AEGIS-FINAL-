#!/usr/bin/env python3
"""Test script for Warden backend integration and camera streaming."""

import requests
import json
import time
from datetime import datetime

BACKEND_URL = "http://10.75.11.83:8000"
WARDEN_DIRECT_IP = "10.75.11.120"  # Update after getting IP from Warden serial monitor

def print_header(title):
    """Print formatted section header."""
    print(f"\n{'='*60}")
    print(f"  {title}")
    print(f"{'='*60}\n")

def test_backend_health():
    """Check if backend is online."""
    print_header("Testing Backend Health")
    try:
        response = requests.get(f"{BACKEND_URL}/", timeout=5)
        print(f"✅ Backend Status: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return True
    except Exception as e:
        print(f"❌ Backend Error: {e}")
        return False

def test_warden_camera_direct():
    """Test direct access to Warden camera."""
    print_header("Testing Warden Camera (Direct Access)")
    
    # Test camera status
    try:
        response = requests.get(f"http://{WARDEN_DIRECT_IP}:81/status", timeout=5)
        print(f"✅ Camera Status: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
    except Exception as e:
        print(f"❌ Camera Status Error: {e}")
        return False
    
    # Test snapshot
    try:
        print(f"\n📸 Testing snapshot capture...")
        response = requests.get(f"http://{WARDEN_DIRECT_IP}:81/capture", timeout=10)
        print(f"✅ Snapshot Response: {response.status_code}")
        print(f"✅ Snapshot Size: {len(response.content)} bytes")
        
        # Save snapshot with timestamp
        filename = f"warden_snapshot_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        with open(filename, 'wb') as f:
            f.write(response.content)
        print(f"✅ Snapshot saved: {filename}")
        return True
    except Exception as e:
        print(f"❌ Snapshot Error: {e}")
        return False

def test_warden_stream_info():
    """Display Warden stream URLs."""
    print_header("Warden Camera Stream URLs")
    
    print(f"🎥 Direct Stream URLs:")
    print(f"   Root Page:     http://{WARDEN_DIRECT_IP}:81/")
    print(f"   MJPEG Stream:  http://{WARDEN_DIRECT_IP}:81/stream")
    print(f"   Snapshot:      http://{WARDEN_DIRECT_IP}:81/capture")
    print(f"   Status JSON:   http://{WARDEN_DIRECT_IP}:81/status")
    
    print(f"\n🔗 Backend Proxy URLs:")
    print(f"   Stream:        {BACKEND_URL}/api/v1/video/stream/Warden")
    print(f"   Snapshot:      {BACKEND_URL}/api/v1/video/snapshot/Warden")
    print(f"   Video Viewer:  {BACKEND_URL}/video")
    
    print(f"\n💡 TIP: Open these URLs in your browser or VLC player")
    return True

def test_warden_status():
    """Get Warden status from backend."""
    print_header("Testing Warden Backend Status")
    try:
        response = requests.get(f"{BACKEND_URL}/api/v1/bots/Warden", timeout=5)
        print(f"✅ Status Code: {response.status_code}")
        
        if response.status_code == 200:
            data = response.json()
            print(f"\nWarden Status:")
            print(f"  Bot ID: {data.get('bot', {}).get('bot_id', 'N/A')}")
            print(f"  Status: {data.get('bot', {}).get('status', 'N/A')}")
            print(f"  IP Address: {data.get('bot', {}).get('ip_address', 'N/A')}")
            print(f"  WiFi RSSI: {data.get('bot', {}).get('wifi_rssi', 'N/A')} dBm")
            
            system_info = data.get('bot', {}).get('system_info', {})
            if system_info:
                print(f"\nSystem Info:")
                print(f"  Free Heap: {system_info.get('free_heap', 0):,} bytes")
                print(f"  Free PSRAM: {system_info.get('free_psram', 0):,} bytes")
            
            video_info = data.get('video', {})
            if video_info:
                print(f"\nVideo Info:")
                print(f"  Stream URL: {video_info.get('stream_url', 'N/A')}")
                print(f"  Status: {video_info.get('status', 'N/A')}")
        
        return True
    except Exception as e:
        print(f"❌ Status Error: {e}")
        return False

def test_warden_detections():
    """Get Warden vision detections from backend."""
    print_header("Testing Warden Vision Detections")
    try:
        response = requests.get(f"{BACKEND_URL}/api/v1/bots/Warden/detections", timeout=5)
        print(f"✅ Detections Code: {response.status_code}")
        
        if response.status_code == 200:
            data = response.json()
            source = data.get('source', 'unknown')
            detections = data.get('detections', [])
            
            print(f"\nDetection Source: {source}")
            print(f"Detection Count: {len(detections)}")
            
            if detections:
                print(f"\nActive Detections:")
                for i, det in enumerate(detections, 1):
                    label = det.get('label', 'unknown')
                    confidence = det.get('confidence', 0)
                    is_threat = det.get('is_threat', False)
                    threat_icon = "🚨" if is_threat else "✅"
                    
                    print(f"  {i}. {threat_icon} {label}")
                    print(f"     Confidence: {confidence:.1f}%")
                    print(f"     Threat: {is_threat}")
                    
                    bbox = det.get('bbox', {})
                    if bbox:
                        print(f"     BBox: x={bbox.get('x')}, y={bbox.get('y')}, "
                              f"w={bbox.get('w')}, h={bbox.get('h')}")
            else:
                print(f"  No active detections")
        
        return True
    except Exception as e:
        print(f"❌ Detections Error: {e}")
        return False

def test_incidents():
    """Get all incidents from backend."""
    print_header("Testing Incidents API")
    try:
        response = requests.get(f"{BACKEND_URL}/api/v1/incidents", timeout=5)
        print(f"✅ Incidents Code: {response.status_code}")
        
        if response.status_code == 200:
            incidents = response.json()
            
            # Filter Warden incidents
            warden_incidents = [inc for inc in incidents if inc.get('bot_id') == 'Warden']
            
            print(f"\nTotal Incidents: {len(incidents)}")
            print(f"Warden Incidents: {len(warden_incidents)}")
            
            if warden_incidents:
                print(f"\nRecent Warden Incidents:")
                for inc in warden_incidents[-3:]:  # Show last 3
                    print(f"\n  ID: {inc.get('id')}")
                    print(f"  Type: {inc.get('type')}")
                    print(f"  Severity: {inc.get('severity')}")
                    print(f"  Message: {inc.get('message')}")
                    print(f"  Timestamp: {inc.get('timestamp')}")
                    print(f"  Active: {inc.get('active')}")
            else:
                print(f"  No Warden incidents recorded")
        
        return True
    except Exception as e:
        print(f"❌ Incidents Error: {e}")
        return False

def monitor_telemetry(duration=30):
    """Monitor Warden telemetry for specified duration."""
    print_header(f"Monitoring Warden Telemetry ({duration}s)")
    print("Watching for status updates and detections...")
    print("Press Ctrl+C to stop\n")
    
    start_time = time.time()
    last_status = None
    detection_count = 0
    
    while time.time() - start_time < duration:
        try:
            response = requests.get(f"{BACKEND_URL}/api/v1/bots/Warden", timeout=5)
            if response.status_code == 200:
                data = response.json()
                bot = data.get('bot', {})
                timestamp = datetime.now().strftime('%H:%M:%S')
                
                status = bot.get('status', 'UNKNOWN')
                detections = bot.get('vision_detections', [])
                
                # Print only if status changed or there are detections
                if status != last_status or detections:
                    print(f"[{timestamp}] Status: {status} | Detections: {len(detections)}")
                    last_status = status
                    
                    if detections:
                        detection_count += len(detections)
                        for det in detections:
                            label = det.get('label', 'unknown')
                            conf = det.get('confidence', 0)
                            threat = det.get('is_threat', False)
                            threat_icon = "🚨" if threat else "ℹ️"
                            print(f"  {threat_icon} {label} ({conf:.1f}%) - Threat: {threat}")
                
            time.sleep(3)  # Match Warden's 3-second telemetry interval
            
        except KeyboardInterrupt:
            print("\n\n⏹️  Monitoring stopped by user")
            break
        except Exception as e:
            timestamp = datetime.now().strftime('%H:%M:%S')
            print(f"[{timestamp}] ❌ Monitor Error: {e}")
            time.sleep(3)
    
    elapsed = time.time() - start_time
    print(f"\n📊 Monitoring Summary:")
    print(f"   Duration: {elapsed:.1f}s")
    print(f"   Total Detections: {detection_count}")
    print(f"   Last Status: {last_status}")

def main():
    """Run all tests."""
    print("\n" + "="*60)
    print("  AEGIS WARDEN BACKEND INTEGRATION TEST SUITE")
    print("="*60)
    print(f"\nConfiguration:")
    print(f"  Backend URL: {BACKEND_URL}")
    print(f"  Warden IP: {WARDEN_DIRECT_IP}")
    print(f"\n⚠️  Update WARDEN_DIRECT_IP if needed (check serial monitor)")
    
    input("\nPress Enter to start tests...")
    
    tests = [
        ("Backend Health Check", test_backend_health),
        ("Warden Stream URLs", test_warden_stream_info),
        ("Warden Camera Direct", test_warden_camera_direct),
        ("Warden Backend Status", test_warden_status),
        ("Warden Vision Detections", test_warden_detections),
        ("Incidents API", test_incidents),
    ]
    
    results = []
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
            time.sleep(1)  # Small delay between tests
        except Exception as e:
            print(f"❌ Test '{test_name}' crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print_header("Test Summary")
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"{status} - {test_name}")
    
    print(f"\n📊 Total: {passed}/{total} tests passed")
    
    # Optional: Live monitoring
    if passed >= 4:  # At least backend + camera working
        print("\n" + "="*60)
        response = input("Monitor live telemetry? (y/n): ")
        if response.lower() == 'y':
            duration = input("Duration in seconds (default 30): ")
            duration = int(duration) if duration.isdigit() else 30
            monitor_telemetry(duration)
    
    print("\n" + "="*60)
    print("  Test Complete!")
    print("="*60)
    print(f"\n💡 Next Steps:")
    print(f"   1. Open camera in browser: http://{WARDEN_DIRECT_IP}:81/")
    print(f"   2. View in VLC: http://{WARDEN_DIRECT_IP}:81/stream")
    print(f"   3. Backend dashboard: {BACKEND_URL}/video")
    print()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⏹️  Tests interrupted by user")
    except Exception as e:
        print(f"\n\n❌ Fatal error: {e}")
