# AEGIS Mobile App — Setup & Development Guide

## Overview

This is the live command dashboard for the A.E.G.I.S. swarm, built with React Native (Expo). It displays real-time video feeds from the robots, shows map data, alerts, and provides manual control interfaces.

## Prerequisites

- **Node.js** 18+ and npm
- **Expo CLI**: `npm install -g expo-cli`
- **Git**

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/sakibulla/AEGIS-Frontend.git
cd AEGIS-Frontend
```

### 2. Install Dependencies

```bash
npm install
```

### 3. Set Up Environment Variables (Optional)

Create a `.env` file in the project root if you need to configure API endpoints or robot IPs:

```
REACT_APP_API_URL=http://your-api-endpoint
REACT_APP_ROBOT_IPS=192.168.1.10,192.168.1.11,192.168.1.12
```

## Running the App

### Development Mode (Expo)

```bash
npm start
```

This opens the Expo dev menu. Press:
- `i` for iOS simulator (macOS only)
- `a` for Android emulator
- `w` for web (browser)

### Building for Android

```bash
eas build --platform android
```

### Running on Physical Device

1. Install **Expo Go** app from Google Play or Apple App Store
2. Run `npm start`
3. Scan the QR code in the Expo dev menu with your device

## Project Structure

```
aegis-app/
├── src/
│   ├── screens/          # Screen components (Dashboard, Feeds, Controls, Map)
│   ├── components/       # Reusable UI components
│   ├── hooks/            # Custom React hooks (e.g., useStreamUrls)
│   ├── services/         # API and data services
│   ├── constants/        # Theme, mock data, configuration
│   └── navigation/       # Navigation setup
├── android/              # Android native code
├── App.js                # Main app entry point
├── app.json              # Expo configuration
└── README.md             # Project overview
```

## Video Streaming Setup

### Live MJPEG Feeds

Each robot streams MJPEG video from its camera. Configure stream URLs in the app settings.

### Connecting to Robot Streams

1. Note the robot IP and streaming port (e.g., `http://192.168.1.100:8080`)
2. In the app, tap **Settings** on a feed card and paste the stream URL
3. The app stores URLs in AsyncStorage for persistence

## Available Scripts

| Command | Purpose |
|---------|---------|
| `npm start` | Start Expo dev server |
| `npm run android` | Build and run on Android emulator |
| `npm run ios` | Build and run on iOS simulator (macOS only) |
| `npm test` | Run test suite (if configured) |

## Troubleshooting

### App Crashes on Startup

- Clear cache: `npm start -- --clear`
- Restart the Expo dev server
- Check that all dependencies installed: `npm install`

### Video Feed Won't Load

- Verify the vision server is running on the robot
- Check that the robot IP is reachable: `ping <robot-ip>`
- Ensure the URL format is correct (includes `/video_feed` endpoint)
- Check app logs in Expo dev menu

### AsyncStorage Errors

- Clear app data on device or simulator
- Reinstall app: `npm install` then `npm start`

## Development Notes

- The app uses **React Navigation** for screen navigation
- **AsyncStorage** persists stream URLs and user settings
- **WebView** component renders MJPEG streams with HTML5 `<img>` tags
- Dark theme is defined in `src/constants/theme.js`

## Contributing

1. Create a new branch: `git checkout -b feature/your-feature`
2. Make your changes
3. Test on a physical device or emulator
4. Submit a pull request

## License

This project is part of the A.E.G.I.S. swarm initiative. See main README for details.

---

For questions or issues, open an issue on GitHub or contact the team.
