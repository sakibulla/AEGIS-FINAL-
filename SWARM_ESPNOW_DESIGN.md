# AEGIS Swarm — Master-to-Master ESP-NOW Design

Status: **design draft, not yet implemented**
Scope: Guardian Master, Pathfinder Master, Warden Master (all ESP32-S3-Eye).
Out of scope: each Master's existing Master↔Slave ESP-NOW link (Guardian↔Guardian-Slave,
Pathfinder↔Pathfinder-Slave, Warden↔Warden-Slave) — those keep working exactly as they do today.
This document only adds a **second, independent ESP-NOW link between the three Masters**.

---

## 1. Goal

Right now the three bots are islands. Guardian doesn't know Warden saw fire; Pathfinder finds a
door and only a stub broadcast function (unused by anyone) exists; nobody knows Guardian just
woke up. The goal is a peer-to-peer swarm layer, over ESP-NOW only, so the three Masters can
tell each other:

- **Guardian**: "Hi ESP" wake word fired → wake up the swarm.
- **Warden**: fire/smoke detected → alert the swarm.
- **Guardian**: intruder / dangerous object confirmed → alert the swarm.
- **Pathfinder**: door found, local map snapshot → share with the swarm.
- All three: periodic heartbeat, so each bot knows if a sibling has gone offline.

This must keep working even if the backend Wi-Fi AP is down, out of range, or was never
configured — it must not depend on the `A34` AP association being alive.

Swarm state is **bot-to-bot only** (confirmed, §10 Q4) — the backend/dashboard never sees it. So
each Master must also surface swarm status on its own **integrated LCD** (§11), otherwise nobody
outside earshot of the serial monitor can tell the swarm link is even alive.

---

## 2. The channel problem (read this before implementing)

Today, every Master calls `esp_wifi_connect()` to the backend AP and then reads back
whatever channel it landed on (`esp_wifi_get_channel()` in Guardian's `wifi_event_handler`,
same pattern in Warden's `espnow_init()`). ESP-NOW piggybacks on that channel.

That's fine for Master↔own-Slave (single link, whichever channel happens to be current), but it's
fragile for a 3-way mesh: if the AP's channel changes, or one bot briefly loses the AP while the
other two are still associated, all three drift onto different channels and ESP-NOW between
Masters silently stops working — with no error, packets just vanish.

**ESP-IDF constraint that matters here:** if Wi-Fi STA is *associated* to an AP,
`esp_wifi_set_channel()` has no effect — the radio is locked to the AP's channel. You can only
freely set an arbitrary channel when STA is *not* associated to anything. This means "pin ESP-NOW
to a fixed channel" and "stay connected to the backend AP" are only simultaneously true if the AP
itself is pinned to that channel.

**Design decision (per your channel-strategy answer): fixed channel, decoupled from the AP.**
Concretely, that means:

1. Define `AEGIS_SWARM_CHANNEL 6` (matches Guardian's existing unused `ESPNOW_CHANNEL 6` define)
   as a compile-time constant shared by all three projects — not something read back from
   `esp_wifi_get_channel()`.
2. **If the backend AP is reachable:** the AP itself must be pinned to channel 6 in its router
   config. This is an infra requirement, not a firmware one — most routers let you fix the
   2.4 GHz channel instead of "Auto". When a Master associates to a channel-6 AP, `esp_wifi_get_channel()`
   trivially confirms 6 and everything lines up — no behavior change needed for the happy path.
3. **If the backend AP is unreachable or was never configured:** each Master, once it gives up
   Wi-Fi retries (or if `ENABLE_BACKEND` is compiled out), explicitly calls
   `esp_wifi_set_mode(WIFI_MODE_STA)` (already done) followed by
   `esp_wifi_set_channel(AEGIS_SWARM_CHANNEL, WIFI_SECOND_CHAN_NONE)` **without** connecting to an
   AP. This is the standard "ESP-NOW-only" bring-up pattern from Espressif's own examples. The
   swarm link comes up fully independent of any router — true "without Wi-Fi" operation.
4. Either way, the ESP-NOW **peer entries for the other two Masters** are always registered with
   `peer.channel = AEGIS_SWARM_CHANNEL` (not `0`/home-channel like the existing Master↔Slave
   peers use) — so a stale/rogue AP channel can never silently detune the swarm link.

Net effect: the backend HTTP path is best-effort and can flap freely; the swarm path is a hard
constant that never depends on AP state. This is the one router config change required
(pin channel 6) — flag it now so it doesn't get discovered during field testing.

---

## 3. Topology & node IDs

```
            ┌───────────────┐
            │  Guardian     │  wake word, face/intruder, dangerous-object
            │  Master       │
            └───┬───────┬───┘
     swarm ESP-NOW (ch 6, broadcast)
   ┌────────────┴──┐   ┌┴───────────────┐
   │  Pathfinder    │   │  Warden        │
   │  Master        │   │  Master        │
   │ doors/exits,   │   │ fire / smoke   │
   │ local map      │   │                │
   └────────────────┘   └────────────────┘
```

Each Master keeps its existing private link straight down to its own Slave — unaffected.

```c
// aegis_swarm_protocol.h
typedef enum {
    SWARM_NODE_GUARDIAN   = 1,
    SWARM_NODE_PATHFINDER = 2,
    SWARM_NODE_WARDEN     = 3,
} swarm_node_id_t;
```

Note `SWARM_NODE_WARDEN = 3` intentionally does **not** reuse Warden's existing
`WARDEN_SOURCE_ID = 2` (that ID is scoped to the Master↔Slave protocol and must stay untouched;
the swarm layer has its own independent ID space to avoid coupling the two protocols).

---

## 4. MAC registry

Three more addresses, following the exact pattern already used for the hardcoded slave MACs
(`SLAVE_MAC` in `espnow_master.cpp`, `s_pathfinder_slave_mac` in `espnow_mesh.cpp`,
`WARDEN_SLAVE_MAC` in `warden_config.h`):

```c
// aegis_swarm_protocol.h
static const uint8_t SWARM_MAC_GUARDIAN[6]   = { 0xE8, 0xF6, 0x0A, 0xD6, 0xC8, 0xE8 };  // E8:F6:0A:D6:C8:E8
static const uint8_t SWARM_MAC_PATHFINDER[6] = { 0xE8, 0xF6, 0x0A, 0xD7, 0x91, 0x60 };  // E8:F6:0A:D7:91:60
static const uint8_t SWARM_MAC_WARDEN[6]     = { 0x94, 0xA9, 0x90, 0x0A, 0xEF, 0x6C };  // 94:A9:90:0A:EF:6C
```

These were recorded directly off each board (STA MAC) and are final — no MAC-logging bring-up
step needed. Re-record and update this table if a board is ever replaced.

Each Master registers the *other two* as ESP-NOW peers at init (in addition to its own Slave peer
it already has), all with `channel = AEGIS_SWARM_CHANNEL`, `encrypt = false`. A broadcast peer
(`FF:FF:FF:FF:FF:FF`) is also kept registered — used for alerts that all bots should get without
needing to enumerate them (Pathfinder's `espnow_mesh.cpp` already registers this broadcast peer
today, for reference).

Point-to-point (`esp_now_send` to a specific MAC) vs. broadcast is a per-message choice — see §6.

---

## 5. Shared packet format

One new header, copied verbatim into all three `main/` directories (no shared-library
infrastructure exists between the three ESP-IDF projects today, so duplication — same approach
already used for e.g. `WIFI_SSID`/`WIFI_PASSWORD` constants — is the pragmatic choice; keep a
single canonical copy in a `swarm_protocol/` folder at the repo root and diff against it before
each flash, rather than building real cross-project shared-component infra for 3 files).

```c
// aegis_swarm_protocol.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SWARM_MAGIC 0xA5C1   // distinguishes swarm packets from this bot's own Master<->Slave traffic

typedef enum {
    SWARM_MSG_HEARTBEAT   = 0x01,  // periodic liveness, all bots, broadcast
    SWARM_MSG_WAKE        = 0x02,  // Guardian -> swarm: "Hi ESP" fired, everyone wake up
    SWARM_MSG_FIRE_ALERT  = 0x03,  // Warden   -> swarm: fire/smoke state change
    SWARM_MSG_SECURITY_ALERT = 0x04, // Guardian -> swarm: intruder / dangerous object
    SWARM_MSG_MAP_UPDATE  = 0x05,  // Pathfinder -> swarm: door/map snapshot (mirrors existing MapPacket)
    SWARM_MSG_ALL_CLEAR   = 0x06,  // any bot -> swarm: its own alert condition cleared
} swarm_msg_type_t;

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;        // SWARM_MAGIC, always first two bytes -> demux key in recv_cb
    uint8_t  type;         // swarm_msg_type_t
    uint8_t  source;       // swarm_node_id_t
    uint32_t sequence;     // monotonically increasing per-sender, for dedup / drop detection
    uint32_t uptime_ms;    // sender's uptime, doubles as a coarse heartbeat timestamp

    union {
        struct {                       // SWARM_MSG_FIRE_ALERT
            uint8_t  state;            // mirrors warden_command_t: 0 clear,1 fire,2 smoke,3 both
            uint16_t fire_confidence_x1000;
            uint16_t smoke_confidence_x1000;
        } fire;

        struct {                       // SWARM_MSG_SECURITY_ALERT
            uint8_t  is_intruder;
            uint8_t  is_dangerous_object;
            float    confidence;
            char     label[24];        // e.g. object label, "Intruder"
        } security;

        struct {                       // SWARM_MSG_MAP_UPDATE
            float    x, y;             // spatial coordinate (matches existing MapPacket)
            bool     has_door;
            uint8_t  snap_index;
            uint8_t  total_snaps;
        } map;

        uint8_t raw[24];               // headroom / future fields, keeps struct size stable
    } payload;
} swarm_packet_t;   // fixed size well under ESP-NOW's 250-byte cap
#pragma pack(pop)
```

Design choices worth calling out:
- **`magic` first** is what makes the demux in §7 possible: every existing per-bot protocol
  (`guardian_message_t`, `warden_packet_t`, `pathfinder_scan_packet_t`) starts with something else
  (a different magic, or a raw `msg_type` byte that never collides with `0xC1`/`0xA5`), so a single
  `esp_now_register_recv_cb` can safely tell "this is swarm traffic" from "this is my own Slave
  talking to me" by checking the first two bytes before doing anything else.
- **`sequence`** lets a receiver ignore an out-of-order/duplicate retransmit and detect a sender
  restart (sequence resets to 0).
- Fixed-size union keeps every message the same wire size — simplest possible framing, no length
  field needed, `esp_now_send(..., sizeof(swarm_packet_t))` always.

---

## 6. Message flows

| Trigger (existing code) | Sender | Delivery | Receivers do |
|---|---|---|---|
| `voice_activation_wait()` returns true, before camera/AI init (`main.cpp` `app_main`) | Guardian | broadcast | Pathfinder/Warden each start a 5-minute delay timer on receipt; if not cleared before it elapses, they transition to scout/active-patrol behavior (see §10 Q3, §8a) |
| `update_robot_security_state()` alert transition (`main.cpp`) | Guardian | broadcast | Warden/Pathfinder log the alert; Pathfinder can bias movement away from the reported area (future) |
| Warden fire/smoke state machine, wherever it currently calls into its own `espnow_init` send path (`warden_main.cpp`) | Warden | broadcast | Guardian escalates its own alert state even without seeing the fire itself; Pathfinder (future) can route toward exits instead of continuing its scan pattern |
| `broadcast_map_packet()` call sites in Pathfinder's `main.cpp` (already exist, currently going nowhere) | Pathfinder | broadcast | Guardian/Warden log door coordinates; this is the one flow that's 90% built already — it just needs a `SWARM_MSG_MAP_UPDATE`-shaped payload and two more listeners |
| Timer, every ~5 s, same cadence as each bot's existing `status_task` | all three | broadcast | Each bot keeps a small `last_seen[3]` table; a sibling silent for >15 s is logged as "offline" (surfaced to backend telemetry as a bonus, not required for v1) |

Everything above is **broadcast**, not point-to-point — with only 3 nodes and events that every
sibling plausibly cares about, broadcast keeps the logic simple (no per-recipient retry/ack
bookkeeping) at the cost of a few wasted bytes on the bot that doesn't act on a given message.
Point-to-point is worth revisiting only if a future message is meaningfully sensitive
(e.g. only Pathfinder should ever see raw map data) — not the case for any message above.

---

## 7. Receive-side demux

Each Master's existing `esp_now_register_recv_cb` handler needs a **two-line prefix check**
before its existing logic, nothing more invasive:

```c
static void master_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len >= (int)sizeof(uint16_t) && *(const uint16_t *)data == SWARM_MAGIC) {
        swarm_handle_packet(info, data, len);   // new file: swarm_link.c/.cpp
        return;
    }
    // ...existing per-bot handling unchanged below this line...
}
```

This is a small, low-risk change to three files that already exist:
`espnow_master.cpp` (Guardian), `espnow_mesh.cpp` (Pathfinder), and the recv callback registered
inside `espnow_init()` in `warden_main.cpp`.

---

## 8. New files per project

Same three files added to each of the three Master `main/` directories:

- `aegis_swarm_protocol.h` — §5, byte-identical across all three projects.
- `swarm_link.h` / `swarm_link.cpp` — owns: peer registration for the other two Masters + the
  broadcast peer at `AEGIS_SWARM_CHANNEL`; `swarm_send(type, payload)`; `swarm_handle_packet(...)`
  (called from the demux in §7); the `last_seen[3]` liveness table; the heartbeat timer task.

Existing files get small, additive edits — no existing Master↔Slave logic is touched:

- Guardian: `espnow_master.cpp` (demux prefix + call `swarm_link_init()`), `main.cpp`
  (call `swarm_send(SWARM_MSG_WAKE, ...)` right after the wake-word wait returns, and
  `swarm_send(SWARM_MSG_SECURITY_ALERT, ...)` inside `update_robot_security_state()`).
- Pathfinder: `espnow_mesh.cpp` (demux prefix), `main.cpp` (replace/augment the existing
  `broadcast_map_packet()` call sites to also emit `SWARM_MSG_MAP_UPDATE`).
- Warden: `warden_main.cpp` (demux prefix in `espnow_init()`'s recv callback, `swarm_send(SWARM_MSG_FIRE_ALERT, ...)`
  wherever `warden_command_t` currently transitions).

### 8a. `SWARM_MSG_WAKE` → 5-minute scout delay (Warden only — see update below)

> **Update (post-implementation):** Pathfinder no longer waits for `SWARM_MSG_WAKE` at all.
> Testing showed a standalone Pathfinder (no Guardian in range, no backend Wi-Fi) never started
> scouting, since it could never receive a `WAKE` to arm the timer described below. Pathfinder now
> runs its own boot-time duty cycle instead: scout for 5 minutes, rest for 5 minutes, repeat,
> starting the moment it powers on (`pathfinder_mapping_task` in `main.cpp`, `DUTY_CYCLE_RUN_MS`/
> `DUTY_CYCLE_REST_MS`). It still reacts to `SWARM_MSG_FIRE_ALERT` (evacuation) and
> `SWARM_MSG_ALL_CLEAR` exactly as before. Warden's behavior is unchanged and still follows the
> `WAKE`-gated design below.

Per §10 Q3: receiving `SWARM_MSG_WAKE` does not itself start scouting. It arms a one-shot
5-minute (`300000 ms`) software timer (`esp_timer` one-shot, same primitive as the heartbeat's
periodic timer in `swarm_link.cpp`) local to each of Pathfinder's and Warden's own state
machines — `swarm_link` only needs to invoke a callback on `SWARM_MSG_WAKE` receipt, it does not
own the timer or the resulting behavior change itself:

- On `SWARM_MSG_WAKE` receipt: if no timer is currently pending, start it. If one is already
  pending (a second `WAKE` arrived before the first fired — e.g. Guardian re-triggered), restart
  it from zero rather than stacking timers.
- On elapse with nothing having cancelled it: transition into scout/active-patrol behavior
  (Pathfinder's existing movement/scan state machine in `main.cpp`; Warden's equivalent in
  `warden_main.cpp`) — the concrete "what scouting does" behavior is each bot's own existing
  patrol logic, not something new introduced by the swarm layer.
- Cancellation: any bot broadcasting `SWARM_MSG_ALL_CLEAR` before the 5 minutes are up cancels a
  pending timer without triggering scout mode. (Whether Guardian's own alert clearing should emit
  `ALL_CLEAR` here, vs. only using it for fire/security alerts as originally scoped in §5, is a
  small follow-up decision — flag if you want `WAKE` cancellation wired to something narrower.)
- This is per-node, not swarm-wide: Pathfinder and Warden each run their own independent 5-minute
  timer off the same `WAKE` broadcast; one going into scout mode does not depend on or affect the
  other's timer.

---

## 9. Interaction with the backend's bot-fetch model

`AEGIS-Backend` (`app/services/video_service.py`) is not a passive telemetry sink — it actively
**pulls** from each bot on demand:

- It learns a bot's current IP from the `ip_address` field inside the status telemetry POST each
  Master already sends (`routes.py` line ~614, `video_service.register_bot_ip(...)`).
- Whenever the frontend dashboard has an active viewer, a background worker does
  `GET http://<bot-ip>/stream` (and falls back to `/snapshot` or `/capture`) directly against the
  bot, plus `GET /detections` and `GET /control` for Pathfinder specifically.
- **Gap found in passing, not swarm-related, flagging since you brought this up:** Pathfinder's
  `web_streamer.cpp` implements `/detections` and `/control` handlers; Guardian and Warden don't
  implement either route today, so those two calls presumably fail/timeout against them right now.
  Not something this design touches — just worth knowing before someone spends time debugging why
  the dashboard's control panel does nothing for Guardian/Warden.

**What this means for the swarm design:**

1. **This pull model needs the bots on Wi-Fi, routable, all the time** — it's a hard dependency,
   not best-effort like the HTTP telemetry push. §2's "ESP-NOW-only, no-AP" fallback path is still
   correct as a *degradation* behavior (a bot that has genuinely lost the AP keeps talking to its
   two siblings over the swarm even though the dashboard can no longer reach it directly) — but it
   is not something to trigger proactively. The expected/primary deployment is: all three bots
   Wi-Fi-connected to the (channel-6-pinned) AP, backend pulling video/control over that Wi-Fi link,
   and the swarm riding the same already-live radio channel underneath it, all the time. The
   no-AP fallback only matters for whichever specific bot actually drops off Wi-Fi.
2. ~~**The swarm can backstop the backend-fetch model.**~~ **Declined (§10 Q4).** Swarm stays
   strictly bot-to-bot — no re-POST-on-behalf-of-a-sibling to the backend, now or later for v1.
   A bot whose own Wi-Fi is flaky simply stays invisible to the dashboard, same as today; the
   swarm link does not compensate for that. Net effect: the backend/dashboard has **zero
   visibility into swarm traffic**, by design — the only place swarm state is observable is each
   bot's own integrated display (§11).

---

## 10. Open questions for you before implementation starts

1. ~~**MAC addresses**~~ — **Answered.** Recorded and filled into §4:
   Guardian `E8:F6:0A:D6:C8:E8`, Pathfinder `E8:F6:0A:D7:91:60`, Warden `94:A9:90:0A:EF:6C`.
2. ~~**Router channel pin**~~ — **Answered.** The `A34` AP already stays parked on channel 6
   whenever Wi-Fi is connected, so §2's happy path holds as-is: no router change needed, and
   `AEGIS_SWARM_CHANNEL` stays `6` to match. Confirmed requirement: when a Master is **disconnected**
   from Wi-Fi (AP unreachable / never configured), it must still bring up ESP-NOW on channel 6 —
   this is exactly the §2.3 fallback (`esp_wifi_set_channel(AEGIS_SWARM_CHANNEL, WIFI_SECOND_CHAN_NONE)`
   with STA up but not associated). So in both states — connected or not — the swarm radio sits on
   channel 6; nothing else in §2 changes.
3. ~~**Wake fan-out behavior**~~ — **Answered.** On receipt of `SWARM_MSG_WAKE`, Pathfinder and
   Warden do **not** react immediately. Each starts a **5-minute delay timer**; if the timer
   elapses with no `SWARM_MSG_ALL_CLEAR`/reset in between, the bot transitions into its scout
   (active patrol) behavior. This is a real state-machine change in Pathfinder's and Warden's
   `main.cpp`/`warden_main.cpp` — out of scope for `swarm_link` itself, which only needs to expose
   the receive event; see §8a below for what the delay timer needs.
4. ~~**Swarm-relayed telemetry (§9)**~~ — **Answered.** Swarm stays strictly bot-to-bot for v1 —
   no re-POST-on-behalf-of-a-sibling. §9.2's proposal is declined; §9.1 (the "no swarm relay to
   backend" framing) is now simply the permanent behavior, not a v1-only placeholder. This also
   means the swarm's only user-visible surface is on-device (§11) — the backend/dashboard has no
   view into swarm traffic at all, by design.

Once these are answered (or you're fine with reasonable defaults), the implementation is
mechanical: one shared header, one new `swarm_link` module × 3, and the small edits listed in §8.

---

## 11. On-device swarm status display (all three integrated LCDs)

Since swarm state never reaches the backend (§10 Q4), the only place it can be observed is each
bot's own screen. Checked what's actually there before designing this:

**Current state, confirmed by reading the code — this is genuinely greenfield, not a new view on
an existing UI:**
- All three boards are ESP32-S3-EYE, physically shipping a 240×240, 16-bit RGB565 ST7789 LCD over
  SPI3 (`esp32_s3_eye.h`'s `BSP_LCD_H_RES`/`V_RES` = 240, `BSP_LCD_SPI_NUM` = `SPI3_HOST`).
- **Guardian** already has the display stack *fetched* — `main/idf_component.yml` line 9 pulls
  `espressif/esp32_s3_eye: ^3.1.0`, which drags in `esp_lvgl_port` and `lvgl` as transitive
  managed components — but it is **not wired in**: `main/CMakeLists.txt`'s `REQUIRES` list doesn't
  include any of the three, and nothing in `main/*.cpp` calls `bsp_display_start()` or any
  `lv_*`/`bsp_*` display function. The BSP exposes `lv_display_t *bsp_display_start(void)`, which
  per its own doc comment initializes SPI + the panel and starts LVGL's own internal handling task.
- **Pathfinder and Warden have no display dependency at all** — no `esp32_s3_eye`/`lvgl`/`ST7789`
  reference anywhere in either project.
- No project has any existing status bar, overlay, or on-screen text today — there's no layout
  convention to conform to, and no render/UI task exists anywhere (confirmed via `xTaskCreate`
  search). What all three *do* have is a periodic low-priority polling task (Guardian's
  `status_task`, `main.cpp:2152`, 5 s; Pathfinder's `telemetry_task`, `main.cpp:372`, 3 s;
  Warden's `telemetry_task`, `warden_backend.cpp:537`) that currently only pushes JSON to the
  backend — same shape as what a display-refresh task would look like, minus the backend part.

### 11.1 Bring-up required per board (prerequisite, before any swarm-display code)

- **Guardian**: add `esp32_s3_eye`, `esp_lvgl_port`, `lvgl` to `main/CMakeLists.txt`'s `REQUIRES`
  (the dependency is already downloaded, just not linked into the build); call
  `bsp_display_start()` once at boot, before `swarm_display_init()`.
- **Pathfinder, Warden**: add `espressif/esp32_s3_eye: ^3.1.0` to each project's own
  `main/idf_component.yml`, mirroring Guardian's existing line — pulls the same BSP/LVGL stack.
  Then the same `bsp_display_start()` call.
- Treat this as its own bring-up step per board — verify with a trivial static label on screen
  before wiring live swarm data into it, especially on Pathfinder/Warden where the LCD has never
  been driven at all. Exact lock/unlock function names (BSP convention is typically something like
  `bsp_display_lock()`/`bsp_display_unlock()` around LVGL calls made from a task other than LVGL's
  own) should be confirmed against the checked-out `esp32_s3_eye` BSP header at implementation
  time — the research pass here only confirmed `bsp_display_start()` by name.

### 11.2 Layout — 240×240, reserved bottom strip (proposed default)

No existing layout to reconcile against, so proposing a simple default, open to change:

- Top ~200 px: left alone for whatever each bot already wants there (camera preview, detection
  overlay, etc.) — out of scope for this doc.
- Bottom 40 px: a fixed 3-cell swarm status strip, **same left-to-right order on all three
  boards** (Guardian | Pathfinder | Warden) so it reads identically no matter which bot you're
  looking at. Each cell:
  - Node label (`GUARDIAN`/`PATHFINDER`/`WARDEN`).
  - Liveness dot: green if heard from within the heartbeat window, gray if silent >15 s (same
    threshold as the `last_seen[3]` table in §6/§8).
  - One line of current alert text if that node has an active, uncleared alert (e.g. `"FIRE"`,
    `"INTRUDER"`, `"WAKE"`), else `"OK"`.

### 11.3 Data flow — swarm_link drives swarm_display, event-driven not polled

- `swarm_handle_packet()` (§7/§8, the single existing chokepoint for all incoming swarm state)
  calls a new `swarm_display_update(node_id, alert_text_or_null)` right after it updates
  `last_seen[]`/alert state — screen refresh piggybacks on state change, no separate poll needed
  for the "a message arrived" case.
- Liveness going gray is silence, not an event, so it can't be triggered from `swarm_handle_packet`
  alone: the heartbeat timer already running in `swarm_link` (§8) also calls
  `swarm_display_update()` every cycle, to re-evaluate the 15 s timeout against wall-clock time
  even when nothing has arrived.
- Any LVGL call made from `swarm_link`'s task (not LVGL's own internal task) needs the BSP's
  display lock held around it — see the naming caveat in §11.1.

### 11.4 New files (extends §8's list)

- `swarm_display.h` / `swarm_display.cpp` × 3 — same shared-then-copied pattern as
  `swarm_link.h`/`.cpp`. Owns the 6 LVGL objects (3 labels + 3 dots), `swarm_display_init()`
  (called once, after `bsp_display_start()`), `swarm_display_update(node_id, alert_text_or_null)`.
- `swarm_link.cpp`: `swarm_handle_packet()` and the heartbeat timer callback each gain one
  additional call to `swarm_display_update()` — small additive edit, consistent with everything
  else in §8.
- Per-project one-time edits: Guardian's `main/CMakeLists.txt` (`REQUIRES`, §11.1); Pathfinder's
  and Warden's `main/idf_component.yml` (new dependency, §11.1); all three call
  `bsp_display_start()` once at boot, before `swarm_link_init()`/`swarm_display_init()`.
