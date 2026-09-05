# A.E.G.I.S. Mobile App — Comprehensive Project Prompt

Use this prompt to discuss the project with another AI assistant.

---

## Project Overview

**A.E.G.I.S.** (Autonomous Emergency Ground Intelligence Swarm) is a low-cost, COTS-based swarm of ground robots designed to respond to indoor emergencies. The mobile app serves as the **live command dashboard** for real-time monitoring and control of the robot swarm.

### Problem Statement
Most homes and buildings only have passive safety tools (fixed cameras, alarms). They can't provide context about emergencies or take physical action. AEGIS fills this gap by deploying autonomous robots that:
- Map the space (Pathfinder)
- Check on people (Guardian)
- Detect hazards like fire and gas (Warden)

### Three-Robot Swarm Architecture

| Robot | Role | Capabilities |
|-------|------|--------------|
| 🟦 **Pathfinder** | Explorer / SLAM | Maps spaces, shares occupancy grids, autonomous navigation |
| 🟧 **Guardian** | Rescue / Defence | Listens for help calls, delivers first aid, motion detection |
| 🟥 **Warden** | Hazard Control | Detects fire/gas, thermal scanning, hazard reporting |

---

## Current Tech Stack

### Frontend (React Native / Expo)
- **Framework**: React Native with Expo CLI
- **Navigation**: React Navigation (bottom tabs + stack navigation)
- **UI Components**: Expo Vector Icons (Ionicons), custom SwarmUI components
- **Styling**: React Native StyleSheet with theme system
- **State Management**: React Context (ThemeContext)
- **Storage**: AsyncStorage for persistent data (stream URLs, settings)
- **WebView**: react-native-webview (already installed, not yet used)

### Current Dependencies
```json
{
  "expo": "~52.0.0",
  "react": "18.3.1",
  "react-native": "0.76.5",
  "@react-navigation/native": "^6.1.18",
  "@react-navigation/bottom-tabs": "^6.6.1",
  "@react-navigation/stack": "^6.4.1",
  "react-native-webview": "^13.8.0",
  "react-native-maps": "1.18.0",
  "@react-native-async-storage/async-storage": "^1.21.0"
}
```

### Backend (Assumed)
- Python Flask/FastAPI server (referenced in AegisService)
- REST API endpoints for bot status, commands, incidents, map data
- WebSocket for live telemetry updates
- Video streaming (MJPEG format)

---

## App Architecture & File Structure

```
aegis-app/
├── src/
│   ├── screens/
│   │   ├── DashboardScreen.js      # Main dashboard (bots, alerts, incidents)
│   │   ├── MapScreen.js             # Map visualization
│   │   ├── FeedsScreen.js           # Video feed management
│   │   └── ControlScreen.js         # Robot manual control
│   ├── components/
│   │   └── SwarmUI.js               # Reusable UI components (BotCard, StatusPip, etc.)
│   ├── navigation/
│   │   └── AppNavigator.js          # Bottom tab navigator setup
│   ├── services/
│   │   └── AegisService.js          # REST + WebSocket API client
│   ├── hooks/
│   │   └── useStreamUrls.js         # Custom hook for video stream management
│   ├── context/
│   │   └── ThemeContext.js          # Dark/light theme toggle
│   ├── constants/
│   │   ├── theme.js                 # Design system (colors, typography, spacing)
│   │   └── mockData.js              # Sample bot, alert, incident data
│   └── navigation/
│       └── AppNavigator.js          # App navigation structure
├── android/                          # Android native code
├── app.json                          # Expo configuration
├── App.js                            # Root component
├── package.json                      # Dependencies
└── README.md                         # Project overview
```

---

## Design System (AEGIS Theme)

### Color Palette

**Dark Mode (Primary)**:
- **Backgrounds**: `#060d18` (bg0), `#0a1525` (bg1), `#0f1e34` (bg2), `#162844` (bg3)
- **Brand Cyan**: `#3dd8ff` (Guardian color)
- **Status Green**: `#3dd68c` (Pathfinder online)
- **Status Amber**: `#f5a524` (Warden alert)
- **Status Red**: `#f87171` (Emergency)
- **Text**: `#e8f2ff` (primary), `#6a8aaa` (secondary), `#3a5070` (muted)

**Light Mode** also available with inverted palette.

### Typography
- **Font Sizes**: xs (10), sm (12), base (14), md (16), lg (18), xl (22), xxl (28)
- **Weights**: regular (400), medium (500), bold (600)

### Spacing
- xs (4), sm (8), md (12), lg (16), xl (20), xxl (28)

### Border Radius
- sm (6), md (10), lg (14), xl (20), full (999)

---

## Core UI Components (SwarmUI.js)

### StatusPip
Animated status indicator with three states:
- `online` → Green pulsing dot
- `alert` → Amber pulsing dot
- `offline` → Gray dot

### BotCard
Displays individual robot status:
- Bot name, role, color-coded icon
- Battery percentage & map coverage
- Current task (first task displayed)
- Status border color changes with status

### AlertBanner
Prominent alert display with severity level:
- Warning (amber) or Critical (red)
- Animated pulsing dot
- Title, detail, timestamp

### IncidentRow
Single-line incident log entry:
- Icon (color-coded by severity)
- Title, subtitle, timestamp
- Severity: warning, success, info

### MeshBadge
Network status badge:
- Shows "ESP-NOW" (current mesh protocol)
- Indicates if connection is active

---

## Screen Components

### DashboardScreen (Primary)
**Purpose**: Swarm overview and quick actions

**Sections**:
1. **Top Bar**: App title, mesh badge, bot count, theme toggle, notification bell
2. **Active Alerts**: AlertBanner components for critical issues
3. **Stat Cards**: Bots online, active alerts, map coverage percentage
4. **Swarm Status**: BotCard grid (3 cards)
5. **Control Mode**: Toggle between Autonomous and Manual RC modes
6. **Incident Log**: Last 4 incidents (scrollable)
7. **Emergency Override**: SOS button for emergency services

**Mock Data Used**:
- `BOTS` array (3 robots)
- `ALERTS` array
- `INCIDENTS` array
- `BOTS.length`, alert count calculation

### MapScreen
Expected to show:
- 2D floor plan / occupancy grid from Pathfinder SLAM
- Robot positions overlaid on map
- Room labels and coverage areas

### FeedsScreen
Expected to show:
- Video stream cards for each robot (Pathfinder, Guardian, Warden)
- MJPEG stream URLs loaded from AsyncStorage
- Ability to add/edit/remove stream URLs

### ControlScreen
Expected to show:
- Manual joystick/gamepad controls
- Direction buttons for each robot
- Mode selector (move, stop, etc.)
- Command status feedback

---

## API Service Layer (AegisService.js)

### REST Endpoints (Expected Backend)
```
GET  /api/bots              → Bot status, battery, location
GET  /api/incidents         → Incident log
POST /api/bots/{botId}/command → Send commands (move, stop, mode_autonomous, mode_manual)
POST /api/emergency         → Trigger SOS (type: sos, fire, police, ambulance)
GET  /api/map               → Occupancy grid from SLAM
GET  /video/{botId}         → MJPEG video stream
```

### WebSocket
```
ws://192.168.X.X:5000/ws   → Live telemetry updates (JSON messages)
```

### Current Implementation
- **Base URL**: `http://192.168.1.100:5000` (configurable)
- **Fetch helpers**: `fetchBotStatus()`, `fetchIncidents()`, `sendBotCommand()`
- **WebSocket**: `createSwarmSocket(onMessage)` callback pattern
- **Video URLs**: `getVideoStreamUrl(botId)` builder

---

## Mock Data Structure

### Bot Object
```javascript
{
  id: 'pathfinder',          // unique identifier
  name: 'Pathfinder',        // display name
  role: 'Explorer · SLAM',   // role description
  status: 'online',          // 'online' | 'alert' | 'offline'
  battery: 82,               // percentage
  mapCoverage: 78,           // percentage (null if not applicable)
  location: { x: 0.18, y: 0.50 },  // normalized 0–1 for map overlay
  color: '#3dd68c',          // hex color for UI
  icon: 'map-search',        // icon key for iconMap
  tasks: ['Mapping Room 3', 'Broadcasting occupancy grid']
}
```

### Alert Object
```javascript
{
  id: 'a1',
  type: 'gas',
  severity: 'warning',       // 'warning' | 'critical'
  title: 'Gas sensor reading elevated',
  detail: 'MQ-2 above threshold · Living Room',
  bot: 'Warden',
  time: '2m ago',
  icon: 'alert-triangle'
}
```

### Incident Object
```javascript
{
  id: 'i1',
  type: 'gas',
  title: 'Gas leak detected',
  sub: 'MQ-2 · Warden · Living Room',
  time: '2m ago',
  severity: 'warning',       // 'warning' | 'success' | 'info'
  icon: 'alert-triangle'
}
```

---

## Current Features

✅ Dark/light theme toggle
✅ Bottom tab navigation (Dashboard, Map, Feeds, Control)
✅ Bot status cards with battery and map coverage
✅ Alert and incident logging UI
✅ Mock data for development
✅ Theme context for app-wide styling
✅ Ionicons integration
✅ SafeArea support

---

## Next Steps / TODOs

### High Priority
1. **WebView Integration**: Add a 5th tab or screen for web dashboard
   - Create `WebViewScreen.js`
   - Add to AppNavigator
   - Host HTML content (locally, on a dev server, or cloud)

2. **Connect Backend**: Replace mock data with real API calls
   - Implement `useEffect` hooks in screens to call AegisService
   - WebSocket integration for live updates
   - Error handling and retry logic

3. **Video Streaming**: Implement MJPEG stream display in FeedsScreen
   - Load stream URLs from AsyncStorage
   - Display via WebView or native MJPEG player

### Medium Priority
4. Map visualization (MapScreen)
5. Manual control interface (ControlScreen)
6. Real robot command execution
7. Push notifications for alerts

### Low Priority
8. Offline mode / local caching
9. Cloud backup of incident logs
10. Multi-user support / role-based access

---

## WebView Hosting Options

### Option 1: Local HTML (Simplest)
Embed HTML string directly in WebViewScreen component.

**Pros**: No server, offline, instant
**Cons**: Static only

### Option 2: Local Node.js Server (Development)
Express server on dev machine, accessible from emulator/device via IP.

**Pros**: Live reload, easy iteration
**Cons**: Network-dependent, dev-only

### Option 3: Cloud Hosting (Production)
Deploy to Vercel, Netlify, or AWS.

**Pros**: Accessible anywhere, scalable
**Cons**: Internet required, potential costs

### Option 4: Backend Server Integration
Serve HTML from your Python Flask backend alongside API endpoints.

**Pros**: Unified API surface, easier auth/cors
**Cons**: Adds backend complexity

---

## Key Context for Development

### Design Principles
- **Dark tactical UI**: Deep navy backgrounds, cyan accents for brand, amber for warnings, red for critical
- **Minimal clutter**: Focus on essential info (bot status, alerts, commands)
- **Real-time updates**: WebSocket for live telemetry, no polling
- **Decentralized resilience**: UI must handle individual robot failures gracefully

### User Personas
1. **Emergency Responder**: Needs quick situational awareness, large readable text, one-tap SOS
2. **Robot Operator**: Needs manual control, real-time video, precise commands
3. **Administrator**: Needs incident logs, audit trail, settings management

### Performance Considerations
- Minimize re-renders (use `useMemo`, `useCallback`)
- Lazy-load video feeds
- Implement pagination for incident logs
- Cache API responses when appropriate

---

## Debugging Tips

### Common Issues

**Video Feed Won't Load**
- Verify backend video endpoint is running
- Check robot IP reachable: `ping <ip>`
- Ensure URL format includes endpoint path (e.g., `/video/pathfinder`)

**AsyncStorage Errors**
- Clear app cache: `npm start -- --clear`
- Reset AsyncStorage: uninstall and reinstall app

**WebSocket Connection Fails**
- Check backend ws:// server is running
- Verify firewall allows WebSocket port
- Monitor console for connection errors

**Styling Inconsistencies**
- Verify theme colors exported from `src/constants/theme.js`
- Check StyleSheet is called with correct colors object
- Test both dark and light modes

---

## References & Links

- [React Native Docs](https://reactnative.dev/)
- [Expo Documentation](https://docs.expo.dev/)
- [React Navigation](https://reactnavigation.org/)
- [Ionicons](https://ionic.io/ionicons)
- [react-native-webview](https://github.com/react-native-webview/react-native-webview)

---

## Contact & Team

**AEGIS Team**: Shahriar Jaman, MD Sakib Sarker, Ahmed Sadman Sadik, Mohammed Bin Ahmed, Zahid Hasan Rana, Md. Fahmidul Hasan

**Supervised by**: Dr. Shahnewaz Siddique, Associate Professor, Department of Electrical and Computer Engineering (ECE), North South University (NSU)

---

**Last Updated**: July 2026
