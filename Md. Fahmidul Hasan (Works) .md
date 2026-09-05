# AEGIS Project — Progress Report

**Prepared by:** Md. Fahmidul Hasan
**Covers:** Work completed to date (through the 25th)

---

## 1. Frontend — built end-to-end

I built the entire mobile app (React Native / Expo) from the ground up. It's the single screen our group leader and anyone else would look at to see what all three bots are doing at once — instead of three separate tools, there's one dashboard that shows Guardian, Pathfinder, and Warden side by side, live.

Each bot needed different information shown because they do different jobs: Pathfinder is about exploration and mapping (SLAM), Guardian is about rescue/defence and has AI vision (face/person detection), and Warden is about hazard detection (fire/gas). Rather than one generic bot card, I built the data layer so each bot's card shows the metric that actually matters for that bot — a map-building status for Pathfinder, a wake-word/first-aid state for Guardian, and fire/gas alerts for Warden — while still using shared, reusable UI components everywhere so the app stays consistent instead of turning into three different mini-apps.

I also built the supporting screens around the dashboard: a live map view, a manual control screen, a feeds screen for camera snapshots, and an about/info screen — plus a small shared component library (bot cards, alert banners, incident rows, mesh/swarm status badges) so new screens don't have to be built from scratch.

- [DashboardScreen.js](AEGIS_SOFTWARE/AEGIS-Frontend/src/screens/DashboardScreen.js) — main dashboard: a `BotCard` grid for all three bots, a "Bots online X/3" swarm status row, live incident log, and the PDF report download button.
- [useTelemetry.js](AEGIS_SOFTWARE/AEGIS-Frontend/src/hooks/useTelemetry.js) — `BOT_META` defines all three bots (Pathfinder: Explorer/SLAM, Guardian: Rescue/Defence/AI Vision, Warden: Hazard/Fire/Gas) and normalizes each bot's live status differently.
- Supporting screens: `MapScreen.js`, `ControlScreen.js`, `FeedsScreen.js`, `AboutScreen.js`, plus reusable `SwarmUI.js` components (`BotCard`, `AlertBanner`, `IncidentRow`, `MeshBadge`).

## 2. Connected the backend to all three bots and surfaced it in the frontend

The backend server (built by a teammate) is where all three bots' data lands, but on its own it's just an API — nobody was actually looking at it. My job was to wire the frontend up to that API so the data becomes something people can actually see and use in real time.

Concretely, that meant building the connection layer that pulls each bot's status individually (since Guardian, Pathfinder, and Warden each report different kinds of data) and also opening a live connection (WebSocket) so the dashboard updates itself the instant a bot reports something new, rather than the user having to refresh the app. This is the piece that makes the dashboard feel "live" instead of a static status page.

- [routes.py](AEGIS_SOFTWARE/AEGIS-Backend/app/api/routes.py) — `POST /api/v1/telemetry/ingest` takes `{bot_id, kind, payload}` from any bot; `GET /api/v1/bots`, `/bots/{bot_id}`, `/bots/{bot_id}/detections`, `/bots/{bot_id}/map` are bot-scoped.
- WebSocket broadcast (`/api/v1/ws/telemetry`) streams `TELEMETRY_FRAME` updates live to the dashboard for all three bots simultaneously — this is what drives the real-time `useTelemetry` hook on the frontend.

## 3. Implemented the database and PDF report download

Before this, incidents (fire detected, gas detected, person found, etc.) only existed for as long as the app was open — nothing was saved. I added a real database on the backend so every incident from every bot gets permanently recorded with its severity, type, source bot, and timestamp, and can be looked back on later or filtered (e.g. "show me only Warden's critical incidents from today").

On top of that, I built the "download report" feature: a button in the app that generates a proper PDF of the incident history — a formatted table with time, bot, type, severity, and message, color-coded by how serious each incident was — so the data isn't just trapped on a phone screen, it can be printed, shared, or filed as a record.

- [database.py](AEGIS_SOFTWARE/AEGIS-Backend/app/db/database.py) — SQLite database (`aegis_incidents.db`) via SQLAlchemy.
- [models.py](AEGIS_SOFTWARE/AEGIS-Backend/app/db/models.py) — `IncidentRecord` table (`id, title, severity, type, message, bot_id, timestamp, active`).
- [incident_store.py](AEGIS_SOFTWARE/AEGIS-Backend/app/services/incident_store.py) — persists incidents to the DB and supports filtering by bot/severity/type/date range.
- [pdf_export.py](AEGIS_SOFTWARE/AEGIS-Backend/app/services/pdf_export.py) — generates the report using `reportlab` (chosen so the server doesn't need an external PDF tool installed).
- Exposed via `GET /api/v1/incidents/pdf`, which the frontend's download button calls directly.

## 4. Replaced live video feeds with snapshot polling (space constraint)

Originally the plan was to show a live video feed from each bot's camera in the app. In practice this was too heavy — continuous video eats bandwidth and memory on both the bot's side and the phone's side, and we ran into real space limitations trying to keep three simultaneous video streams running.

To fix this without losing the "see what the bot sees" feature, I replaced continuous video with a snapshot-based feed: the app just asks the bot for a fresh photo roughly once a second and displays it, refreshing continuously so it still looks close to live, but at a fraction of the resource cost of true video streaming. I kept a visible "SNAPSHOT MODE" indicator so it's clear to the user this is a still-image feed and not full video, along with proper loading/offline states if a bot goes quiet.

- [SimpleSnapshotViewer.js](AEGIS_SOFTWARE/AEGIS-Frontend/src/components/SimpleSnapshotViewer.js) — pulls a fresh JPEG snapshot from the bot on a fixed interval (`setInterval(..., 500)`, roughly a 1-second refresh cycle) instead of holding open a continuous video stream.
- The older continuous-stream component (`StreamViewer.js`) was retired from the live feed screens in favor of this approach once space limitations made full video impractical.

## 5. Implemented the ESP-NOW swarm across all three master bots (works without Wi-Fi)

One of the core requirements was that the three bots be able to talk to each other directly — e.g. Guardian warning Pathfinder and Warden about a threat — even if there's no Wi-Fi router around, since in a real deployment the bots may be out of range of any network. I implemented this using ESP-NOW, a peer-to-peer radio protocol built into the ESP32 chips that works without an access point at all.

I designed the shared message format all three bots use to talk to each other (what a "fire alert" or "map update" packet looks like on the wire), and then implemented the actual link on each of the three Master bots so that each one knows how to reach the other two directly, radio-to-radio, all fixed to the same channel so they don't miss each other's messages.

- [aegis_swarm_protocol.h](swarm_protocol/aegis_swarm_protocol.h) — shared protocol header: fixed `AEGIS_SWARM_CHANNEL 6` (independent of the backend Wi-Fi's connection state), node IDs for Guardian/Pathfinder/Warden, hardcoded peer MAC addresses, and a packed `swarm_packet_t` (fire/security/map alert payloads) sized to stay under ESP-NOW's 250-byte cap.
- Each master has its own `swarm_link.cpp` implementing the link: [AEGIS_GURDIAN_MASTER_FOMO/main/swarm_link.cpp](AEGIS_GURDIAN/AEGIS_GURDIAN_MASTER_FOMO/main/swarm_link.cpp), [aegis_pathfinder_master/main/swarm_link.cpp](AEGIS_PATHFINDER/aegis_pathfinder_master/main/swarm_link.cpp), [AEGIS_WARDEN_MASTER/main/swarm_link.cpp](AEGIS_WARDEN/AEGIS_WARDEN_MASTER/main/swarm_link.cpp).
- Each registers the other two bots as ESP-NOW peers and pins the Wi-Fi channel to 6, all while running in station mode without an actual access-point connection — which is what makes the swarm keep working even with no Wi-Fi present.

## 6. Added the missing slave MAC addresses so all three share swarm channel 6

Each bot is actually two devices: a Master (the "brain") and a Slave (a smaller helper board handling sensors). For the Slave to send data to its own Master over ESP-NOW, it needs to know the Master's exact radio address (MAC address) and be listening on the same channel.

When I picked this up, only the Guardian slave had this configured — Pathfinder and Warden's slave boards didn't know their Master's address yet, so they had no way to actually deliver their sensor data. I added the correct Master MAC address to both of those slaves and confirmed all three were locked to channel 6, so all three Master↔Slave pairs — and the inter-Master swarm link from item 5 — now operate cleanly on the same channel without addressing gaps or collisions.

- Guardian slave: [AEGIS_GURDIAN_SLAVE/src/main.cpp](AEGIS_GURDIAN/AEGIS_GURDIAN_SLAVE/src/main.cpp) — already had `GUARDIAN_MASTER_MAC[]` + `GUARDIAN_ESPNOW_CHANNEL 6`.
- Pathfinder slave: [espnow_slave.h](AEGIS_PATHFINDER/aegis_pathfinder_slave/main/espnow_slave.h) — added the master MAC; confirmed `WIFI_CHANNEL 6` used for channel-set and peer registration.
- Warden slave: [AEGIS_WARDEN_SLAVE/src/main.cpp](AEGIS_WARDEN/AEGIS_WARDEN_SLAVE/src/main.cpp) — added `WARDEN_MASTER_MAC[]` alongside `ESPNOW_CHANNEL 6`.

## 7. Implemented the Warden slave code and its MQ2 gas sensor reporting

Warden's whole purpose is hazard detection, and gas is one of the two hazards it's supposed to catch (alongside fire). I implemented the Warden slave's firmware logic for reading the MQ-2 gas sensor and getting that reading back to the Master.

This wasn't just "read a pin" — the Master→Slave link originally only worked in one direction (Master could send commands down to the Slave, but the Slave had no way to send anything back up). I had to add a new message type specifically for the Slave to report its own sensor data upward, then implement the actual sensor-reading and threshold-checking logic: the slave continuously samples the gas sensor, and when the reading crosses a danger threshold, it packages that up and sends it to the Master over ESP-NOW so it can be acted on (alerted in the app, shared with the swarm, etc.).

- [warden_protocol.h](AEGIS_WARDEN/AEGIS_WARDEN_SLAVE/include/warden_protocol.h) — added `warden_slave_status_t`, a new packed struct carrying `gas_detected` and `gas_value` back to the Master.
- [AEGIS_WARDEN_SLAVE/src/main.cpp](AEGIS_WARDEN/AEGIS_WARDEN_SLAVE/src/main.cpp) — `readGasSensor()` reads the MQ-2 on GPIO34, `checkGasLevel()` compares against a threshold (650) on a rate-limited interval, and `sendGasStatusToMaster()` sends the reading via `esp_now_send`.
- Documented wiring/calibration notes in `AEGIS_WARDEN_SLAVE/GAS_SENSOR_NOTES.md`.

## 8. Bug fixes and memory/space optimization

As the project grew — more sensors, an AI vision model, a display UI — the Master boards started running out of RAM and flash, and things that used to work quietly broke. Most of my later time went into tracking these down and fixing them one by one:

- **Channel mismatch bug** — Pathfinder's Master was accidentally hardcoded to talk on ESP-NOW channel 9, while its Slave had only ever been listening on channel 6. This meant the Master and Slave were never actually able to hear each other, even though nothing looked wrong in the code at a glance. I traced this down and fixed the Master to use the Slave's real channel. ([aegis_pathfinder_master/main/main.cpp](AEGIS_PATHFINDER/aegis_pathfinder_master/main/main.cpp), ~line 871)
- **Display freezing under memory pressure** — once the face-recognition/vision model was loaded, the on-screen display would fail to allocate the memory it needed to draw. I fixed this on both Guardian and Pathfinder by shrinking how much of the screen the display driver tries to buffer at once (from a full frame down to a small strip) and moving that buffer to a faster, more reliable part of memory, so the display and the AI model could coexist without crashing.
  - Guardian Master ([main.cpp](AEGIS_GURDIAN/AEGIS_GURDIAN_MASTER_FOMO/main/main.cpp), ~line 2562)
  - Pathfinder Master ([main.cpp](AEGIS_PATHFINDER/aegis_pathfinder_master/main/main.cpp), ~line 880)
- **Flash partition tuning** — the base partition layout (how the flash storage is divided between the app code, the AI model, and other data) was originally set up by a teammate, but as the app and model grew, it stopped fitting. I re-tuned the region sizes on all three Masters so everything fits without overflowing flash. ([AEGIS_GURDIAN_MASTER_FOMO/partitions.csv](AEGIS_GURDIAN/AEGIS_GURDIAN_MASTER_FOMO/partitions.csv), [aegis_pathfinder_master/partitions.csv](AEGIS_PATHFINDER/aegis_pathfinder_master/partitions.csv), [AEGIS_WARDEN_MASTER/partitions.csv](AEGIS_WARDEN/AEGIS_WARDEN_MASTER/partitions.csv))
- **Shrinking the data bots send to the backend** — ESP-NOW has a hard 250-byte limit per message, and as more sensors got added, the data a bot needed to send started bumping up against that limit ("space with the bot" issue). Instead of sending verbose text/JSON, I kept all the swarm and sensor packets as small, fixed-size binary structures with every field's byte size accounted for, so everything fits reliably even as more sensors were added — this is the "partition work" on the data side, as opposed to the flash partition work above.

---

### Summary

| Area | Status |
|---|---|
| Frontend (all screens, all 3 bots) — built by me | Done |
| Backend ↔ frontend integration | Done |
| Database + PDF report export | Done |
| Video → 1-second snapshot feed (space optimization) | Done |
| ESP-NOW swarm (Wi-Fi-independent) across 3 masters | Done |
| Slave MAC addresses added (Pathfinder, Warden) + channel-6 addressing | Done |
| Warden slave code + MQ2 gas sensor → Master | Done |
| Channel mismatch fix + buffer/partition memory optimization | Done |
