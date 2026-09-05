# A.E.G.I.S.
**Autonomous Emergency Ground Intelligence Swarm**

*A low-cost, COTS-based swarm of ground robots that responds to indoor emergencies — mapping the space, checking on people, and detecting hazards — so help doesn't have to wait for a human to arrive first.*

📺 **Project overview video:** [Watch on YouTube](https://www.youtube.com/watch?v=Inbc1NvTbIw)  
🌐 **Live Dashboard Demo:** [https://aegis-software-six.vercel.app](https://aegis-software-six.vercel.app)

![Platform](https://img.shields.io/badge/platform-iOS%20%7C%20Android%20%7C%20Web-blue)
![React Native](https://img.shields.io/badge/React%20Native-0.76.9-61DAFB?logo=react)
![Expo](https://img.shields.io/badge/Expo-52.0.0-000020?logo=expo)
![License](https://img.shields.io/badge/license-Academic%20Project-orange)

---

## 📋 Table of Contents

- [📸 Screenshots](#-screenshots)
- [🚀 Features at a Glance](#-features-at-a-glance)
- [❓ The Problem](#the-problem)
- [💡 The Solution](#the-solution)
- [✨ Why It's Different](#why-its-different)
- [👥 Who It's For](#who-its-for)
- [🏆 Project Status](#-project-status)
- [👥 Team](#-team)
- [🤝 Contributing](#-contributing)
- [📄 License](#-license)

---

## 📸 Screenshots

<div align="center">
  <p><em>🎬 Check out the <a href="https://aegis-software-six.vercel.app">live demo</a> to see the dashboard in action!</em></p>
</div>

### Dashboard Overview
<img src="assets/images/screenshots/Screenshot 2026-07-24 193458.png" alt="A.E.G.I.S. Dashboard" width="800"/>

*Real-time monitoring of all three robots with live telemetry, battery status, and alert notifications*

### Application Interface
<img src="assets/images/screenshots/Screenshot 2026-07-24 193507.png" alt="A.E.G.I.S. Interface" width="800"/>

*Complete view of the A.E.G.I.S. control system showing swarm status, incident logs, and bot controls*

### Key Features:
- **📊 Real-time status** monitoring for all 3 bots (Pathfinder, Guardian, Warden)
- **🚨 Live incident feed** with gas leaks, motion detection, and emergency alerts
- **🗺️ SLAM map visualization** showing explored areas
- **📹 Video feeds** from multiple robots
- **🎮 Manual control interface** for emergency override

---

## 🚀 Features at a Glance

| Feature | Description |
|---------|-------------|
| 🤖 **Multi-Robot Coordination** | Three specialized robots working as a decentralized swarm |
| 🗺️ **Real-time SLAM** | Live mapping and navigation without pre-existing floor plans |
| 📡 **ESP-NOW Communication** | Low-latency wireless mesh network between robots |
| 📱 **Cross-Platform Dashboard** | Monitor and control from iOS, Android, or web browser |
| 🚨 **Emergency Detection** | Gas leaks, fire, falls, and intrusion detection |
| 🎮 **Manual Override** | Take direct RC control during critical situations |
| 💰 **Low-Cost Design** | Total hardware cost ~$453 USD using COTS components |
| 🔋 **Autonomous Operation** | No human intervention needed for routine patrols |

---

## The Problem

Most homes and small buildings only have passive safety tools — fixed cameras and alarms:

- **Blind spots** — fixed cameras miss closed rooms and corners
- **No context** — an alarm goes off, but nobody knows if it's a false alarm or someone is actually trapped
- **No action** — alerts just... alert. They can't go check, and they can't help.

## The Solution

A.E.G.I.S. is a team of three small autonomous robots that work together like a hive — no central controller, no single point of failure. Each robot has a distinct job:

| Robot | Role | What it does |
|---|---|---|
| 🟦 **Pathfinder** | Explorer | Maps the space, finds doors and obstacles, shares the map with the team |
| 🟧 **Guardian** | Rescue | Listens for calls for help, checks on people, carries a first-aid box |
| 🟥 **Warden** | Hazard Control | Watches for fire and gas leaks, searches for trapped people after a fire |

They talk to each other wirelessly in real time, so if one robot goes offline, the other two keep working. Everything is viewable live on a central dashboard — video feeds, maps, alerts, and status.

**Total cost to build all three robots: ~$453 USD (55,670 BDT)** — a fraction of the cost of comparable commercial or research robotics platforms.

## Why It's Different

- **Actually does something** — instead of just alerting, the robots physically go check, assess, and even deliver first aid
- **No single point of failure** — fully decentralized; the mission continues even if a robot fails
- **Cheap and reproducible** — built entirely from off-the-shelf parts (ESP32s, common sensors), no custom hardware
- **Built for real emergencies** — designed around the situations that actually happen most: fires, gas leaks, falls, intrusions

## Who It's For

- **Homes** — active protection against fire, gas leaks, falls, and intrusions, no subscription needed
- **Small offices & shops** — an affordable alternative to expensive single-purpose security systems
- **Under-resourced areas** — low-cost, locally buildable emergency response where professional services are slow or limited

## 🏆 Project Status

Built as a senior design project at the Department of ECE, North South University.

- ✅ **Phase 1 — Lab Prototype:** Core architecture validated, individual components tested
- 🔄 **Phase 2 — Controlled Testing:** Full swarm integration and scenario rehearsals (in progress)
- ⏳ **Phase 3 — Real-World Pilots:** Campus building and residential trials
- ⏳ **Phase 4 — Release:** Open-source publication or commercial partnership

## 👥 Team

**Shahriar Jaman** · **MD Sakib Sarker** · **Ahmed Sadman Sadik** · **Mohammed Bin Ahmed** · **Zahid Hasan Rana** · **Md. Fahmidul Hasan**

Supervised by **Dr. Shahnewaz Siddique**, Associate Professor, Department of Electrical and Computer Engineering (ECE), North South University (NSU)

---

## 🤝 Contributing

This is an academic research project. If you're interested in collaborating or have questions about the system architecture, please reach out to the team or supervisor.

## 📄 License

This project was developed as part of an undergraduate capstone project at North South University. Please contact the team for licensing information.

---

<div align="center">

### 🌟 Star this repo if you find it interesting!

**Links:** [🌐 Live Demo](https://aegis-software-six.vercel.app) • [📺 Video](https://www.youtube.com/watch?v=Inbc1NvTbIw) • [📚 Documentation](docs/)

*Built with ❤️ by the A.E.G.I.S. team at North South University*

</div>
