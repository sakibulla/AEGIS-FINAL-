#!/usr/bin/env python3
"""
Test script for AEGIS Live Video Streaming endpoints.
Verifies snapshot generation, frame ingestion, IP configuration,
MJPEG streaming responses, and HTML surveillance station rendering.
"""
import sys
import json
import time
import requests

BASE_URL = "http://localhost:8000"


def print_section(title: str):
    print("\n" + "=" * 60)
    print(f"  {title}")
    print("=" * 60)


def test_video_status():
    print_section("Testing Video Status Endpoint (/api/v1/video/status)")
    try:
        res = requests.get(f"{BASE_URL}/api/v1/video/status", timeout=5)
        print(f"Status Code: {res.status_code}")
        print(f"Response: {json.dumps(res.json(), indent=2)}")
        return res.status_code == 200 and "bots" in res.json()
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_video_config():
    print_section("Testing Video Config Endpoint (/api/v1/video/config)")
    payload = {
        "bot_id": "Guardian",
        "ip_address": "10.75.11.83",
        "port": 80
    }
    try:
        res = requests.post(f"{BASE_URL}/api/v1/video/config", json=payload, timeout=5)
        print(f"Status Code: {res.status_code}")
        print(f"Response: {json.dumps(res.json(), indent=2)}")
        return res.status_code == 200 and res.json().get("ip_address") == "10.75.11.83"
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_video_snapshot():
    print_section("Testing Video Snapshot Endpoint (/api/v1/video/snapshot/Guardian)")
    try:
        res = requests.get(f"{BASE_URL}/api/v1/video/snapshot/Guardian", timeout=5)
        print(f"Status Code: {res.status_code}")
        print(f"Content-Type: {res.headers.get('Content-Type')}")
        print(f"Snapshot Size: {len(res.content)} bytes")
        # Check JPEG header
        is_jpeg = res.content.startswith(b'\xff\xd8')
        print(f"Valid JPEG magic bytes: {is_jpeg}")
        return res.status_code == 200 and is_jpeg
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_video_frame_ingest():
    print_section("Testing Frame Ingestion (/api/v1/video/frame)")
    dummy_jpeg = b'\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9'
    try:
        files = {'frame': ('test_frame.jpg', dummy_jpeg, 'image/jpeg')}
        data = {'bot_id': 'Guardian'}
        res = requests.post(f"{BASE_URL}/api/v1/video/frame", files=files, data=data, timeout=5)
        print(f"Status Code: {res.status_code}")
        print(f"Response: {json.dumps(res.json(), indent=2)}")
        return res.status_code == 200 and res.json().get("status") == "success"
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_mjpeg_stream():
    print_section("Testing Live MJPEG Stream (/api/v1/video/stream/Guardian)")
    try:
        # Connect to stream and read the first 2 multipart chunks
        with requests.get(f"{BASE_URL}/api/v1/video/stream/Guardian", stream=True, timeout=5) as res:
            print(f"Status Code: {res.status_code}")
            content_type = res.headers.get('Content-Type', '')
            print(f"Content-Type: {content_type}")
            
            if res.status_code != 200 or "multipart/x-mixed-replace" not in content_type:
                print(f"[FAIL] Expected multipart/x-mixed-replace content type")
                return False

            chunks_read = 0
            total_bytes = 0
            for chunk in res.iter_content(chunk_size=2048):
                total_bytes += len(chunk)
                chunks_read += 1
                if chunks_read >= 5 or total_bytes > 8192:
                    break
            print(f"Successfully received {chunks_read} stream chunks ({total_bytes} bytes)")
            return True
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_html_video_station():
    print_section("Testing HTML Video Station (/video)")
    try:
        res = requests.get(f"{BASE_URL}/video", timeout=5)
        print(f"Status Code: {res.status_code}")
        print(f"Content-Type: {res.headers.get('Content-Type')}")
        has_brand = "AEGIS" in res.text
        has_feed = "stream-img" in res.text
        print(f"Contains AEGIS Branding: {has_brand}, Contains Stream Feed Element: {has_feed}")
        return res.status_code == 200 and has_brand and has_feed
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def run_all():
    print("=" * 60)
    print("  AEGIS Live Video Streaming Test Suite")
    print("=" * 60)
    print(f"Testing server at: {BASE_URL}")

    results = {
        "Video Status": test_video_status(),
        "Video Config": test_video_config(),
        "Video Snapshot": test_video_snapshot(),
        "Video Frame Ingest": test_video_frame_ingest(),
        "MJPEG Live Stream": test_mjpeg_stream(),
        "HTML Video Station": test_html_video_station(),
    }

    print_section("Test Summary")
    passed = sum(1 for v in results.values() if v)
    total = len(results)

    for name, ok in results.items():
        status = "[PASS]" if ok else "[FAIL]"
        print(f"{status}  {name}")

    print(f"\nResult: {passed}/{total} tests passed")
    return passed == total


if __name__ == "__main__":
    if len(sys.argv) > 1:
        BASE_URL = sys.argv[1]
    ok = run_all()
    sys.exit(0 if ok else 1)
