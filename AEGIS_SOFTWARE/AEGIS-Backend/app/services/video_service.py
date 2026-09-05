import asyncio
import io
import time
from datetime import datetime, timezone
from typing import Any, AsyncGenerator, Dict, Optional

import httpx
from PIL import Image, ImageDraw, ImageFont


class VideoStreamingService:
    """
    AEGIS Video Streaming Service
    High-performance, decoupled producer-consumer video proxying engine.
    - Continuously ingests frames from physical ESP32-S3 cameras in background workers.
    - Caches latest frames in memory for instantaneous, zero-latency client streaming.
    - Generates smooth 20+ FPS futuristic animated HUD when hardware is offline or connecting.
    - Never blocks HTTP response generators with synchronous network handshakes.
    """

    def __init__(self) -> None:
        # Canonical Bot Names & Default IP Mappings
        self.bot_ips: Dict[str, str] = {
            "Guardian": "10.122.202.110",
            "Pathfinder": "10.122.202.176",
            "Warden": "10.122.202.50",
        }
        self._custom_ports: Dict[str, int] = {
            "Warden": 81,
        }
        self._latest_frames: Dict[str, bytes] = {}
        self._latest_frame_time: Dict[str, float] = {}
        self._frame_counters: Dict[str, int] = {}
        self._active_streamers: Dict[str, int] = {}
        
        # Background worker tasks for each bot
        self._worker_tasks: Dict[str, asyncio.Task] = {}
        self._running = False

    def _normalize_bot_id(self, bot_id: str) -> str:
        """Normalize bot ID to canonical title case (Pathfinder, Guardian, Warden)."""
        if not bot_id:
            return "Guardian"
        s = bot_id.strip()
        for canonical in ["Guardian", "Pathfinder", "Warden"]:
            if s.lower() == canonical.lower():
                return canonical
        return s.capitalize()

    def start(self) -> None:
        """Start background frame ingestion workers for all registered bots."""
        if self._running:
            return
        self._running = True
        for bot_id in list(self.bot_ips.keys()):
            self._ensure_worker(bot_id)
        print("[AEGIS Video] Background Video Streaming Service started (on-demand mode).")

    def stop(self) -> None:
        """Stop all background worker tasks."""
        self._running = False
        for bot_id, task in self._worker_tasks.items():
            if not task.done():
                task.cancel()
        self._worker_tasks.clear()
        print("[AEGIS Video] Video Streaming Service stopped.")

    def _ensure_worker(self, bot_id: str) -> None:
        """Ensure a background worker is running for a specific bot."""
        canonical = self._normalize_bot_id(bot_id)
        task = self._worker_tasks.get(canonical)
        if task is None or task.done():
            try:
                loop = asyncio.get_running_loop()
                self._worker_tasks[canonical] = loop.create_task(self._bot_stream_worker(canonical))
            except RuntimeError:
                pass

    def register_bot_ip(self, bot_id: str, ip_address: Optional[str], port: int = 80) -> None:
        """Dynamically record or update bot IP address from incoming telemetry."""
        if not ip_address or ip_address == "0.0.0.0" or ip_address.startswith("127."):
            return
        canonical = self._normalize_bot_id(bot_id)
        old_ip = self.bot_ips.get(canonical)
        self.bot_ips[canonical] = ip_address.strip()
        self._custom_ports[canonical] = port
        print(f"[AEGIS Video] Bot '{canonical}' IP registered: {ip_address}:{port}")
        
        # Restart worker for the updated IP if needed
        if old_ip != ip_address.strip():
            if canonical in self._worker_tasks and not self._worker_tasks[canonical].done():
                self._worker_tasks[canonical].cancel()
            self._ensure_worker(canonical)

    def set_bot_ip(self, bot_id: str, ip_address: str, port: int = 80) -> None:
        """Manually configure or override bot IP address."""
        self.register_bot_ip(bot_id, ip_address, port)

    def ingest_frame(self, bot_id: str, frame_bytes: bytes) -> None:
        """Store an uploaded frame directly from the ESP32."""
        if not frame_bytes or len(frame_bytes) < 32:
            return
        canonical = self._normalize_bot_id(bot_id)
        self._latest_frames[canonical] = frame_bytes
        self._latest_frame_time[canonical] = time.time()
        self._frame_counters[canonical] = self._frame_counters.get(canonical, 0) + 1

    def get_latest_frame(self, bot_id: str) -> Optional[bytes]:
        """Return the most recent raw JPEG frame bytes if available."""
        canonical = self._normalize_bot_id(bot_id)
        return self._latest_frames.get(canonical)

    def get_snapshot(self, bot_id: str) -> bytes:
        """Return the latest frame or a styled standby snapshot."""
        canonical = self._normalize_bot_id(bot_id)
        frame = self._latest_frames.get(canonical)
        if frame and (time.time() - self._latest_frame_time.get(canonical, 0)) < 10.0:
            return frame
        return self.generate_standby_frame(canonical, "STANDBY SNAPSHOT")

    async def _bot_stream_worker(self, bot_id: str) -> None:
        """
        Background worker dedicated to pulling frames from a single ESP32 bot.
        Uses on-demand non-blocking streaming connection: only connects when there are
        active viewers, freeing up the camera for onboard AI (e.g. fire/smoke detection on Warden).
        """
        print(f"[AEGIS Video Worker] Background task started for '{bot_id}' (on-demand mode)")
        while self._running:
            # Only connect when there is at least one active viewer
            if self._active_streamers.get(bot_id, 0) <= 0:
                await asyncio.sleep(0.5)
                continue

            ip = self.bot_ips.get(bot_id)
            port = self._custom_ports.get(bot_id, 81 if bot_id == "Warden" else 80)

            if not ip or ip == "0.0.0.0" or ip == "127.0.0.1":
                await asyncio.sleep(1.0)
                continue

            stream_url = f"http://{ip}:{port}/stream"

            try:
                timeout = httpx.Timeout(connect=3.0, read=8.0, write=5.0, pool=5.0)
                async with httpx.AsyncClient(timeout=timeout) as client:
                    async with client.stream("GET", stream_url) as response:
                        if response.status_code == 200:
                            print(f"[AEGIS Video Worker] Connected to live camera stream at {stream_url} for '{bot_id}'")
                            buffer = bytearray()

                            async for chunk in response.aiter_bytes(chunk_size=8192):
                                # Immediately break out if all viewers disconnected or service stopped
                                if not self._running or self._active_streamers.get(bot_id, 0) <= 0:
                                    print(f"[AEGIS Video Worker] No active viewers for '{bot_id}', disconnecting from {stream_url}")
                                    break
                                buffer.extend(chunk)

                                while True:
                                    soi = buffer.find(b"\xff\xd8")
                                    if soi == -1:
                                        if len(buffer) > 150000:
                                            buffer.clear()
                                        break

                                    eoi = buffer.find(b"\xff\xd9", soi + 2)
                                    if eoi == -1:
                                        break

                                    frame_bytes = bytes(buffer[soi : eoi + 2])
                                    del buffer[: eoi + 2]

                                    if len(frame_bytes) > 200:
                                        self._latest_frames[bot_id] = frame_bytes
                                        self._latest_frame_time[bot_id] = time.time()
                                        self._frame_counters[bot_id] = self._frame_counters.get(bot_id, 0) + 1

            except asyncio.CancelledError:
                break
            except Exception as e:
                # If stream route failed and viewers are still active, try snapshot endpoint fallback
                if self._active_streamers.get(bot_id, 0) > 0:
                    try:
                        snap_url = f"http://{ip}:{port}/capture" if port == 81 else f"http://{ip}:{port}/snapshot"
                        snap_timeout = httpx.Timeout(1.5)
                        async with httpx.AsyncClient(timeout=snap_timeout) as client:
                            resp = await client.get(snap_url)
                            if resp.status_code == 200 and len(resp.content) > 200:
                                self._latest_frames[bot_id] = resp.content
                                self._latest_frame_time[bot_id] = time.time()
                                self._frame_counters[bot_id] = self._frame_counters.get(bot_id, 0) + 1
                    except Exception:
                        pass

            # Backoff before next reconnect attempt
            await asyncio.sleep(0.5)

    def generate_standby_frame(self, bot_id: str, status_msg: str = "SEARCHING FOR ESP32 STREAM...") -> bytes:
        """
        Generate a high-tech cyberpunk HUD standby frame
        when the physical ESP32 camera is not reachable or still booting.
        """
        canonical = self._normalize_bot_id(bot_id)
        width, height = 640, 480
        image = Image.new("RGB", (width, height), color=(8, 12, 20))
        draw = ImageDraw.Draw(image)

        # Draw tech grid pattern
        for x in range(0, width, 40):
            draw.line([(x, 0), (x, height)], fill=(14, 20, 32), width=1)
        for y in range(0, height, 40):
            draw.line([(0, y), (width, y)], fill=(14, 20, 32), width=1)

        # Draw HUD corners
        c_len = 24
        c_color = (0, 210, 255)
        # Top-Left
        draw.line([(16, 16), (16 + c_len, 16)], fill=c_color, width=2)
        draw.line([(16, 16), (16, 16 + c_len)], fill=c_color, width=2)
        # Top-Right
        draw.line([(width - 16, 16), (width - 16 - c_len, 16)], fill=c_color, width=2)
        draw.line([(width - 16, 16), (width - 16, 16 + c_len)], fill=c_color, width=2)
        # Bottom-Left
        draw.line([(16, height - 16), (16 + c_len, height - 16)], fill=c_color, width=2)
        draw.line([(16, height - 16), (16, height - 16 - c_len)], fill=c_color, width=2)
        # Bottom-Right
        draw.line([(width - 16, height - 16), (width - 16 - c_len, height - 16)], fill=c_color, width=2)
        draw.line([(width - 16, height - 16), (width - 16, 16 + c_len)], fill=c_color, width=2)

        # Center reticle / crosshair
        cx, cy = width // 2, height // 2
        r_size = 36
        draw.ellipse([(cx - r_size, cy - r_size), (cx + r_size, cy + r_size)], outline=(0, 160, 220), width=1)
        draw.line([(cx - r_size - 12, cy), (cx - 8, cy)], fill=(0, 210, 255), width=2)
        draw.line([(cx + 8, cy), (cx + r_size + 12, cy)], fill=(0, 210, 255), width=2)
        draw.line([(cx, cy - r_size - 12), (cx, cy - 8)], fill=(0, 210, 255), width=2)
        draw.line([(cx, cy + 8), (cx, cy + r_size + 12)], fill=(0, 210, 255), width=2)

        # Header info
        now_str = datetime.now(timezone.utc).strftime("%H:%M:%S.%f")[:-4] + " UTC"
        draw.text((24, 22), f"AEGIS SURVEILLANCE // {canonical.upper()}", fill=(0, 230, 255))
        draw.text((width - 150, 22), now_str, fill=(140, 170, 200))

        # Target IP info
        ip = self.bot_ips.get(canonical, "NOT DETECTED")
        port = self._custom_ports.get(canonical, 81 if canonical == "Warden" else 80)
        draw.text((24, 44), f"TARGET: {ip}:{port}", fill=(90, 130, 160))

        # Status text in center
        pulse_dot = "." * ((int(time.time() * 2) % 4) + 1)
        status_line = f"{status_msg} {pulse_dot}"
        draw.text((24, cy + 44), status_line, fill=(255, 180, 50))

        # Bottom info bar
        draw.text((24, height - 32), "STANDBY PROXY FEED | 640x480 VGA", fill=(100, 140, 170))
        draw.text((width - 120, height - 32), "MODE: AUTO", fill=(0, 210, 255))

        buf = io.BytesIO()
        image.save(buf, format="JPEG", quality=85)
        return buf.getvalue()

    async def stream_video(self, bot_id: str) -> AsyncGenerator[bytes, None]:
        """
        Stream live MJPEG multipart chunks.
        Instantaneously yields cached hardware frames from memory or animated standby HUD.
        Zero-latency startup (< 50ms) and never hangs.
        """
        canonical = self._normalize_bot_id(bot_id)
        self._ensure_worker(canonical)

        self._active_streamers[canonical] = self._active_streamers.get(canonical, 0) + 1
        print(f"[AEGIS Video] Client connected to live stream for '{canonical}' (viewers: {self._active_streamers[canonical]})")

        try:
            while True:
                recent_frame = self._latest_frames.get(canonical)
                recent_time = self._latest_frame_time.get(canonical, 0)
                now = time.time()

                # If we have a fresh hardware frame (< 4.0s old), stream it
                if recent_frame and (now - recent_time) < 4.0:
                    yield (
                        b"--frame\r\n"
                        b"Content-Type: image/jpeg\r\n"
                        b"Content-Length: " + str(len(recent_frame)).encode() + b"\r\n\r\n"
                        + recent_frame
                        + b"\r\n"
                    )
                    await asyncio.sleep(0.033)  # ~30 FPS
                else:
                    # Stream smooth animated Standby HUD
                    ip = self.bot_ips.get(canonical, "NOT DETECTED")
                    port = self._custom_ports.get(canonical, 81 if canonical == "Warden" else 80)
                    standby = self.generate_standby_frame(canonical, f"CONNECTING TO {ip}:{port}...")
                    yield (
                        b"--frame\r\n"
                        b"Content-Type: image/jpeg\r\n"
                        b"Content-Length: " + str(len(standby)).encode() + b"\r\n\r\n"
                        + standby
                        + b"\r\n"
                    )
                    await asyncio.sleep(0.05)  # ~20 FPS for standby

        except asyncio.CancelledError:
            pass
        finally:
            self._active_streamers[canonical] = max(0, self._active_streamers.get(canonical, 1) - 1)
            print(f"[AEGIS Video] Client disconnected from '{canonical}' stream (remaining viewers: {self._active_streamers[canonical]})")

    async def fetch_bot_detections(self, bot_id: str) -> Optional[Dict[str, Any]]:
        """Directly query the ESP32 bot's HTTP server for active Edge Impulse detections."""
        canonical = self._normalize_bot_id(bot_id)
        ip = self.bot_ips.get(canonical)
        port = self._custom_ports.get(canonical, 81 if canonical == "Warden" else 80)
        if not ip or ip == "0.0.0.0":
            return None

        try:
            timeout = httpx.Timeout(1.5)
            async with httpx.AsyncClient(timeout=timeout) as client:
                res = await client.get(f"http://{ip}:{port}/detections")
                if res.status_code == 200:
                    return res.json()
        except Exception:
            pass
        return None

    async def send_bot_control(self, bot_id: str, cmd: Optional[str] = None, mode: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """Send motor command or mode change directly to the ESP32 bot."""
        canonical = self._normalize_bot_id(bot_id)
        ip = self.bot_ips.get(canonical)
        port = self._custom_ports.get(canonical, 81 if canonical == "Warden" else 80)
        if not ip or ip == "0.0.0.0":
            return None

        params = {}
        if cmd:
            params["cmd"] = cmd
        if mode:
            params["mode"] = mode

        try:
            timeout = httpx.Timeout(2.0)
            async with httpx.AsyncClient(timeout=timeout) as client:
                res = await client.get(f"http://{ip}:{port}/control", params=params)
                if res.status_code == 200:
                    return res.json()
        except Exception as e:
            print(f"[AEGIS Video Error] Failed to send control to {canonical}: {e}")
        return None

    def get_status(self) -> Dict[str, Any]:
        """Return diagnostic status of all video streams."""
        now = time.time()
        bots_info = {}
        for canonical, ip in self.bot_ips.items():
            port = self._custom_ports.get(canonical, 81 if canonical == "Warden" else 80)
            last_t = self._latest_frame_time.get(canonical, 0)
            is_active = (now - last_t) < 5.0 if last_t > 0 else False
            bots_info[canonical] = {
                "ip_address": ip,
                "port": port,
                "stream_url": f"http://{ip}:{port}/stream",
                "backend_stream_url": f"/api/v1/video/stream/{canonical}",
                "backend_snapshot_url": f"/api/v1/video/snapshot/{canonical}",
                "last_frame_received_seconds_ago": round(now - last_t, 1) if last_t > 0 else None,
                "total_frames_processed": self._frame_counters.get(canonical, 0),
                "is_receiving_frames": is_active,
                "active_viewers": self._active_streamers.get(canonical, 0),
            }
        return {
            "status": "online",
            "service": "AEGIS Live Video Streaming Engine",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "bots": bots_info,
        }
