// Mock data — replace with real WebSocket/HTTP calls to your Python server

export const BOTS = [
  {
    id: 'pathfinder',
    name: 'Pathfinder',
    role: 'Explorer · SLAM',
    status: 'offline',
    battery: 0,
    mapCoverage: null,
    location: { x: 0.18, y: 0.50 },
    color: '#3dd68c',
    icon: 'map-search',
    tasks: ['Offline · Standby'],
  },
  {
    id: 'guardian',
    name: 'Guardian',
    role: 'Rescue · Defence',
    status: 'offline',
    battery: 0,
    mapCoverage: null,
    location: { x: 0.50, y: 0.44 },
    color: '#3dd8ff',
    icon: 'shield-check',
    tasks: ['Offline · Standby'],
  },
  {
    id: 'warden',
    name: 'Warden',
    role: 'Hazard · Fire',
    status: 'offline',
    battery: 0,
    mapCoverage: null,
    location: { x: 0.73, y: 0.50 },
    color: '#f5a524',
    icon: 'flame',
    tasks: ['Offline · Standby'],
  },
];

export const ALERTS = [
  {
    id: 'a1',
    type: 'gas',
    severity: 'warning',
    title: 'Gas sensor reading elevated',
    detail: 'MQ-2 above threshold · Living Room',
    bot: 'Warden',
    time: '2m ago',
    icon: 'alert-triangle',
  },
];

export const INCIDENTS = [
  {
    id: 'i1',
    type: 'gas',
    title: 'Gas leak detected',
    sub: 'MQ-2 · Warden · Living Room',
    time: '2m ago',
    severity: 'warning',
    icon: 'alert-triangle',
  },
  {
    id: 'i2',
    type: 'rescue',
    title: 'First aid delivered',
    sub: 'Guardian · Bedroom 2',
    time: '14m ago',
    severity: 'success',
    icon: 'first-aid-kit',
  },
  {
    id: 'i3',
    type: 'map',
    title: 'Map updated',
    sub: 'Pathfinder · Room 3 added',
    time: '21m ago',
    severity: 'info',
    icon: 'map-2',
  },
  {
    id: 'i4',
    type: 'motion',
    title: 'Motion detected',
    sub: 'Guardian · Hallway',
    time: '35m ago',
    severity: 'info',
    icon: 'eye',
  },
];

export const MAP_ROOMS = [
  { id: 'r1', label: 'Living Room', x: 0.05, y: 0.15, w: 0.35, h: 0.65 },
  { id: 'r2', label: 'Kitchen',     x: 0.42, y: 0.10, w: 0.28, h: 0.40 },
  { id: 'r3', label: 'Bedroom 1',   x: 0.42, y: 0.54, w: 0.28, h: 0.38 },
  { id: 'r4', label: 'Bedroom 2',   x: 0.73, y: 0.15, w: 0.22, h: 0.35 },
  { id: 'r5', label: 'Bathroom',    x: 0.73, y: 0.54, w: 0.22, h: 0.38 },
];

export const VIDEO_FEEDS = [
  { id: 'v1', botId: 'pathfinder', label: 'Pathfinder', streamUrl: null },
  { id: 'v2', botId: 'guardian',   label: 'Guardian',   streamUrl: null },
  { id: 'v3', botId: 'warden',     label: 'Warden',     streamUrl: null },
];
