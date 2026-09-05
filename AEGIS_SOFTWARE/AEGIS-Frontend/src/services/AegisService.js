import AsyncStorage from '@react-native-async-storage/async-storage';
import { Platform } from 'react-native';

const STORAGE_SERVER_KEY = 'aegis_backend_server_url';
const DEFAULT_PORT = 8000;

// Resolve default host based on runtime platform.
// Backend runs on the same machine as the app.
function getDefaultHost() {
  // Android emulator maps the host loopback to 10.0.2.2, not localhost.
  if (Platform.OS === 'android') {
    return `http://10.0.2.2:${DEFAULT_PORT}`;
  }
  return `http://localhost:${DEFAULT_PORT}`;
}

let currentServerUrl = getDefaultHost();

// Initialize server URL from storage
AsyncStorage.getItem(STORAGE_SERVER_KEY)
  .then((stored) => {
    if (stored) {
      currentServerUrl = stored.replace(/\/+$/, '');
    }
  })
  .catch(() => {});

/**
 * Get current backend base URL (e.g. "http://localhost:8000")
 */
export function getBackendBaseUrl() {
  return currentServerUrl;
}

/**
 * Update backend base URL and persist
 */
export async function setBackendBaseUrl(url) {
  if (!url) return;
  const cleanUrl = url.trim().replace(/\/+$/, '');
  currentServerUrl = cleanUrl;
  await AsyncStorage.setItem(STORAGE_SERVER_KEY, cleanUrl);
}

/**
 * Get WebSocket URL for telemetry
 */
export function getWebSocketUrl() {
  const base = currentServerUrl.replace(/^http/, 'ws');
  return `${base}/api/v1/ws/telemetry`;
}

// ─── REST Helpers ───────────────────────────────────────────────

/**
 * Fetch bot swarm statuses from backend
 */
export async function fetchBotStatus() {
  const res = await fetch(`${currentServerUrl}/api/v1/bots`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Fetch incident logs from backend
 */
export async function fetchIncidents() {
  const res = await fetch(`${currentServerUrl}/api/v1/incidents`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Fetch the full persistent incident history from the database (unlike
 * fetchIncidents(), which only returns the live in-memory rolling window).
 */
export async function fetchIncidentHistory(filters = {}) {
  const params = new URLSearchParams();
  Object.entries(filters).forEach(([key, value]) => {
    if (value !== undefined && value !== null && value !== '') params.set(key, value);
  });
  const qs = params.toString();
  const res = await fetch(`${currentServerUrl}/api/v1/incidents/history${qs ? `?${qs}` : ''}`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Build the backend URL that returns a downloadable PDF incident report.
 * Optional filters: bot_id, severity, incident_type, start, end, limit.
 */
export function getIncidentsPdfUrl(filters = {}) {
  const params = new URLSearchParams();
  Object.entries(filters).forEach(([key, value]) => {
    if (value !== undefined && value !== null && value !== '') params.set(key, value);
  });
  const qs = params.toString();
  return `${currentServerUrl}/api/v1/incidents/pdf${qs ? `?${qs}` : ''}`;
}

/**
 * Fetch video streaming diagnostic status
 */
export async function fetchVideoStatus() {
  const res = await fetch(`${currentServerUrl}/api/v1/video/status`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Configure or override bot IP address and camera port on the backend
 */
export async function configureVideoFeed(botId, ipAddress, port = 80) {
  const res = await fetch(`${currentServerUrl}/api/v1/video/config`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      bot_id: botId,
      ip_address: ipAddress,
      port: Number(port) || 80,
    }),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Ingest telemetry into the backend (e.g. from ESP32 testing or hardware simulation)
 */
export async function ingestTelemetry(botId, kind, payload) {
  const res = await fetch(`${currentServerUrl}/api/v1/telemetry/ingest`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      bot_id: botId,
      kind,
      payload,
    }),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Trigger manual emergency dispatch
 */
export async function triggerEmergency(type = 'sos') {
  const res = await fetch(`${currentServerUrl}/api/v1/emergency/manual-call`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ type }),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Cycle to the next test scenario on backend
 */
export async function cycleTestScenario() {
  const res = await fetch(`${currentServerUrl}/api/v1/test/next-scenario`, {
    method: 'POST',
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

/**
 * Get current scenario metadata
 */
export async function fetchCurrentScenario() {
  const res = await fetch(`${currentServerUrl}/api/v1/test/current-scenario`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

// ─── WebSocket for Live Telemetry ───────────────────────────────

/**
 * Connect to AEGIS swarm telemetry WebSocket
 */
export function createSwarmSocket(onMessage, onOpen, onError, onClose) {
  const wsUrl = getWebSocketUrl();
  const ws = new WebSocket(wsUrl);

  ws.onopen = () => {
    console.log('[AEGIS] Swarm telemetry socket connected to', wsUrl);
    if (onOpen) onOpen();
  };

  ws.onmessage = (e) => {
    try {
      const data = JSON.parse(e.data);
      if (onMessage) onMessage(data);
    } catch (err) {
      console.warn('[AEGIS] Socket JSON parse error', err);
    }
  };

  ws.onerror = (e) => {
    console.warn('[AEGIS] Socket error', e.message || e);
    if (onError) onError(e);
  };

  ws.onclose = (e) => {
    console.log('[AEGIS] Socket closed', e.code);
    if (onClose) onClose(e);
  };

  return ws;
}

// ─── Video Stream & Snapshot URL Builders ────────────────────────

/**
 * Returns the backend proxied MJPEG live stream URL for a given bot
 * e.g. http://localhost:8000/api/v1/video/stream/Guardian
 */
export function getVideoStreamUrl(botId = 'Guardian') {
  const normalizedId = botId.charAt(0).toUpperCase() + botId.slice(1).toLowerCase();
  return `${currentServerUrl}/api/v1/video/stream/${normalizedId}`;
}

/**
 * Returns the backend JPEG snapshot URL for a given bot
 * e.g. http://localhost:8000/api/v1/video/snapshot/Guardian
 */
export function getVideoSnapshotUrl(botId = 'Guardian') {
  const normalizedId = botId.charAt(0).toUpperCase() + botId.slice(1).toLowerCase();
  return `${currentServerUrl}/api/v1/video/snapshot/${normalizedId}`;
}

/**
 * Returns direct HTML surveillance station viewer URL
 */
export function getVideoViewerUrl(botId = 'Guardian') {
  const normalizedId = botId.charAt(0).toUpperCase() + botId.slice(1).toLowerCase();
  return `${currentServerUrl}/api/v1/video/view/${normalizedId}`;
}

