# AEGIS 

AEGIS Guardian FOMO is a multi-robot security and monitoring system built around ESP32 devices, a FastAPI backend, and a frontend dashboard. The platform combines fire/smoke detection, intrusion monitoring, object detection, and robot coordination into a single connected ecosystem.

## Overview

The project contains three main robot subsystems:

- Guardian: vision-based security monitoring using ESP32-S3-EYE and FOMO detection
- Warden: fire and smoke detection with alerting and incident reporting
- Pathfinder: mapping and navigation assistance with object and door detection

These robots communicate with a centralized backend that ingests telemetry, exposes video streams, and manages incidents and command flow.

## Repository Structure

```text
AEGIS_GUARDIAN_FOMO/
├── AEGIS_GURDIAN/
│   └── AEGIS_GURDIAN_MASTER_FOMO/
├── AEGIS_PATHFINDER/
│   └── aegis_pathfinder_master/
├── AEGIS_SOFTWARE/
│   ├── AEGIS-Backend/
│   └── AEGIS-Frontend/
├── AEGIS_WARDEN/
│   └── AEGIS_WARDEN_MASTER/
├── swarm_protocol/
├── ALL_BOTS_BACKEND_CONFIGURATION.md
├── README_QUICK_START.md
├── SWARM_ESPNOW_DESIGN.md
├── CHECK_SYSTEM.bat
└── README.md
```

## System Components

### Backend
The backend service is implemented in the AEGIS-Backend folder and exposes APIs for telemetry ingestion, incident reporting, and video proxying.

### Frontend
The frontend app is located in AEGIS_SOFTWARE/AEGIS-Frontend and provides monitoring and dashboard views for real-time robot status and stream data.

### Robot Firmware
Each robot firmware tree is configured for ESP-IDF development and includes the code needed to connect to Wi-Fi, report to the backend, and coordinate via ESP-NOW.

## Quick Start

1. Start the backend

```bash
cd AEGIS_SOFTWARE\AEGIS-Backend
START_BACKEND.bat
```

2. Build and flash a robot firmware target if needed

```bash
cd AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py build flash monitor
```

```bash
cd AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO
idf.py build flash monitor
```

```bash
cd AEGIS_PATHFINDER\aegis_pathfinder_master
idf.py build flash monitor
```

3. Start the frontend dashboard

```bash
cd AEGIS_SOFTWARE\AEGIS-Frontend
npm install
npm start
```

For more detailed setup, see [README_QUICK_START.md](README_QUICK_START.md) and [ALL_BOTS_BACKEND_CONFIGURATION.md](ALL_BOTS_BACKEND_CONFIGURATION.md).

## Key Features

- Multi-bot telemetry and state reporting
- Vision-based detection using FOMO models
- Fire, smoke, and security incident generation
- ESP-NOW swarm coordination
- Centralized API backend for monitoring and control
- Real-time frontend dashboard and video access

## Development Notes

- Backend: Python/FastAPI
- Frontend: React Native / Expo
- Embedded firmware: ESP-IDF for ESP32 boards
- Communication: Wi-Fi + ESP-NOW mesh coordination

## Documentation

The repository includes several design and implementation notes:

- [README_QUICK_START.md](README_QUICK_START.md)
- [ALL_BOTS_BACKEND_CONFIGURATION.md](ALL_BOTS_BACKEND_CONFIGURATION.md)
- [SWARM_ESPNOW_DESIGN.md](SWARM_ESPNOW_DESIGN.md)
- Robot-specific documentation under each subsystem directory

## Purpose

This project is intended for research, prototyping, and deployment of a connected defensive robotics and monitoring system. It is organized as a multi-module platform and is suitable for further extension with additional sensors, automation, or deployment tooling.
