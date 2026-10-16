# Command-bar panels (sidebar report pages)

The 6 command-bar buttons under the minimap each swap the lower sidebar to a report page.
Buttons (left→right) and their **verified** art groups (rendered from op2_art, confirmed by the
user): **Gear 41, Microscope 44, Truck 43, Comms 42, Space 45, Game 40 (green Sierra logo)** →
model `[41, 44, 43, 42, 45, 40]`. Selected button shows its green (frame 1) art.

**Icon → panel (OP2 order):** Gear → **Factories**, Microscope → **Labs**, Truck → **Resources**,
Comms → **Communications**, Spaceship → **Space Program**, Sierra → **Game**. (So `selectedCmd` index
0=Factories, 1=Labs, 2=Resources, 3=Comms, 4=Space, 5=Game — the `titles` array order.)

Status: buttons + green-selected state DONE. **ALL SIX panel layouts are built** — Factories (Gear,0),
Labs (Science,1), Resources (Truck,2), Comms (3), Space (Spaceship,4), Game (Sierra,5). Live data so
far: factory/lab lists, mine/smelter counts, current Mark. Still sim-blocked (Phase 3): production/
research status, economy values, the message log, satellite status, and the panels' nav/action
buttons (up/down/JUMP/STARSHIP).

**Where the QML lives:** `Main.qml` owns the command bar + panel frame/title; the bigger panel
bodies are their own files — `ResourcesPanel.qml`, `SpacePanel.qml`, `DebugPanel.qml` (dev controls
under the Game menu), and `SelectionPanel.qml` (the "7th view"). Each takes the `TilesetView` as
`view` and owns its sub-page state; shared colors/metrics come from the `Theme.qml` singleton.

---

## 1. Truck → "Resources Report"  (selectedCmd index 2)  ✅ (layout built; values mostly sim-blocked)
Colony economy summary via `TilesetView::resourcesReport()` (QVariantMap). OP2 field set + layout
(main row + indented sub-rows), built in `ResourcesPanel.qml` from the map:
- **Colonists: <n>** — Available Workers: <n>, Available Scientists: <n>
- **Morale Level: <n>** — Trend: (Rising/Steady/Falling)
- **Power Use: <used>/<max>** — Unpowered Structures: <n>
- **Food Stored: <n>** — Production: <n>
- **Common Metals: <n>** — Mines: <n>, Smelters: <n>
- **Rare Metals: <n>** — Mines: <n>, Smelters: <n>

**Live now:** the **Mines/Smelters counts** (scanned from the local player's Common/Rare Ore Mines +
Smelters). Everything else (colonists, morale, power, food, stored metals) is **0/placeholder** until
the economy sim. Trend defaults to "Steady".

### Sub-reports — a navigator (`ResourcesPanel.page`)
The Resources Report is a small navigator: the **Summary's lines drill down** into per-category
**Status** pages, and each Status has a **STRUCTURES** list (buildings with live status). Pages built:
- **summary** — the economy overview. Clicking **Colonists** → colonists page; **Morale Level** →
  morale page.
- **colonists** — Colonists Status (Alt+C): Total Population, Residence Capacity, Children, Workers
  (Assigned, Available), Scientists (as Scientists, as Workers, to Research, Available). Sim-blocked.
  Bottom nav: STRUCTURES / SUMMARY.
- **colonistStruct** — Colonists Structures: University / Nursery / Medical Center with **live
  Active/Idle status**, double-click to focus. Bottom nav: DETAILS / SUMMARY.
- **morale** — Morale Status (Alt+M): level + bar, Residence/Rec/Med demands, Net Food, ratios, and
  **Nursery/University/GORF Operational** (these 3 are **derived** — player owns an active one). Rest
  sim-blocked. Bottom nav: STRUCTURES / SUMMARY.
- **moraleStruct** — Morale Structures: the morale buildings (residences, recreation, consumer,
  medical) with **live Active/Idle status**, double-click to focus. Bottom nav: DETAILS / SUMMARY.

Rows render via one Repeater (`main`/`sub`/`sub2`/`op`/`gap` row kinds); structures via a ListView
(shared shape with Factories/Labs). Bottom buttons are contextual per page. **TODO:** the other
Status pages (Power / Food / Common / Rare drill-downs) + Colonists' STRUCTURES list, and the real
data once the economy/population sim lands.

## 2. Microscope → "Labs: Current Research"  (selectedCmd index 1)  ✅ (list built; research sim-blocked)
One entry per lab the **local player** owns, via `TilesetView::labsReport()` (shares the
`buildingReport()` helper with Factories):
- `<Lab name> # <player>-<NN>` / `Researching <topic | Nothing>`  — Basic / Standard / Advanced Lab.
- `Researching Nothing` until the research sim exists. Empty-state: "No labs."
- Same QML ListView as Factories (model switches on the button); rows hover-highlight; **double-click
  → focus + select** the lab. Refreshed on `reportsChanged`.

## 3. Gear → "Factories Report"  (selectedCmd index 0)  ✅ (list built; production sim-blocked)
One entry per factory building the **local player** owns, read live from the World via
`TilesetView::factoriesReport()`:
- `<Factory name> # <player>-<NN>` (NN = per-type instance counter) / `Producing <item | Nothing>`
- Factory types: Structure Factory, Vehicle Factory, Arachnid Factory, Consumer Factory, Spaceport.
- `producing` is **"Producing Nothing"** until the production sim exists. Empty-state: "No factories."
- Rendered by a QML ListView (hover-highlighted rows); refreshed on `reportsChanged` (mission (re)load).
- **Double-click an entry** → `TilesetView::focusUnit(idx, gen)` centres the map on that factory and
  selects it, so the selection panel ("7th view") shows its full info. Each entry carries the World
  handle (`idx`/`gen`) for this.

## 4. Comms → "Communications: Messages"  (selectedCmd index 3)  ◑ (header live; log sim-blocked)
Event log. **Header `<mark>: Current Mark` is LIVE** (from `gameClock.mark`). Below it, a newest-first
ListView over `TilesetView::commsMessages()` ({ mark, text, warn }), the delegate colour-coding
**green = info / red = warning**:
- info: "1 Child born", "1 New Worker Trained", "Morale Level: Good", "Power Levels: Optimal",
  "Food Supply is Growing".
- warning (red): "1 Worker has died", "1 Child has died", "1 Structure Disabled".

The list is **empty until the event/message system exists** (births/deaths/training/disabled/morale/
power/food events) — the rendering is ready, it just needs a message buffer. (OP2's up/down + JUMP
nav buttons at the bottom are TODO — they need messages to act on.)

## 5. Spaceship → "Space Program"  (selectedCmd index 4)  ◑ — two pages (`SpacePanel.page`)
A two-page navigator; the bottom button toggles between them.
- **Satellites** (default) — `spaceReport()` ({ name, operational }): Communications Satellite, Solar
  Satellite → green Operational / red Not Operational. Bottom button: **STARSHIP**.
- **Starship** (via STARSHIP) — `starshipReport()` ({ name, status, done }): the starship-operation
  components — **Ion Drive Module, Fusion Drive Module, Fueling Systems, Skydock** — each with its
  research/build state (green when done, **red until done**). Bottom button: **SATELLITES**.

All sim-blocked: satellites default **Not Operational**, components **Not Researched**, until the
space-program / research sims. (OP2's ▲▼ scroll buttons are TODO; more starship components may exist.)

## 6. Game (Sierra logo) → "Game Options"
A menu of 4 buttons (each drawn with the OP2 button graphic, **art group 50**: frame 0 normal,
frame 1 pressed), stacked vertically with the label overlaid:
- **MISSION OBJECTIVES** — show the mission briefing (from the `.opm` LevelDetails). TODO action.
- **ONLINE MANUAL** — open the manual/wiki. TODO action.
- **PREFERENCES** — game options (outpost2.ini). TODO action.
- **END MISSION** — end/quit. TODO action.
Self-contained (no sim data needed) → built first.

Below the four buttons, the **developer/debug controls** (game speed, scroll, render toggles,
Export PNG/Map, scene-load buttons) live in `DebugPanel.qml` (a scrollable `Flickable`) — this is
their home now that the panel fills the lower sidebar. They show only on the Game menu
(selectedCmd === 5).

---

**Data dependency:** panels 1–5 are mostly blocked on Phase-3 sim systems (economy, research,
production, messages, space). Build order suggestion: a shared panel frame + header styling
(green-on-black, the OP2 look) first, wired to whatever data exists, then fill each as its sim
system lands. The Game panel (6) is the most self-contained (mission objectives come from the
.opm `LevelDetails`/briefing; options already live in outpost2.ini).
