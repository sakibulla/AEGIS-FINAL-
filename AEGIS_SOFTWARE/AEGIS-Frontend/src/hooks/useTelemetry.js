import { useEffect, useRef, useState, useCallback } from 'react';
import {
  BOTS as initialBots,
  ALERTS as initialAlerts,
  INCIDENTS as initialIncidents,
} from '../constants/mockData';
import {
  getBackendBaseUrl,
  setBackendBaseUrl,
  createSwarmSocket,
  fetchCurrentScenario,
  cycleTestScenario,
  triggerEmergency,
  fetchBotStatus,
  fetchIncidents,
} from '../services/AegisService';

const RECONNECT_MS = 3000;

const BOT_META = {
  pathfinder: {
    id: 'pathfinder',
    name: 'Pathfinder',
    role: 'Explorer · SLAM',
    color: '#3dd68c',
    icon: 'map-search',
    location: { x: 0.18, y: 0.50 },
  },
  guardian: {
    id: 'guardian',
    name: 'Guardian',
    role: 'Rescue · Defence · AI Vision',
    color: '#3dd8ff',
    icon: 'shield-check',
    location: { x: 0.50, y: 0.44 },
  },
  warden: {
    id: 'warden',
    name: 'Warden',
    role: 'Hazard · Fire · Gas',
    color: '#f5a524',
    icon: 'flame',
    location: { x: 0.73, y: 0.50 },
  },
};

const STATUS_MAP = {
  PATROL: 'online',
  MAPPING: 'online',
  RESCUE: 'alert',
  ALERT: 'alert',
  CLEAR: 'online',  // Added CLEAR for Warden's normal state
  OFFLINE: 'offline',
  online: 'online',
  alert: 'alert',
  offline: 'offline',
};

const SEVERITY_MAP = {
  CRITICAL: 'warning',
  HIGH: 'warning',
  MEDIUM: 'info',
  LOW: 'info',
  warning: 'warning',
  info: 'info',
  success: 'success',
};

const ICON_MAP = {
  GAS_LEAK: 'alert-triangle',
  FIRE: 'flame',
  MEDICAL_HELP: 'first-aid-kit',
  INTRUDER: 'eye',
  OBSTACLE: 'construct',
  EXIT_DOOR_FOUND: 'exit',
  DOOR_DETECTED: 'exit',
  gas: 'alert-triangle',
  rescue: 'first-aid-kit',
  map: 'map-2',
  motion: 'eye',
};

const formatTime = (timestamp) => {
  if (!timestamp) return 'now';
  const date = new Date(timestamp);
  if (Number.isNaN(date.getTime())) return timestamp;
  const diffSeconds = Math.floor((Date.now() - date.getTime()) / 1000);
  if (diffSeconds < 60) return 'Just now';
  if (diffSeconds < 3600) return `${Math.floor(diffSeconds / 60)}m ago`;
  return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
};

function normalizeBotId(raw) {
  if (!raw) return 'guardian';
  const str = String(raw).toLowerCase().trim();
  if (str.includes('path')) return 'pathfinder';
  if (str.includes('guard') || str.includes('gurd')) return 'guardian';
  if (str.includes('ward')) return 'warden';
  return str;
}

function mapBackendBot(bot) {
  const rawId = normalizeBotId(bot.bot_id || bot.id || '');
  const meta = BOT_META[rawId] || {
    id: rawId || 'unknown',
    name: bot.bot_id || bot.name || 'Unknown Bot',
    role: 'Swarm Node',
    color: '#3dd8ff',
    icon: 'hardware-chip',
    location: { x: 0.5, y: 0.5 },
  };

  const mapPacket = bot.map_packet || bot.mapPacket || null;
  const visionDetections = bot.vision_detections || bot.visionDetections || [];
  const hazardData = bot.hazard_data || bot.hazardData || null;
  const systemInfo = bot.system_info || bot.systemInfo || {};
  const firstAid = bot.first_aid_status || bot.firstAidStatus || {};
  const detectionState = bot.detection_state || bot.detectionState || null;

  const wakeWordTriggered = Boolean(bot.wake_word_triggered || bot.wake_word_active || bot.wakeWordTriggered);
  const wakeWordLabel = bot.wake_word_label || bot.wakeWordLabel || (wakeWordTriggered ? 'Hi ESP' : '');
  const firstAidDelivered = Boolean(firstAid.delivered || bot.first_aid_delivered || bot.firstAidDelivered);

  const isOnline = (STATUS_MAP[bot.status] || (bot.status ? bot.status.toLowerCase() : 'offline')) !== 'offline';
  const effectiveStatus = isOnline ? (STATUS_MAP[bot.status] || bot.status.toLowerCase()) : 'offline';

  // Derive dynamic task descriptions from live hardware telemetry
  const tasks = [];
  if (!isOnline) {
    tasks.push('Offline · Unit in standby');
  } else if (rawId === 'warden') {
    if (visionDetections.length > 0) {
      const topDet = visionDetections[visionDetections.length - 1];
      tasks.push(`AI Vision: ${topDet.label} (${(topDet.confidence || 0).toFixed(0)}%)`);
    } else {
      tasks.push('Watching for Fire / Smoke');
    }
    tasks.push('ESP-NOW mesh synchronized');
  } else if (rawId === 'pathfinder') {
    if (mapPacket) {
      tasks.push(`Mapping Snap ${mapPacket.snap_index || 0}/${mapPacket.total_snaps || 10}`);
      if (mapPacket.has_door) tasks.push('Door aperture identified');
    } else {
      tasks.push('Broadcasting SLAM occupancy grid');
    }
  } else if (rawId === 'guardian') {
    if (wakeWordTriggered) {
      tasks.push(`Wake-word: "${wakeWordLabel}"`);
    } else {
      tasks.push('Listening for wake-word ("Hi ESP")');
    }
    if (visionDetections.length > 0) {
      const topDet = visionDetections[visionDetections.length - 1];
      tasks.push(`Vision: ${topDet.label} (${(topDet.confidence || 0).toFixed(0)}%)`);
    } else {
      tasks.push('AI Vision: FOMO / Threat active');
    }
    if (firstAidDelivered) {
      tasks.push('First-aid box delivered');
    }
  }

  return {
    id: meta.id,
    name: meta.name,
    role: meta.role,
    status: effectiveStatus,
    battery: isOnline ? (bot.battery ?? bot.battery_pct ?? 95) : 0,
    freeHeap: isOnline ? (bot.freeHeap ?? systemInfo.free_heap ?? bot.free_heap ?? 120000) : 0,
    psram: isOnline ? (bot.psram ?? systemInfo.psram ?? bot.psram ?? 256000) : 0,
    wifiRssi: isOnline ? (bot.wifiRssi ?? bot.wifi_rssi ?? -58) : -128,
    ipAddress: isOnline ? (bot.ipAddress ?? bot.ip_address ?? '10.75.11.50') : '0.0.0.0',
    timestamp: bot.timestamp || new Date().toISOString(),
    mapCoverage: isOnline && mapPacket && typeof mapPacket.snap_index === 'number' && typeof mapPacket.total_snaps === 'number'
      ? Math.round((mapPacket.snap_index / Math.max(1, mapPacket.total_snaps)) * 100)
      : (isOnline && rawId === 'pathfinder' ? (bot.mapCoverage ?? 0) : null),
    mapPacket: isOnline ? mapPacket : null,
    visionDetections: isOnline ? visionDetections : [],
    hazardData: isOnline ? hazardData : null,
    detectionState: isOnline ? detectionState : null,
    wakeWordTriggered: isOnline && wakeWordTriggered,
    wakeWordLabel: isOnline ? wakeWordLabel : '',
    firstAidDelivered: isOnline && firstAidDelivered,
    firstAidStatus: isOnline ? firstAid : { box_attached: false, delivered: false },
    location: {
      x: typeof mapPacket?.x_coord === 'number' ? Math.min(1, Math.max(0, mapPacket.x_coord)) : meta.location.x,
      y: typeof mapPacket?.y_coord === 'number' ? Math.min(1, Math.max(0, mapPacket.y_coord)) : meta.location.y,
    },
    color: meta.color,
    icon: meta.icon,
    tasks: tasks.length > 0 ? tasks : ['Offline · Unit in standby'],
  };
}

function mapBackendIncident(incident) {
  const id = incident.id ?? `INC-${Math.random().toString(16).slice(2, 8).toUpperCase()}`;
  return {
    id,
    type: (incident.type || 'incident').toLowerCase(),
    title: incident.title || 'Security / Swarm Event',
    sub: `${incident.bot_id || 'Guardian'} · ${incident.message || incident.detail || 'Event reported'}`,
    time: formatTime(incident.timestamp),
    severity: SEVERITY_MAP[incident.severity] || 'info',
    icon: ICON_MAP[incident.type] || 'information-circle',
  };
}

function mapBackendAlert(incident) {
  return {
    id: `alert-${incident.id}`,
    type: (incident.type || 'alert').toLowerCase(),
    severity: incident.severity === 'CRITICAL' || incident.severity === 'HIGH' || incident.severity === 'warning' ? 'warning' : 'info',
    title: incident.title || 'Alert Raised',
    detail: incident.message || incident.detail || 'Live hardware event detected',
    bot: incident.bot_id || 'Guardian',
    time: formatTime(incident.timestamp),
    icon: ICON_MAP[incident.type] || 'alert-triangle',
  };
}

export function useTelemetry() {
  const [bots, setBots] = useState(() => 
    initialBots.map(bot => {
      const mapped = mapBackendBot(bot);
      return {
        ...mapped,
        status: 'offline',
        battery: 0,
        freeHeap: 0,
        psram: 0,
        wifiRssi: -128,
        ipAddress: '0.0.0.0',
        tasks: ['Waiting for connection...'],
      };
    })
  );
  const [alerts, setAlerts] = useState([]);
  const [incidents, setIncidents] = useState([]);
  const [notifications, setNotifications] = useState([]);
  const [unreadCount, setUnreadCount] = useState(0);
  const [emergencyStatus, setEmergencyStatus] = useState(null);
  const [connected, setConnected] = useState(false);
  const [error, setError] = useState(null);
  const [scenarioIndex, setScenarioIndex] = useState(1);
  const [scenarioCount, setScenarioCount] = useState(4);
  const [scenarioName, setScenarioName] = useState('Normal Operation');
  const [serverUrl, setServerUrlState] = useState(() => {
    try {
      return getBackendBaseUrl();
    } catch (err) {
      console.warn('[useTelemetry] Failed to get backend URL:', err);
      return 'http://localhost:8000';
    }
  });
  const [lastHeartbeat, setLastHeartbeat] = useState(null);
  const [activeBots, setActiveBots] = useState(new Set()); // Track which bots are actually connected

  const socketRef = useRef(null);
  const retryRef = useRef(null);
  const mountedRef = useRef(true);
  const botLastSeenRef = useRef(new Map()); // Track last telemetry time for each bot

  // Handle incoming live payload from backend WebSocket
  const handlePayload = useCallback((payload) => {
    if (!payload || typeof payload !== 'object') return;

    setLastHeartbeat(Date.now());

    if (typeof payload.scenario_index === 'number') {
      setScenarioIndex(payload.scenario_index);
    }
    if (typeof payload.scenario_count === 'number') {
      setScenarioCount(payload.scenario_count);
    }
    if (typeof payload.scenario_name === 'string') {
      setScenarioName(payload.scenario_name);
    }

    // 1. Full Swarm Telemetry Frame
    if (payload.type === 'TELEMETRY_FRAME') {
      if (Array.isArray(payload.bots)) {
        setBots(payload.bots.map(mapBackendBot));
      }

      if (Array.isArray(payload.incidents) && payload.incidents.length > 0) {
        const mappedIncidents = payload.incidents.map(mapBackendIncident).slice(0, 20);
        setIncidents((curr) => {
          const ids = new Set(curr.map((i) => i.id));
          const newOnes = mappedIncidents.filter((i) => !ids.has(i.id));
          return [...newOnes, ...curr].slice(0, 25);
        });
        const newAlerts = mappedIncidents
          .filter((i) => i.severity === 'warning' || i.severity === 'critical')
          .map(mapBackendAlert);
        if (newAlerts.length > 0) {
          setAlerts((curr) => [...newAlerts, ...curr].slice(0, 10));
        }
      }

      if (payload.automatic_emergency_called || payload.emergency_type) {
        setEmergencyStatus({
          triggered: true,
          mode: 'AUTOMATIC',
          emergency_type: payload.emergency_type || 'POLICE_AND_AMBULANCE',
          services_notified: ['POLICE', 'AMBULANCE', 'SECURITY'],
          timestamp: new Date().toISOString(),
        });
      }
      return;
    }

    // 2. Hardware ESP32 Ingested Telemetry (Real-time Guardian updates!)
    if (payload.type === 'INGESTED_TELEMETRY') {
      const { bot_id, kind, payload: data } = payload;
      const normId = normalizeBotId(bot_id || 'Guardian');

      console.log(`[AEGIS] Received telemetry: ${bot_id} (${kind})`, data);

      // Mark this bot as active (connected)
      setActiveBots((current) => {
        const updated = new Set(current);
        updated.add(normId);
        console.log(`[AEGIS] Active bots:`, Array.from(updated));
        return updated;
      });

      // Update last seen timestamp
      botLastSeenRef.current.set(normId, Date.now());

      setBots((current) =>
        current.map((bot) => {
          if (bot.id !== normId) return bot;

          const updated = { ...bot };

          if (kind === 'telemetry' || kind === 'status') {
            // Mark bot as online when we receive telemetry
            updated.status = 'online';
            
            if (data.status) updated.status = STATUS_MAP[data.status] || data.status.toLowerCase();
            if (typeof data.battery_pct === 'number') updated.battery = data.battery_pct;
            else updated.battery = 95; // Default if not provided
            
            if (data.system_info) {
              if (data.system_info.free_heap) updated.freeHeap = data.system_info.free_heap;
              if (data.system_info.free_psram || data.system_info.psram) updated.psram = data.system_info.free_psram || data.system_info.psram;
            }
            if (typeof data.wifi_rssi === 'number') updated.wifiRssi = data.wifi_rssi;
            if (data.ip_address) updated.ipAddress = data.ip_address;
            
            // GUARDIAN NOTIFICATIONS
            if (typeof data.wake_word_triggered === 'boolean') {
              const wasTriggered = bot.wakeWordTriggered;
              updated.wakeWordTriggered = data.wake_word_triggered;
              updated.wakeWordLabel = data.wake_word_label || (data.wake_word_triggered ? 'Hi ESP' : '');
              
              // Notify when wake word is newly detected
              if (data.wake_word_triggered && !wasTriggered) {
                const wakeWordNotif = {
                  id: `wake-word-${Date.now()}`,
                  title: `🎤 Wake Word Detected: "${data.wake_word_label || 'Hi ESP'}"`,
                  detail: `${bot_id} is now listening for voice commands`,
                  time: new Date().toISOString(),
                  severity: 'info',
                  icon: 'mic',
                  type: 'wake_word',
                };
                setNotifications((curr) => [wakeWordNotif, ...curr].slice(0, 30));
                setUnreadCount((c) => c + 1);
              }
            }
            
            if (data.first_aid_status) {
              const wasDelivered = bot.firstAidDelivered;
              updated.firstAidStatus = data.first_aid_status;
              updated.firstAidDelivered = Boolean(data.first_aid_status.delivered);
              
              // Notify when first aid is newly delivered
              if (data.first_aid_status.delivered && !wasDelivered) {
                const firstAidNotif = {
                  id: `first-aid-${Date.now()}`,
                  title: `🏥 First Aid Box Delivered`,
                  detail: `${bot_id} successfully delivered medical supplies`,
                  time: new Date().toISOString(),
                  severity: 'success',
                  icon: 'medkit',
                  type: 'first_aid',
                };
                setNotifications((curr) => [firstAidNotif, ...curr].slice(0, 30));
                setUnreadCount((c) => c + 1);
              }
            }
            
            // WARDEN NOTIFICATIONS
            if (data.detection_state) {
              updated.detectionState = data.detection_state;
              
              // Fire detected
              if (data.detection_state.fire_streak >= 3) {
                const lastFireNotif = bot.detectionState?.fire_streak >= 3;
                if (!lastFireNotif) {
                  const fireNotif = {
                    id: `fire-${Date.now()}`,
                    title: `🔥 FIRE DETECTED!`,
                    detail: `${bot_id} detected fire with ${data.detection_state.fire_streak} consecutive detections`,
                    time: new Date().toISOString(),
                    severity: 'warning',
                    icon: 'flame',
                    type: 'fire',
                  };
                  setNotifications((curr) => [fireNotif, ...curr].slice(0, 30));
                  setUnreadCount((c) => c + 1);
                  updated.status = 'alert';
                }
              }
              
              // Smoke detected
              if (data.detection_state.smoke_streak >= 3) {
                const lastSmokeNotif = bot.detectionState?.smoke_streak >= 3;
                if (!lastSmokeNotif) {
                  const smokeNotif = {
                    id: `smoke-${Date.now()}`,
                    title: `💨 SMOKE DETECTED!`,
                    detail: `${bot_id} detected smoke with ${data.detection_state.smoke_streak} consecutive detections`,
                    time: new Date().toISOString(),
                    severity: 'warning',
                    icon: 'cloud',
                    type: 'smoke',
                  };
                  setNotifications((curr) => [smokeNotif, ...curr].slice(0, 30));
                  setUnreadCount((c) => c + 1);
                  updated.status = 'alert';
                }
              }
              
              // Hazard cleared
              if (data.detection_state.clear_streak >= 10) {
                const wasInAlert = bot.status === 'alert' || (bot.detectionState?.fire_streak >= 3 || bot.detectionState?.smoke_streak >= 3);
                if (wasInAlert) {
                  const clearNotif = {
                    id: `clear-${Date.now()}`,
                    title: `✅ Hazard Cleared`,
                    detail: `${bot_id} reports area is clear - no fire or smoke detected`,
                    time: new Date().toISOString(),
                    severity: 'success',
                    icon: 'checkmark-circle',
                    type: 'clear',
                  };
                  setNotifications((curr) => [clearNotif, ...curr].slice(0, 30));
                  setUnreadCount((c) => c + 1);
                }
              }
            }
            
            // High gas level alert
            if (data.hazard_data && data.hazard_data.gas_ppm > 300) {
              const lastGasLevel = bot.hazardData?.gas_ppm || 0;
              if (lastGasLevel <= 300) {
                const gasNotif = {
                  id: `gas-${Date.now()}`,
                  title: `⚠️ High Gas Level Detected`,
                  detail: `${bot_id} detected ${data.hazard_data.gas_ppm} PPM (threshold: 300 PPM)`,
                  time: new Date().toISOString(),
                  severity: 'warning',
                  icon: 'warning',
                  type: 'gas',
                };
                setNotifications((curr) => [gasNotif, ...curr].slice(0, 30));
                setUnreadCount((c) => c + 1);
              }
            }
            
            // Low battery warning
            if (updated.battery < 20 && bot.battery >= 20) {
              const batteryNotif = {
                id: `battery-${normId}-${Date.now()}`,
                title: `🔋 Low Battery Warning`,
                detail: `${bot_id} battery at ${updated.battery}% - return to charging station`,
                time: new Date().toISOString(),
                severity: 'warning',
                icon: 'battery-dead',
                type: 'battery',
              };
              setNotifications((curr) => [batteryNotif, ...curr].slice(0, 30));
              setUnreadCount((c) => c + 1);
            }
            
            console.log(`[AEGIS] Updated bot ${bot.id}:`, { status: updated.status, ip: updated.ipAddress, battery: updated.battery });
          } else if (kind === 'map_packet') {
            updated.mapPacket = data;
            updated.status = 'online';
            if (typeof data.snap_index === 'number' && typeof data.total_snaps === 'number') {
              updated.mapCoverage = Math.min(100, Math.round((data.snap_index / data.total_snaps) * 100));
            }
            if (typeof data.x_coord === 'number' && typeof data.y_coord === 'number') {
              updated.location = {
                x: Math.max(0.05, Math.min(0.95, data.x_coord)),
                y: Math.max(0.05, Math.min(0.95, data.y_coord)),
              };
            }
            if (data.ip_address) updated.ipAddress = data.ip_address;
          } else if (kind === 'vision_detection') {
            const currentVision = Array.isArray(updated.visionDetections) ? [...updated.visionDetections] : [];
            currentVision.push(data);
            updated.visionDetections = currentVision.slice(-5);

            if (data.ip_address) updated.ipAddress = data.ip_address;

            if (data.is_threat) {
              updated.status = 'alert';
              const threatNotif = {
                id: `threat-${Date.now()}`,
                title: `🚨 THREAT DETECTED: ${data.label || 'Intruder'}`,
                detail: `${bot_id} vision AI detected threat with ${(data.confidence || 90).toFixed(0)}% confidence`,
                time: new Date().toISOString(),
                severity: 'warning',
                icon: 'eye',
                type: 'threat',
              };
              setNotifications((curr) => [threatNotif, ...curr].slice(0, 30));
              setUnreadCount((c) => c + 1);
            } else if (updated.status === 'offline') {
              updated.status = 'online';
            }
          }

          return mapBackendBot(updated);
        })
      );
      return;
    }

    // 3. Emergency Status Event
    if (payload.type === 'EMERGENCY_STATUS') {
      const notif = {
        id: `emergency-status-${payload.timestamp || Date.now()}`,
        title: `${payload.mode === 'AUTOMATIC' ? 'Automatic' : 'Manual'} Emergency Dispatch`,
        detail: `Services notified: ${payload.services_notified?.join(', ') || 'POLICE, FIRE, AMBULANCE'}`,
        time: payload.timestamp || new Date().toISOString(),
        severity: 'warning',
        icon: 'warning',
        type: 'emergency',
      };
      setNotifications((current) => [notif, ...current].slice(0, 30));
      setUnreadCount((count) => count + 1);
      setEmergencyStatus(payload);
      return;
    }

    // 4. Single Alert / Incident Broadcast
    if (payload.type === 'ALERT' || payload.incident) {
      const inc = payload.incident || payload;
      const mapped = mapBackendIncident(inc);
      setIncidents((curr) => [mapped, ...curr].slice(0, 25));
      setAlerts((curr) => [mapBackendAlert(inc), ...curr].slice(0, 10));
      setUnreadCount((c) => c + 1);
      return;
    }

    // 5. Bot Update Array
    if (Array.isArray(payload.bots)) {
      setBots(payload.bots.map(mapBackendBot));
    }
  }, []);

  // Fetch initial REST data from backend
  const loadInitialData = useCallback(async () => {
    try {
      const scenario = await fetchCurrentScenario();
      setScenarioIndex(scenario.scenario_index || 1);
      setScenarioCount(scenario.scenario_count || 4);
      setScenarioName(scenario.scenario_name || 'Normal Patrol');
    } catch (e) {
      console.warn('[AEGIS] Failed to fetch scenario REST', e.message);
    }

    try {
      const botList = await fetchBotStatus();
      if (Array.isArray(botList) && botList.length > 0) {
        const activeIds = new Set();
        const mappedBots = botList.map(bot => {
          const normId = normalizeBotId(bot.bot_id || bot.id || '');
          const isOnline = (STATUS_MAP[bot.status] || (bot.status ? bot.status.toLowerCase() : 'offline')) !== 'offline';
          if (isOnline) {
            activeIds.add(normId);
          }
          return mapBackendBot(bot);
        });
        
        // Update active bots set
        setActiveBots(activeIds);
        
        // Merge with existing bot metadata, keeping offline bots that aren't in the response
        setBots(current => {
          const botMap = new Map(mappedBots.map(b => [b.id, b]));
          return current.map(existingBot => {
            if (botMap.has(existingBot.id)) {
              return botMap.get(existingBot.id);
            }
            // Keep existing bot but mark as offline if not in active list
            return {
              ...existingBot,
              status: 'offline',
              battery: 0,
              tasks: ['Offline · Waiting for connection...'],
            };
          });
        });
      }
    } catch (e) {
      console.warn('[AEGIS] Failed to fetch bot status REST', e.message);
    }

    try {
      const incList = await fetchIncidents();
      if (Array.isArray(incList) && incList.length > 0) {
        setIncidents(incList.map(mapBackendIncident));
      }
    } catch (e) {
      console.warn('[AEGIS] Failed to fetch incidents REST', e.message);
    }
  }, []);

  // Connect WebSocket
  const connectSocket = useCallback(() => {
    if (!mountedRef.current) return;

    try {
      if (socketRef.current) {
        socketRef.current.close();
      }

      socketRef.current = createSwarmSocket(
        (data) => {
          if (!mountedRef.current) return;
          console.log('[AEGIS] WebSocket message received:', data.type, data.bot_id || 'system');
          handlePayload(data);
        },
        () => {
          if (!mountedRef.current) return;
          console.log('[AEGIS] WebSocket connected successfully');
          setConnected(true);
          setError(null);
        },
        (err) => {
          if (!mountedRef.current) return;
          console.error('[AEGIS] WebSocket error:', err);
          setError(err?.message || 'WebSocket communication error');
        },
        () => {
          if (!mountedRef.current) return;
          console.log('[AEGIS] WebSocket disconnected, will retry...');
          setConnected(false);
          if (retryRef.current) clearTimeout(retryRef.current);
          retryRef.current = setTimeout(connectSocket, RECONNECT_MS);
        }
      );
    } catch (err) {
      console.warn('[AEGIS] Socket creation error', err);
      if (retryRef.current) clearTimeout(retryRef.current);
      retryRef.current = setTimeout(connectSocket, RECONNECT_MS);
    }
  }, [handlePayload]);

  useEffect(() => {
    mountedRef.current = true;
    loadInitialData();
    connectSocket();

    // Bot timeout checker - mark bots as offline if no telemetry for 10 seconds
    const timeoutChecker = setInterval(() => {
      const now = Date.now();
      const TIMEOUT_MS = 10000; // 10 seconds

      setBots((current) =>
        current.map((bot) => {
          const lastSeen = botLastSeenRef.current.get(bot.id);
          if (lastSeen && now - lastSeen > TIMEOUT_MS && bot.status !== 'offline') {
            console.warn(`[AEGIS] Bot ${bot.id} timed out, marking offline`);
            return {
              ...bot,
              status: 'offline',
              battery: 0,
              wifiRssi: -128,
              tasks: ['Connection lost · Timeout'],
            };
          }
          return bot;
        })
      );

      // Remove timed-out bots from activeBots set
      setActiveBots((current) => {
        const updated = new Set(current);
        for (const botId of current) {
          const lastSeen = botLastSeenRef.current.get(botId);
          if (lastSeen && now - lastSeen > TIMEOUT_MS) {
            updated.delete(botId);
          }
        }
        return updated;
      });
    }, 5000); // Check every 5 seconds

    return () => {
      mountedRef.current = false;
      if (retryRef.current) clearTimeout(retryRef.current);
      if (socketRef.current) socketRef.current.close();
      clearInterval(timeoutChecker);
    };
  }, [loadInitialData, connectSocket]);

  // Actions
  const cycleScenario = async () => {
    try {
      const data = await cycleTestScenario();
      setScenarioIndex(data.scenario_index || 1);
      setScenarioCount(data.scenario_count || 4);
      setScenarioName(data.scenario_name || 'Swarm Operation');
    } catch (err) {
      console.warn('[AEGIS] Failed to cycle scenario', err);
    }
  };

  const triggerManualEmergency = async (type = 'sos') => {
    try {
      await triggerEmergency(type);
    } catch (err) {
      console.warn('[AEGIS] Failed to trigger emergency', err);
      throw err;
    }
  };

  const updateServerUrl = async (newUrl) => {
    await setBackendBaseUrl(newUrl);
    setServerUrlState(newUrl);
    loadInitialData();
    connectSocket();
  };

  const markNotificationsRead = () => setUnreadCount(0);
  const clearNotifications = () => {
    setNotifications([]);
    setUnreadCount(0);
  };

  return {
    bots,
    alerts,
    incidents,
    notifications,
    unreadCount,
    emergencyStatus,
    connected,
    error,
    scenarioIndex,
    scenarioCount,
    scenarioName,
    serverUrl,
    lastHeartbeat,
    cycleScenario,
    triggerManualEmergency,
    updateServerUrl,
    markNotificationsRead,
    clearNotifications,
    refreshData: loadInitialData,
  };
}

