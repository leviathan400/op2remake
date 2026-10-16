# Dev Log

Running notes of what we've built. Newest entries on top.

---

## 2026-06-10 — Pre-Phase-3 refactor: sim command layer, shared building-art resolver, modular QML

Three structural fixes from a code review, done now so the sim lands on clean seams:

- **Sim command layer** (`sim/include/op2/sim/command.h` + `src/command.cpp`) — the render/UI
  layer is now **read-only over the `World`**. Every gameplay mutation is a `CommandPacket`
  applied by `op2::sim::applyCommand`, exactly as a network peer's packet would be:
  - IDLE/ACTIVATE (detail panel) + the status click-toggle build real **ctMoIdle/ctMoUnidle**
    packets (payload = the target's `index`+`generation` as LE u32s — the generation makes a
    command aimed at a destroyed-and-reused slot detectably stale).
  - Rejected packets (stale handle, malformed payload, unknown opcode) change **nothing** —
    validation is sim-side so every lockstep peer rejects identically.
  - Condition-**Disabled** isn't a player command in OP2 (the sim derives it from power/
    workers/damage/EMP), so the debug toggle goes through an explicit `debugSetDisabled`
    backdoor *inside* the layer — the "only this module mutates the World" rule holds.
  - `TilesetView`: `toggleBuildingAt`/`setSelectedIdle` now share one `setObjectStatus`
    helper that issues the command and refreshes the cached render flags **from** the World
    result (they can no longer drift). World-contract preamble added to `tilesetview.h`.
  - **+14 self-test checks** (now **78**): apply/clear, JSONL round-trip of a real packet,
    stale/unknown/malformed rejection, debug backdoor. New concept note: CONCEPTS.md §6.
- **One building-art authority** (`TilesetView::resolveBuildingArt`) — the per-faction
  Normal/floor/lastFrame/ambient resolution incl. the **Garage/LightTower special cases**
  was duplicated between the showcase (`placeAllBuildings`) and the mission render
  (`placeFromWorld`) and could silently drift. Now both paths call the one resolver
  (in `tilesetview_showcase.cpp`); a new special case lands in both automatically.
- **Main.qml split** (1511 → 564 lines): the window/layout shell stays in `Main.qml`; the
  panel bodies are their own files — `DebugPanel.qml` (dev controls, with inline
  `ToggleLine`/`Stepper`/`DevButton` components replacing ~20 hand-rolled toggle/button
  blocks), `ResourcesPanel.qml` + `SpacePanel.qml` (each **owns its sub-page state**,
  exposes `title`; Main resets pages on command change), `SelectionPanel.qml` (the 7th
  view), and `RedNavButton.qml` (the OP2 red-bordered nav button). **`Theme.qml`
  singleton** centralises the OP2 palette (the measured `#00ff00` panel green, `#7fbf4f`
  HUD green, reds, gold) + HUD metrics — retune a colour in one place. qmllint: no errors.
- Comment passes from the review: GameClock's fractional-tick accumulator (why floor+carry
  keeps the long-run rate exact), minimap fit/inverse transforms, the ui-provider colour-key
  alpha-OR trick, art-provider edge cases.
- Docs refreshed: README (stale check count + layout tree), STATUS (command layer, modular
  QML, 78 checks), ROADMAP (counts, Phase-5 input→packets started, status footer),
  GAME_OBJECTS (UnitType/renderer items were done; command layer added), SELECTION_PANEL,
  COMMAND_PANELS (QML file map), CONCEPTS (§6 + glossary).

---

## 2026-06-04 — Selection panel pixel-matched to OP2 reference

- **Measured** OP2's selection panel from `ui/reference/agridome_panel.png` (192×215; our sidebar
  inner ~196). Findings → fixes:
  - **All panel text is pure `#00ff00`** (OP2's GDI rendering is non-antialiased, so every glyph
    pixel is exactly green). The **armor-class line was wrongly muted gray → fixed to `#00ff00`**.
  - **HP layout**: OP2 shows the value as **text above a thin (~6px) bar**, not centred on a thick
    bar → reworked (value text `#00ff00` + 6px bar; was 13px bar with white number).
  - **Sizes confirmed**: title Arial ~13px bold, body ~11px, upgrade note ~10–11px (matches ours).
  - **Row pitch** tightened (`rowSpacing` 3→1) toward OP2's ~11–12px.
- `docs/UI_REFERENCE.md` updated with the measured numbers + ours-vs-OP2 table. Still missing (sim/
  tech-blocked): the upgrade note, Production / Production Level / In Storage rows.

---

## 2026-06-04 — OP2 UI font identified + matched (Arial via GDI)

- Decompile finding: OP2's in-game UI font is **Arial**, not a bespoke bitmap font. The `Font` class
  wraps a Windows `LOGFONT` + `GLYPHMETRICS[256]` and **rasterises Arial through GDI**
  (`CreateFontIndirectA` + `GetGlyphOutline`) into a cached 8-bit glyph buffer. Three fonts: Arial
  **13px regular** (body), **13px bold**, **15px bold** (headers). Face string @ `0x004deb40`;
  `Font.DrawString` @ `0x004162b0`. Full note: `Review/findings/RENDERER.md`.
- Matched it in the remake: `main.cpp` sets `QGuiApplication::setFont("Arial")` +
  `QQuickWindow::NativeTextRendering` (GDI rasteriser) → the whole UI now has OP2's crisp pixel look
  instead of Qt's antialiased default.

---

## 2026-06-05 — OP2 delayed-hover (corner box + HP bar + name tooltip)

- Hovering an object now reproduces OP2: after a **~0.9s dwell**, a **light corner-bracket box** +
  a **green HP bar** are drawn around it (in `paint`, map-space, follows the camera) and a
  **green-on-black name tooltip** appears at the cursor (QML overlay). Moving to another object
  restarts the dwell; moving off / leaving the view hides everything.
- `TilesetView`: a single-shot `hoverDelayTimer_`, hover-object tracking in `updateHover` (UnitId,
  name, box, hp), `paintHover()` for the box+bar, and `hoverObjActive`/`hoverObjName`/`hoverCursorX/Y`
  properties for the tooltip. The status-bar "Name HP" readout is unchanged.
- **Pixel-tuned** against `ui/reference/on-hover.png` (~2.3× scale): HP bar pure `#00ff00`, ~6px tall
  with a 1px black border, ~55% of the box width, centred; reticle box inflated ~4px outside the
  footprint; tooltip confirmed as 13px green Arial on black with a 1px green border (already matched).

---

## 2026-06-04 — Space object costs parsed into memory + name mapping resolved

- **Resolved the SPACE_n → display-name mapping** authoritatively from the **EXE object-name pointer
  table** (Outpost2.exe @ VA `0x4E9638`), whose entries are in exactly space.txt's order — cross-checked
  against each row's Research Topic in the tech tree. Corrected the guesses: SPACE_15=Children Module,
  LUX_1/2/3=Impulse Items / Wares / Luxury Wares (Consumer Factory goods). Full table in
  `Review/findings/SPACE_PROGRAM.md`.
- **Parsed space.txt into memory**: `populateSpaceObjects()` builds `spaceObjects_`
  (`std::vector<SpaceObject>{code, name, commonCost, rareCost, buildTime, research}`, 23 entries) +
  a name→index map; `spaceObject(name)` looks up costs. Confirmed at load ("23 space objects loaded").
- The Space Program **Starship page now shows real costs** (Common / Rare ore + Build) per component
  from the loaded data. Ready to feed a build/launch menu.

---

## 2026-06-04 — Load space.txt + space-object name finding

- Loaded **`space.txt`** into `TilesetView::spaceSheet_` (alongside building/vehicles): per-type data
  for space objects — Common/Rare ore cost, Build Time, Research Topic. Codes are generic (EDWARD,
  SOLAR_SAT, SPACE_1..15, SULV, RLV, EMP_MISSILE, LUX_1..3).
- Decompile finding: the **display names** aren't in space.txt — they're in the EXE `.data`
  (Ion Drive Module @0x4e4234, Skydock @0x4e41d8, Solar Satellite @0x4e4248, EDWARD Satellite
  @0x4e4258, Communications Satellite @0x4e656c, etc.). Full table + space.txt schema in
  `Review/findings/SPACE_PROGRAM.md`. Next: SPACE_n→name mapping + wire costs/research into the
  launch menu.

---

## 2026-06-04 — Space Program: Starship page

- The Space Program is now a **two-page navigator** (`root.spacePage`): **Satellites** (default) ⇄
  **Starship** (via the STARSHIP button; SATELLITES back). The Starship page lists the
  starship-operation components — Ion Drive Module, Fusion Drive Module, Fueling Systems, Skydock —
  with their research/build state (`starshipReport()`; green when done, red until done; all
  "Not Researched" until the research sim). Bottom toggle button (OP2 red-bordered style).

---

## 2026-06-04 — Visual doc: full in-game menu structure

- Added `docs/OP2_MENU_STRUCTURE.md` — a visual reference of the entire in-game sidebar UI: the 6
  command-bar buttons + their panels, the Resources Report drill-down navigator (Summary → Status →
  Structures), the Communications/Space/Game panels, and the object selection "7th view". Includes a
  **Mermaid hierarchy diagram**, per-menu field breakdowns, the data-status legend (live/partial/sim/
  TODO), hotkeys (Alt+C/Alt+M), and the measured fonts/colours. Flags what still needs OP2 screenshots
  (Power/Food/metals drill-downs, nav/action buttons).

---

## 2026-06-04 — Resources Report navigator (Summary drill-down + Morale)

- Reworked the Resources Report into OP2's **navigator** (`root.rPage`): the **Summary's lines drill
  down** (click Colonists / Morale Level) into per-category **Status** pages, each with a **STRUCTURES**
  list and contextual bottom buttons (Status: STRUCTURES/SUMMARY; Structures: DETAILS/SUMMARY).
- Pages built: **Summary**, **Colonists Status (Alt+C)**, **Morale Status (Alt+M)** + **Morale
  Structures**. New `TilesetView::moraleReport()` (level/demands sim-blocked, but **Nursery/University/
  GORF "Operational" are DERIVED** — player owns an active one) and `moraleStructures()` (morale
  buildings with **live Active/Idle status**, double-click to focus, via a shared `structureList()`
  helper). One Repeater renders rows (`main`/`sub`/`sub2`/`op`/`gap` kinds); structures via a ListView.
- **Colonists Structures** added (`colonistStructures()` — University/Nursery/Medical Center with live
  status): Colonists Status → STRUCTURES → list → DETAILS back. The structures ListView is now shared
  (model switches on the page).
- TODO: the Power/Food/Common/Rare Status drill-downs; real values (economy/population sim).

---

## 2026-06-04 — Space Program panel — all six command panels now built

- Built the **Space Program: Satellites** panel (Spaceship, index 4): one entry per satellite via
  `TilesetView::spaceReport()` ({name, operational}) — green label + indented status, **green
  Operational / red Not Operational**. Defaults to Not Operational until the space-program sim.
- **All six command-panel layouts are now built** (Factories, Labs, Resources, Comms, Space, Game).
  The dead "(waiting on the simulation…)" placeholder is now an unreachable guard.

---

## 2026-06-04 — Communications panel (Comms) — live mark header + ready log

- Built the **Communications: Messages** panel (Comms, index 3): the **`<mark>: Current Mark` header
  is live** (from `gameClock.mark`); below it a newest-first ListView over
  `TilesetView::commsMessages()` ({mark, text, warn}) whose delegate **colour-codes green=info /
  red=warning**. The message list is **empty until the event/message system exists** — rendering is
  ready, just needs a buffer. Empty-state note shown. (OP2 up/down + JUMP nav buttons TODO.)

---

## 2026-06-04 — Resources Report (Truck) layout

- Built the **Resources Report** panel (Truck, index 2) in OP2's layout — main rows + indented
  sub-rows: Colonists / Workers / Scientists, Morale / Trend, Power Use / Unpowered, Food / Production,
  Common & Rare Metals / Mines / Smelters. `TilesetView::resourcesReport()` (QVariantMap).
- **Live now:** the **Mines/Smelters counts** (scanned from the local player's Common/Rare Ore Mines +
  Smelters). The rest (colonists, morale, power, food, stored metals) is 0/placeholder until the
  economy sim. Built from a QML row-model `Repeater`.

---

## 2026-06-04 — Labs Report + command-panel reassignment

- **Labs Report** (Science button) built — lists the local player's labs (Basic/Standard/Advanced)
  with "Researching Nothing", same as Factories. Refactored the shared logic into
  `TilesetView::buildingReport(displayMap, verb)`; `factoriesReport()`/`labsReport()` call it. One QML
  ListView drives both (model switches on the button); rows hover-highlight; double-click focuses.
- **Command-panel reassignment** (user-corrected OP2 order): Gear → **Factories**, Truck →
  **Resources** (swapped); Science → Labs, Comms, Spaceship → Space, Sierra → Game unchanged. The
  `titles` array + the report ListView gating updated accordingly.

---

## 2026-06-04 — Factories Report (first report panel with live data)

- The **Truck → Factories Report** panel (command index 2) now lists the **local player's factory
  buildings** (Structure/Vehicle/Arachnid/Consumer Factory, Spaceport) read live from the World:
  `<Display Name> # <player>-<NN>` (NN = per-type instance counter) + a "Producing Nothing" status
  (production is still sim-blocked). `TilesetView::factoriesReport()` (QVariantList property,
  `reportsChanged` on mission (re)load); QML ListView. Empty-state "No factories." First of the 6
  report panels to show real data; the other four await their sims.
- **Double-click a factory** in the report → `TilesetView::focusUnit(idx, gen)` centres the map on it
  and selects it (selection panel shows its full info). Entries carry the World handle; rows
  hover-highlight.

---

## 2026-06-04 — Selection panel: OP2 green + category corrections

- Panel restyled to OP2's **`#00ff00`**: object-name title, all stat labels/values, and the HP-bar
  fill. Status keeps its state colour (Active green / Idle gray / Disabled red); the armor-class line
  stays muted (as in OP2); the Debug section stays intentionally dimmer (it's not part of OP2).
- Category-icon corrections (user): Trade Center + Light Tower join Command Center as **faction-logo**
  (Eden/Plymouth); University + ConsumerFactory → Colonists; MedicalCenter → Emergency; ArachnidFactory
  → Tech_Arachnid (file pending); vehicles temporarily → Tech_Weapon (per-vehicle TODO).
- Tech icons draw at **native 45×45** (no scaling) inside the group-54 box.
- Full panel reference: `docs/SELECTION_PANEL.md`.

---

## 2026-06-04 — Object category icons in the portrait box

- Each object gets a **category** (hardcoded in `techIconStem`, since it's not in building.txt) →
  a portrait icon from **`ui/tech/Tech_*.png`** (45×45 opaque tiles, drawn as-is, no color-key).
  GuardPosts → Tech_Weapon; **faction-logo buildings** (Command Center, Trade Center, Light Tower) →
  Tech_Eden / Tech_Plymouth by faction; labs → Tech_Science; University/ConsumerFactory → Colonists;
  MedicalCenter → Emergency; ArachnidFactory → **Tech_Arachnid** (icon file pending); plus
  power/metal/food/colonists/vehicle/space categories. Unknown buildings → Tech_Strcture. **Vehicles
  temporarily all → Tech_Weapon** (per-vehicle categories TODO). Drawn centred (48×48) in the group-54 box.
- The icon loads from file (`file://<uiAssetsDir>/tech/<stem>.png`), so icons are customisable;
  `config.uiAssetsDir` exposed to QML. Several categories are best-guess (see code) — easy to retune.

---

## 2026-06-04 — File-based (customisable) UI buttons + portrait box

- The selection panel now shows OP2 **art group 54** (the 60×60 portrait box) top-left, with armor
  class + HP bar to its right (OP2 layout). An icon slot inside the box is wired (`iconSource`,
  empty for now) ready for per-object-type icons.
- **Buttons now load from files** instead of op2_art, so they're customisable: the `ui` provider
  gained a second **common assets dir** (`[UI] AssetsDir`, default `ui/`) searched after the skin
  dir, and it accepts any extension (.png/.bmp). The long button (group 50) is now
  `image://ui/group50-0.png` / `-1.png` (color-keyed `#141414` like the HUD pieces). Exported PNGs
  live in `ui/` (`group50-*`, `group51-*`).

---

## 2026-06-04 — Building requirements (power/workers/scientists) + panel tidy

- `UnitType` gains **powerRequired / workersRequired / scientistsRequired**, read in `populateType`
  from building.txt's "Power required" / "Workers required" / "Scientists required" columns (0 for
  vehicles). The selection panel now shows these (0 → "None", like OP2) instead of `—` placeholders;
  only live usage + storage remain sim-blocked.
- Selection panel tidy: **Kind moved into the Debug section**, which also now shows the **object ID**
  (World `UnitId` = index + generation) and **Location** (1-based tile coords).

---

## 2026-06-04 — Mine survey functions (Robo-Surveyor + EDWARD Satellite)

- The old left-click-to-survey is gone (left-click now selects), so the single-mine survey code was
  promoted to a proper function: **`TilesetView::surveyMineAt(tileX, tileY)`** (Q_INVOKABLE, returns
  bool) — reveals one mining beacon's ore type + yield, i.e. a **Robo-Surveyor** reaching its tile.
- New **`surveyAllMines(player)`** (Q_INVOKABLE, returns count) — reveals **every** mining beacon at
  once: deploying an **EDWARD Satellite**. (Beacons are Gaia/unowned and we render one player's view,
  so it reveals globally; the `player` arg is reserved for future per-player visibility.)
- Both rebuild via `placeResources()`. Added a dev **"Survey Mines (EDWARD)"** button under the
  Game-menu debug controls to test it (shows the count revealed). Right-click now deselects.

---

## 2026-06-04 — Fix command-bar icon order

- Rendered art groups 40–45 to verify them: **41 Gear, 44 Microscope, 43 Truck, 45 Comms
  (dish/tower), 42 Space (satellite probe), 40 Game (green Sierra leaf)**. The old model
  `[41,44,43,42,40,45]` had the last three scrambled (satellite in the Comms slot, the green Game
  logo in the Space slot, the comms dish in the Game slot). Corrected to **`[41,44,43,42,45,40]`**
  (user-confirmed: 42=Comms, 45=Space) so each icon matches its panel. `docs/COMMAND_PANELS.md`
  mapping updated.

---

## 2026-06-04 — Object selection + detail panel (the "7th view")

- **Left-click selects** the object under the cursor in the game view (building → GuardPost →
  vehicle, topmost first); clicking empty ground deselects. A green **selection box** is drawn
  around it (footprint for buildings, 1×1 cell for units). Right-click stays the dev
  condition-Disabled toggle.
- New **selection detail panel** in the sidebar (shows when something's selected and no command
  report is open): **title** `Name #player-index`, **armor class + HP bar** (cur/max), **Status**
  (Active/Idle/Disabled, coloured), **Kind**, **Weapon** (GuardPosts/combat units), and an **IDLE
  / ACTIVATE** action button (toggles the structure's player-Idle flag, written back to the World).
  Power/Workers/Scientists/Storage are `—` placeholders pending the economy sim.
- Plumbing: `TilesetView::selectAt/clearSelection/setSelectedIdle`, `selectedInfo` (QVariantMap) +
  `selectedValid` properties, tracked by the **stable World handle** so it survives a re-render
  (anim toggle, mine survey, etc.). `weapon` stored on placed GuardPosts/combat units.
- TODO: map the sheet's "Armor Class" number to OP2's name (e.g. "Heavy Armor") — the panel shows
  the raw class number for now; vehicle command-icon grid; BUILD/STORAGE factory actions.

---

## 2026-06-04 — Interface skins: draw real OP2 HUD bitmaps (color-keyed)

- New **`ui` image provider** (`render/uiimageprovider.{h,cpp}`): loads OP2 interface (HUD) bitmaps
  and **color-keys the `#141414` background (palette index 0) to transparent**, so the masked areas
  let the game view / sidebar show through instead of blocking them. Usage:
  `Image { source: "image://ui/<name>" }` (`<name>` = file stem).
- The interface is now a **skinnable layer**: `[UI] SkinDir` in `outpost2.ini` points at a directory
  of `Bitmap_*.bmp` files — **the user's own demo or retail interface graphics** (nothing bundled in
  the binary, same rule as `op2_art`). Empty = hand-drawn frame only.
- Pieces drawn so far: **`Bitmap_170`** (468×48, **bottom-right corner**) and **`Bitmap_169`**
  (**bottom-left corner**, wired in QML — renders once its file is added to the skin dir). Both
  fixed-size, anchored to their window corner.
- **Layout model decided: stretchable borders** (OP2's own resize model) — corners fixed-anchored,
  edge/border pieces stretched along their axis, sidebar/minimap/command-bar strip native-size
  anchored right, game view fills the middle. Resolution-independent; one layout drives both skins
  (demo and retail bitmaps are the same sizes).
- **`[UI] UseSkin`** master toggle (default 1): `0` ignores the HUD bitmaps and uses the original
  hand-drawn frame (pre-skin look) without clearing `SkinDir`. QML gates pieces on
  `root.hudSkin = config.useSkin && config.uiSkinDir !== ""`. Skin switched from demo → **op2 (retail)**.
- New divergence **D2** (skinnable HUD supporting demo + retail art) in `DIVERGENCES.md`; design +
  piece map + layout model in `docs/INTERFACE_SKINS.md`.

---

## 2026-06-04 — Debug controls moved under the Game menu

- The dev/debug controls (game speed, scroll, render toggles, Export PNG/Map, scene loaders) moved
  into the **Game Options panel below END MISSION**, in a scrollable `Flickable` — the command panel
  fills the lower sidebar, so they live there now. Shown only on the Game menu.

---

## 2026-06-03 — Command panels: lower-sidebar report pages + the Game menu

- Selecting a command button now swaps in a **panel** below the command bar (OP2's lower-sidebar
  report pages), drawn over the dev controls; deselect to hide. Title per button: Resources
  Report / Labs / Factories / Communications / Space Program / Game Options.
- **Game Options panel built** (the self-contained one): 4 buttons — MISSION OBJECTIVES, ONLINE
  MANUAL, PREFERENCES, END MISSION — drawn with the **OP2 button graphic (group 50)** via the
  `op2art` provider (frame 0 normal / 1 pressed). Actions are TODO stubs for now.
- The other 5 panels show their title + a placeholder ("waiting on the simulation") — they're
  reports of economy/research/production/messages/space state that Phase-3 will provide. Full
  field spec in `docs/COMMAND_PANELS.md`.

---

## 2026-06-03 — Command bar: OP2's 6 menu buttons under the minimap

- Added the **6 command-bar buttons** beneath the minimap (art **groups 40–45**, fixed 42px tall).
  Each group has 3 frames: **0 = normal, 1 = green (clicked/selected), 2 = disabled**.
- Clickable via the existing `op2art` QML image provider — clicking selects a button (shows its
  green frame); clicking it again deselects. Radio-style `root.selectedCmd` (−1 = none). The menu
  *panels* they open (Mission Objectives, etc.) are TODO; the disabled (frame-2) state is wired in
  the model for unit-only missions but not yet triggered.
- Button order matches OP2 left-to-right: **Gear, Microscope, Truck, Comms, Starship, Game
  (Sierra logo)** = art groups **[41, 44, 43, 42, 40, 45]** (raw group order is scrambled vs
  display order). These map to the menu panels each opens (TODO).

---

## 2026-06-03 — Unsurveyed mining beacons + survey-to-reveal; bar-count fix

- Mining beacons now start **UNSURVEYED**, shown as the **animated white "pre-surveyed" mine**
  (**group 1755**, multiple frames; 18551 is its first). Type/yield unknown until surveyed, as in OP2.
- `MissionResource.surveyed` (default false) drives it; `placeResources` shows group 1755 when
  unsurveyed, the ore-type/yield group when surveyed. Vents/fumaroles/markers/wreckage unchanged.
- **Survey-to-reveal:** left-click an unsurveyed beacon to survey it (stand-in for a Robo-Surveyor)
  → it switches to its real ore type + yield. `PlacedSprite` gained cell + a resource back-link.
- **Bar-count fix:** had Bar1/Bar3 swapped. Verified by frame inspection — bar count = sprite
  width: 1 bar = 8px (g1749/1752), 2 = 13px (g1750/1753), 3 = 18px (g1751/1754). So Bar3 (High,
  3 bars) = frame 18503/18539, Bar1 (Low, 1 bar) = 18480/18515.
- **mines.txt decoded** (`OPU/sheets/`): YIELD (High/Med/Low = 3/2/1 bars) × VARIANT (1/2/3) → 9
  configs with economic params (initial/peak/min yield %, truckloads). 6 surveyed graphics
  (ore × bar) with per-bar variants; variant-exact frame selection + mine economics = later.
  Ref: forum.outpost2.net topic 6450.
- Next: drive surveying from a real Robo-Surveyor reaching the beacon (sim), not a click.

---

## 2026-06-03 — Fix unit hover HP; clarify the sheets are OP2's

- Hovering a **vehicle** showed no HP (buildings/GuardPosts worked). Cause: the vehicle render
  path didn't set `PlacedUnit.sourceId`, so hover couldn't reach the World object. Set it — now
  hovering a Lynx/truck/etc. shows "Name HP cur/max" too.
- Terminology: `OPU/sheets/*.txt` are the ORIGINAL **OP2 sheets** (unpacked from the game's `.vol`
  by the OPU 1.4.1 installer), not OPU-specific data — corrected the code comment + memory.

---

## 2026-06-03 — GuardPost idle rotation: stop the lockstep look

- A field of GuardPosts looked like it rotated in unison. Cause wasn't the beat (that's already
  hash-decorrelated per unit) — it was the **re-center target**: turrets drift back toward the
  *hull*, but every object in a mission faces the same way (East), and a GuardPost's whole sprite
  rotates (no static chassis to mask it), so they all settled on East together.
- Fix: a GuardPost now drifts toward a **stable per-unit idle "home"** direction (separate hash
  bits from its beat/mode) instead of the shared hull, so a row settles at varied angles and reads
  as independent. **Vehicles keep hull re-centering** (the correct OP2 look — turret returns to
  forward). One line in `advanceUnitTurrets`.

---

## 2026-06-03 — Real UnitType table (per-type HP/footprint/armor/kind) + vehicle HP

- Added a per-type **`UnitType` table to the `World`** (`setType`/`type`, indexed by typeId):
  kind, footprint, **maxHitPoints**, **armor**. Built once per type at load from the catalog +
  sheets; the single source of truth (NOT serialised/checksummed — it's derived input).
- **`TilesetView::populateType`** classifies (building catalog incl. GuardPost → Building, else
  Vehicle), sets footprint, and pulls HP+armor from the right sheet. Spawn now reads
  `kind`/`hitPoints` from the type table — no per-spawn or per-query sheet re-lookup.
- **Vehicle HP wired** (`vehicles.txt`): name→Code_Name map (Lynx→LIGHT_TANK 300, Panther→MED_TANK
  500, Tiger→BIG_TANK 800, RoboMiner→MINER, GeoCon→GEO_METRO, Spider→SPIDER 100, …). InterColony
  Shuttle is a space unit (not in vehicles.txt) → 0 for now.
- **Hover** now reads HP from the World object's `UnitType` (not a sheet re-lookup) and covers
  **vehicles + GuardPosts + buildings** uniformly (hit-tested via the World cell). So you can hover
  a Lynx and see "Lynx  HP 300/300".
- Self-test still green (66 checks); `World::clear` also clears the type table.

## 2026-06-03 — Structure HP from building.txt onto objects (first sheet→sim wiring)

- Pulled **hit points from the stat sheet** (`OPU/sheets/building.txt`) and stamped them on each
  spawned structure: `Unit.hitPoints = maxHP` at full health. (CommandCenter 2500, GuardPost 500,
  etc. — verified vs the OP2 info panel.) First time a stat sheet feeds the World objects.
- `building.txt` keys on internal **Code_Names** (COMMAND, TOWER_GUARD, ASE…), not our catalog
  names — added a 1:1 name→Code_Name map (`kBuildingCodeName`), correlated by footprint (each
  footprint class matched exactly). `TilesetView::buildingMaxHp(name)` resolves it via the loaded
  `op2::sim::Sheet`. GuardPost (a structure) gets its HP from building.txt too.
- Surfaced for verification: **hovering a building shows "Name  HP cur/max"** in the status bar
  (`hoverInfo`) — a stopgap until a real selection/info panel exists.
- Next: same wiring for vehicles (`vehicles.txt`), and store maxHP on the type (UnitType) rather
  than re-looking-up per query.

---

## 2026-06-03 — Manageability: split the 1,633-line tilesetview.cpp; ROADMAP refresh

- `render/tilesetview.cpp` had grown to 1,633 lines (~¼ of the codebase) holding five
  responsibilities. Split into **4 translation units of the same class** (header unchanged,
  no API change, behaviour-identical):
  - `tilesetview.cpp` (672) — view lifecycle, loadMap, paint/camera/input, setters, exports.
  - `tilesetview_showcase.cpp` (374) — the catalog parade (placeAllBuildings/Units).
  - `tilesetview_mission.cpp` (488) — `.opm` → World render (placeFromWorld, GuardPosts, resources).
  - `tilesetview_anim.cpp` (173) — animation advance (buildings, turrets, sprites).
  - Shared `constexpr T` duplicated per TU; `kMinLoopTicks` + the GP tables / unit-name helpers
    moved to the TU that uses them. Clean rebuild verifies every method is defined exactly once.
- **ROADMAP refreshed** (was stale at 2026-06-01): self-test count, `.opm` now loads into the
  World, Phase-4 rendering largely done + draws-from-sim, object-property model pulled into
  Phase 3, current-status rewrite. STATUS now links DIVERGENCES.md.

---

## 2026-06-03 — First intentional divergence documented (DIVERGENCES.md)

- Started **`DIVERGENCES.md`** to track deliberate deviations from OP2. First entry **D1**:
  a GuardPost **stops animating when Idle or Disabled (any reason)**. OP2 is inconsistent here
  (an idle or power-disabled GP keeps its turret wandering; only an EMP'd one freezes); we make
  it uniform — not operating ⇒ no animation — matching how main structures behave. No code change
  (our `advanceUnitTurrets` already gates GP wander on `active = !idle && !disabled`); doc only.

---

## 2026-06-03 — Structure status: Idle and Disabled are INDEPENDENT

- Corrected the structure-status model to match OP2: **player-Idle** and **condition-Disabled**
  are two **orthogonal** flags, not one cycle. A structure can be *not* player-idled yet
  Disabled (e.g. the DIRT in the OP2 ref: "Disabled-Workers" — no animation, red light, but the
  player never idled it). Already have both bits: `UnitFlag::Idle` / `UnitFlag::Disabled`.
- A structure **operates** (animates + green light) only when **neither** flag is set. Light
  precedence: **player-Idle → gray** (player intent wins, no false alarm), else
  **Disabled → red**, else **green**. GuardPosts: no light when active; gray if idled, red if
  disabled (centred). Both flags drive the same "not operating" → no animation.
- `disabled` will later carry the **reason** (no power / workers / scientists / damage / EMP) for
  the status text — the light stays red for all of them.
- Interaction: **left-click toggles Idle, right-click toggles Disabled** (independently), both
  written back to the World object. (Was a single Active→Idle→Disabled cycle.)

## 2026-06-03 — Objects carry a UnitKind (Building / Vehicle / …)

- Every `Unit` now has a **`kind`** (`UnitKind` enum: Building / Vehicle / Weapon / Other) — a
  real per-object property for gameplay rules (e.g. a weapon that only damages structures),
  selection, and AI. Stored per-instance for now (moves to the `UnitType` table later).
- Threaded through `foldInto` (checksum) + `serialize`/`deserialize` (**save format v1 → v2**).
- Populated at `.opm` load: anything in the building catalog (incl. **GuardPost**) → `Building`,
  else `Vehicle`. So GuardPost is correctly classified as a **structure** (it's rendered via the
  turret path only for compositing — it doesn't move).
- Self-test: +2 checks (kind stored per instance, kind survives save/load) — all pass.
- Design note: status is interpreted per-kind, but **afflictions cross kinds** — `IsEMPed`/
  `IsStickyfoamed` aren't vehicle-only: **EMP disables buildings too** ("Disabled-EMP"), and both
  buildings + units show the electric-arc visual. So for a structure EMP is a Disabled *reason*;
  the arc/foam is a shared render effect. One `flags` field, interpreted by `kind`, as in OP2.

---

## 2026-06-03 — GuardPost status overlay (off = centred gray dot)

- GuardPosts (weapon-bearing buildings, rendered as units) now carry the status property and
  show the overlay — but per OP2 they're the opposite of buildings: **no light when Active**;
  when **off** they get the gray "off" dot (**g49**) **centred** on the post (red **g48** if
  disabled by damage). Own/allied only, like buildings.
- `PlacedUnit` gained the status fields (active/disabled/friendly/sourceId/centerX,Y/isGuardPost);
  `placeFromWorld` stamps them from the World flags; an off GuardPost stops its turret wander
  (`advanceUnitTurrets` skips it). **Click cycles** a GuardPost Active→Idle→Disabled too
  (`toggleBuildingAt` now also hit-tests the 1×1 GuardPost units).

## 2026-06-03 — op2_art sprites exposed to QML (image provider); OP2 button loaded

- New `Op2ArtImageProvider` (QQuickImageProvider, registered as `op2art`) lets QML show any
  op2_art sprite: `Image { source: "image://op2art/<group>/<frame>" }` (frame optional → 0).
  Holds its own `ArtFile` (the BMP blob is only ~7 MB) and renders on demand. Wired in `main.cpp`
  from the `[Game] Outpost2` base path.
- This makes the **OP2 button graphic (group 50)** available — `image://op2art/50/0` (normal) /
  `image://op2art/50/1` (pressed) — ready to skin the UI buttons whenever we choose. Plumbing
  only for now; nothing restyled yet.

## 2026-06-03 — Overlay light only on own + allied buildings (ownership)

- OP2 only lights the computer overlay on **your own and allied** objects. Implemented: the
  `.opm` loader now reads each player's **IsHuman** + **Allies** → derives the **local player**
  (first human) and the **friendly set** (local + allies). `PlacedBuilding.friendly` is stamped
  from the building's owner; the status light draws only when friendly. (Enemy buildings show no
  light.) Non-mission parade = all friendly. `opu_12.opm` is 2 humans / no allies, so with local
  player 0 only the Eden side lights up.

## 2026-06-03 — Status light: Idle shows the gray "off" dot

- Idle buildings now show the **gray "off" dot** (art **group 49**) instead of nothing — so the
  green light visibly "goes out" rather than vanishing, matching OP2's idled look.
- Corrected the light-group mapping: **47 = green (Active)**, **49 = gray (Idle/off)**, **48 =
  red, 8-frame FLASHING (Disabled by damage)** — frame 0 shown solid for now, animate later.
  (Previous entry had red on g49; g49 is actually the gray off-dot.)

## 2026-06-03 — Status light: red (Disabled-by-damage) state added

- The overlay light is now **3-state**: green = Active, **red = Disabled (by damage)**, none =
  Idle. Red uses **art group 49** (the solid single-frame red light); group 48 is the 8-frame
  **flashing** version, reserved for when HP/damage status exists (then we animate it).
- `UnitFlag::Disabled` (precedence over `Idle`) drives the red light; `PlacedBuilding.disabled`
  mirrors it. Both Idle and Disabled hold the base frame (no animation).
- Click now **cycles** a building Active → Idle → Disabled → Active, so all three light states
  are testable before the sim drives them. Each light variant is drawn at the same selection-box
  point, offset by its own anchor.

## 2026-06-03 — Building "computer overlay" status light (green = Active)

- Added OP2's building status light — the small **green dot** at each building's top-left. It's
  a fixed art sprite (**group 47**, a 10×10 green marker) drawn at the building's **selection-box
  top-left + (2,2)** — decoded from `op2art.dll`'s lights branch (`mem_group::drawFlags`, via the
  OP2ArtReader "Draw Lights" code). Light position uses `ArtFile::groupSelectionBox(normalGroup)`
  + the building's logical origin; the sprite is loaded once.
- **Ties to the Active/Idle property:** the green light shows only while a building is **Active**;
  go **Idle** (click it) and the light disappears — a simple on/off indicator, as in OP2.
- **Setting + toggle:** `[Render] Overlay=1` (default on) + an **"Overlay: ON/OFF"** sidebar
  button (`overlayEnabled`).

---

## 2026-06-03 — Object properties begin: building Active/Idle status

- First step of giving World objects real **properties**. Buildings now have a **status —
  Active or Idle** — and **Idle buildings don't animate** (they hold their base frame, no
  ambient/operating cycle). This mirrors OP2, where `Unit.DoAmbientAnimation` is gated by
  `IsEnabled()` (a disabled/idle building doesn't animate).
- **Data model:** new `UnitFlag::Idle` on the op2sim `Unit` (clear = Active). The World object
  is the source of truth.
- **Renderer:** `PlacedBuilding` carries `active` + a `sourceId` link back to its World unit;
  `placeFromWorld` stamps status from the flag, and `advanceAnimation` skips inactive buildings
  (holding the base frame).
- **Interactive:** **left-click a building to toggle Active/Idle** (`toggleBuildingAt` hit-tests
  the footprint; QML `TapHandler`). The toggle writes back to the World object, so it persists
  across re-derives. Click an animated building → it freezes on its base frame; click again →
  it resumes. (Verifiable now; the sim will set status from power/workers later.)

---

## 2026-06-03 — Showcase OPM re-flowed: 1-tile gaps, de-duplicated

- `opu_12_showcase.opm` had 1100 objects packed edge-to-edge (it was 190 distinct
  type+faction+weapon/cargo combos × ~8 directional copies). Re-flowed to **190 objects** (one
  representative each, all facing East — the directional copies were the "duplicates"; the C++
  Parade still shows all 16 directions). Laid out with a **1-tile gap** between every object,
  fitting the 128×128 map (X 4–122, Y 4–18): 1×1 units in a pitch-2 grid, footprint buildings in
  a footprint-aware band below. New reusable generator: `scripts/reflow_showcase.ps1`. Still
  loads in OP2MissionEditor (UTF-8 no BOM, arrays intact).

## 2026-06-03 — GuardPost: fix idle-rotation jitter (seating mismatch)

- Rotating gun GuardPosts jittered off-grid (non-rotating bomb GPs stayed aligned — the tell).
  `advanceUnitTurrets` re-seated with the vehicle chassis-frame mount offset, but a GuardPost has
  no chassis (self-contained combined sprite), so that offset was spurious and varied per
  direction. Now the re-seat skips the mount offset when there's no chassis image, matching
  `placeGuardPost`'s `unitPixel − groupDisplacement` placement exactly. GPs hold steady while
  wandering.

## 2026-06-03 — GuardPost: drop the stray construction one from the building showcase

- The C++ Parade still drew a GuardPost in the **building** showcase grid via its old
  construction-sprite fallback (label "GuardPost", animated) — a leftover now that GuardPosts
  have proper combat-showcase rows. `placeAllBuildings` now **skips** GuardPost.
- Defensive: a GuardPost loaded from a `.opm` with no weapon (e.g. opu_12.opm) now defaults to
  the faction's basic gun (Eden Laser / Plymouth ESG) so it renders, instead of nothing.

## 2026-06-03 — GuardPost graphics: corrected to a single combined sprite

- **Correction to the entry below.** The GuardPost weapon group sprite is the COMPLETE GP for
  that direction — disc base + weapon **baked into one sprite** (proof: group 966 Laser =
  op2_art frame 14125, group 1084 EMP = frame 14722). There is **no separate base**; drawing the
  construction frame underneath only leaked out beneath the tiny bombs. `placeGuardPost` now
  draws the single combined sprite (gun GPs rotate the whole thing; the disc is symmetric so
  reads static). **Bombs use their own combined disc+bomb group** (NOT the vehicle bomb sprite):
  Starflare Eden g1694 / Plymouth g938; Supernova g939 (shared). Found via the editor art +
  EXE bomb-GP selectors. Notes/memory updated.

## 2026-06-03 — GuardPost graphics solved: base + rotating weapon turret

- GuardPosts were rendering as their base only (no weapon). They're actually drawn **like a
  fixed Lynx** — a static base platform + a rotating weapon turret with its OWN art (distinct
  from vehicle turrets), idle-wandering except for the Starflare/Supernova bombs.
- **Decompile dig** (saved to `Review/decompiled/GuardPost_Rendering.md`): GuardPost
  `Initialize()`@0x448120 writes no Normal — the base is the **Construction group's last frame**
  (Eden g81 / Plymouth g178). `GuardPost.DrawUnit`@0x42d450 + `GetTurretAnimationIndex`@0x42d100
  draw the turret over it. The weapon's **Init/default rotation table = its GuardPost turret**
  (e.g. LaserGuardPost@0x4EA890 = g966–981), DISTINCT from vehicle turret groups. Read all 11
  turret tables straight from the EXE `.data` (fileoff = VA−0x401800).
- **Turret groups** (facing 0..15) baked into the renderer: Laser 966, RailGun 982, AcidCloud
  1068, **Thor's Hammer 947–962 (irregular order)**, ESG 1254, RPG 624, Microwave 1011,
  Stickyfoam 1031, Supernova 1342. **Faction-split:** EMP Eden g1084 / Plymouth g2043; Starflare
  constant (non-rotating) Eden g1623 / Plymouth g1341.
- **Renderer:** new `TilesetView::placeGuardPost` (base last-frame as the "chassis" + turret via
  `resolveGuardPostTurret`, a cached synthesised 16-dir UnitDef). Idle wander reuses
  `advanceUnitTurrets`, re-seating the turret off the base's LAST frame via a new
  `PlacedUnit.chassisFrame`. Bombs stay static. Added to BOTH the combat showcase (Eden +
  Plymouth GuardPost rows, all weapons × 16 dirs — see the **C++ Parade** button) and the
  `.opm` mission path (GuardPost weapon comes from its `CargoType`).

---

## 2026-06-03 — Game speed now reliably drives animation (fixed-cadence game loop)

- **Bug:** building/unit/resource animation stopped tracking the game-speed slider (1–0). The
  `GameClock` varied the *timer interval* with speed (27 ms at speed 10) and `advanceAnimation`
  advanced a fixed amount per call — so under the software renderer's paint load the GUI thread
  couldn't fire the short interval, the call rate saturated, and the high speeds collapsed to the
  same effective animation rate. (Screenshot evidence: 2204 ticks / 73 s ≈ 30 tick/s vs ~37 nominal.)
- **Fix — proper game loop** (the placeholder the code comment always promised): the `GameClock`
  timer now runs at a **fixed 40 fps cadence** (`kFrameIntervalMs`); an **accumulator** generates
  whole game ticks per frame from `ticksPerFrame(speed)`, and the clock exposes `ticksThisFrame`.
  Animation advances by *ticks-elapsed-this-frame* (`advanceAnimation(int gameTicks)`), so its pace
  scales with game speed independent of render load. The long-run tick rate is unchanged (Time/Mark
  display unaffected); `setGameSpeed` no longer touches the interval.
- Threaded `gameTicks` through `advanceAnimation` → buildings (`speedScale *= gameTicks`), resource
  sprites (`*gameTicks`), and the idle-turret beat (tick-quantised on `animTick_`, which now advances
  by `gameTicks`). `gameTicks==0` frames (low speed) simply hold. QML driver passes
  `gameClock.ticksThisFrame`.

---

## 2026-06-03 — Map resources render: ore beacons, magma vents, fumaroles, markers, wreckage

- The `.opm`'s `MasterVariant.TethysGame` carries non-unit map features (`Beacons`/`Markers`/
  `Wreckage`) that we weren't drawing. Now parsed + rendered, completing the visible object set.
- **`MissionLoader`** gained a `MissionResource` list (kind + subType + oreType/barYield + tile);
  `loadOpmMission` reads the three arrays. **`TilesetView::placeResources`** turns each into a
  `PlacedSprite` and **`advanceSprites`** loops the animated ones every tick.
- **Art mapping** (cross-checked against OP2MissionEditor's extracted `frm-NNNNN` PNGs — we render
  from the user's own `op2_art.prt`, those folders are just the index reference):
  - **Ore beacons** = the per-yield group picked by ore + bar count (Common Bar3/2/1 = frm
    18480/18491/18503, Rare = 18515/18527/18539), looped so the deposit sparkles.
  - **Fumarole** = frm 132+, **MagmaVent** = 10007+, **Wreckage** = 20338+, **Markers** Circle
    18608+ / DNA 23836+ / Beaker 23854+ — all **continuous** loops (no building rest-gap).
  - All these Gaia objects animate at a calmer cadence (`kGaiaSlowdown` = 6× slower than buildings)
    so vents plume / deposits sparkle gently instead of strobing. The cadence is driven by the
    GameClock tick (`advanceAnimation`), so it scales with game speed exactly like building/unit anims.
  - **Toggle**: `[Render] AnimationAmbientResource` (default on) + a "Resource Anim: ON/OFF" sidebar
    button (`ambientResourceEnabled`), mirroring the Structure/Unit anim toggles; off holds every
    Gaia sprite on its base frame.
  - New `ArtFile::groupOfFrame()` recovers the whole animation group (+ its pixel displacement)
    from one representative frame, so vents/markers/wreckage animate + seat correctly.
- Seated at the tile centre via the group displacement (like every other sprite); drawn over
  terrain but UNDER buildings/units (a vent half-covered by a structure reads right).
- Beacons are 1×1 map features (not player units), so kept as their own `missionResources_` list
  (rebuilt by `placeFromWorld` on a shadow/anim toggle), not forced into the unit pool.
- **opu_12.opm** test mission extended for visual verification: 8 beacons (Common + Rare mining
  beacons at Bar3/Bar2/Bar1 yields, a Fumarole, a MagmaVent), three markers (Circle/DNA/Beaker),
  and a wreckage — all round-trip through OP2MissionEditor.

---

## 2026-06-03 — Mission objects ANIMATE (loaded buildings + units come alive)

- **The `.opm`-loaded World is now rendered with full animation**, identical to the catalog
  showcase — not the static single-frame placement of the first cut. Mission mode and the
  parade now share one builder, so anything loaded from a `.opm` animates exactly like the
  showcase.
- **Extracted `TilesetView::placeBuildingInstance`** — the full building-build logic (Normal
  anim + up to two ambient anims + optional floor underlay + interleaved `[0,1,0,2,…]` play
  order + staggered first rest) was buried in `placeAllBuildings`'s `placeVariant` lambda. It's
  now a shared member taking a top-left tile + resolved `.prt` groups. `placeVariant` is a thin
  wrapper (compute grid cell → call it).
- **`placeFromWorld` building path** now resolves per-faction Normal/Ambient groups and calls
  `placeBuildingInstance`, mirroring the showcase special cases: **Garage** (construction
  last-frame as clean look + floor pad g2039 Eden / g431 Plymouth), **LightTower** (Eden
  animated-lamp Normal eden[5]; Plymouth construction last-frame plym[0]), general buildings
  (eden/plym[5] Normal + [6]/[7] ambients), GuardPost fallback to `displayGroup()`. So a loaded
  Agridome cycles, the Light Tower lamp blinks, etc.
- **`placeFromWorld` unit path** now sets the idle-turret-rotation re-seat inputs
  (`turretDef`/`turretRot`/`bodyRot`/`seatUpX,Y`/`chassisGroup`), so `advanceUnitTurrets()`
  makes loaded combat turrets wander like the parade. **Bomb weapons** (Supernova/Starflare)
  stay static, and on a Tiger they stack the single sprite (drawn twice, raised) — the in-game
  twin-bomb look — matching the showcase exactly.
- Honors the existing `AnimationAmbientStructure`/`AnimationAmbientUnit` sidebar toggles, since
  the advance loops iterate `placed_`/`placedUnits_` regardless of source.
- **Confirmed the data flow:** `.opm` objects are spawned into `world_` (op2sim) and the render
  is *derived* from that World pool — the scene is a view of sim state, not a parallel scene.

---

## 2026-06-03 — `.opm` → World → render (data-driven placement, first cut)

- **The bridge from showcase → sim-driven rendering.** New `MissionLoader` (`missionloader.{h,cpp}`,
  Qt `QJsonDocument`) parses a `.opm`'s players/units into `MissionObject`s. `TilesetView::loadMission`
  spawns each into the deterministic **`World`** (op2sim) — `typeId` via a name↔id registry, owner,
  faction flag, tile, rotation (8-dir → facing byte), weapon stashed in `cargo`. `placeFromWorld()`
  then builds the render lists FROM the World: vehicles composite chassis (+turret via
  `resolveTurretDef`, faction-aware) at their tile/facing; buildings draw their standing frame.
- **Wiring:** a `.opm` startup map → mission mode (renders the World); a plain `.map` → the static
  catalog parade (kept as the fallback + the only thing that draws impossible cross-faction combos).
  Branch in the TilesetView ctor on the map extension.
- Set `[Debug] Map = PlainsAndPasses/opu_12.opm` to exercise it. Coordinates are raw editor tile
  coords (matching the editor's view of the `.map`); to verify vs the engine's `+31/-1` LOCATION offset.
- **Not yet:** mission units don't tick/move (static placement from the World); beacons/walltubes/
  per-truck cargo sprites not parsed; GuardPost renders as its base building (weapon ignored).
- Fixes after first test (curated .opm loaded: all 24 objects → 13 buildings + 11 units, none
  skipped): (1) **shadow/ambient sidebar toggles were reloading the static parade** over the
  mission — made `setShadowsEnabled`/`setMobileShadowsEnabled`/`setAmbientUnitEnabled` rebuild
  via `placeFromWorld()` in mission mode. (2) **Spider/Scorpion drew a covering turret** ("two
  units with no chassis") — those robots are self-contained, so `resolveTurretDef` now only
  returns a turret for Lynx/Panther/Tiger.
- **Placement now matches OP2MissionEditor** (from the editor source, `D:\GITHUB\OP2MissionEditor`):
  - **`.opm` Position is 1-based "game coordinates."** `UnitView.GetMapCoordinates` subtracts
    (1,1) to get the 0-based map cell (then inverts Y for Unity's bottom-up tilemap — we're
    top-down, so no invert). We were using Position raw → everything was +1,+1 off (ConVec at
    16,28 instead of 15,27). Fixed: `loadMission` stores `tileX-1, tileY-1`.
  - **A building's Position is the footprint CENTRE** (`StructureView.GetStructureArea`:
    `xMin = pos − size/2`, floored) — so top-left tile = `(pos−1) − size/2`. Was treating it as
    top-left → ~1-tile gap between adjacent buildings (Agridome|StructureFactory). Our catalog
    footprints match the editor's `GetStructureSize` table exactly.
- **Cargo trucks show their cargo** — full mapping matching OP2MissionEditor's `UnitRenderer`
  (TruckCargo 1–7 named; 8 `Spaceport`→Starship Components; 9 `Garbage`→Wreckage; anything else
  incl. a numeric module id like `"90"`→Gene Bank). `cargoTruckBodyName` swaps the empty
  `CargoTruck` body for the pre-composited loaded-truck sprite; `Empty`/`None` stay empty.

---

## 2026-06-03 — Turret faction art: Eden Starflare found (was using Plymouth on both)

- Discovered weapon turrets can be **faction-specific art**, which we'd wrongly flattened to
  "both". EXE decode: **most turrets ARE shared** (gun weapons + Supernova — player-palette
  only), but **Starflare** and **EMP GuardPost** have distinct Eden/Plymouth groups.
  **Starflare: Eden = g1623 (silver), Plymouth = g1341 (yellow)** — we'd been drawing the
  yellow Plymouth turret on Eden chassis too. Faction selected by the per-player flag
  `0x56EF48 + player*0xC24` (1=Eden/0=Plymouth, per `GoEden`/`GoPlymouth`).
- Fix: catalog Starflare split into `eden` (1623) / `plymouth` (1341); new faction-aware
  `findTurret(name, faction)` in the renderer prefers the faction art and falls back to the
  shared "both" sprite (so gun turrets are unaffected). Built + deployed. `op2art_names.ini`
  updated (1341→Plymouth label, +1623 Eden). Decompile notes + memory record the mechanism.
- Now that both Starflare graphics exist, the C++ showcase can render any cross-faction combo
  (e.g. a Plymouth Lynx with the Eden silver Starflare) — the thing OP2MissionEditor can't.
- **Weapon→faction matrix recorded** (user-confirmed): Eden-only = Laser/RailGun/AcidCloud/
  ThorsHammer; Plymouth-only = ESG/RPG/Microwave/Stickyfoam; both = Starflare/Supernova/EMP.
  Exclusive weapons have one (correct-faction) art and no selector faction-branch — consistent
  with the decode. Implication: the combat showcase's "every chassis × every weapon" columns
  inherently include many cross-faction/impossible combos (each faction's chassis wearing the
  other's weapons) — by design (the code's USP), but reorganizable to faction-native if wanted.

---

## 2026-06-03 — GeoCon & RepairVehicle: real 16-dir rotation arrays

- Both were mis-extracted as **8-dir** (GeoCon 1154–1161, RepairVehicle 1130–1137). Decoded
  the real Inits (same `BULL_DOZER`-style code-name pin): **GeoCon** (`GEO_METRO`, Init
  `0x447C70`) = 16-dir contiguous **1154–1169**; **RepairVehicle** (`REPAIR_TRUCK`, Init
  `0x448880`) = 16-dir contiguous **1130–1145** (+ a repair-arm action anim at +0x2CC,
  g1625–1632). 16-dir matches every other colony vehicle (only combat chassis/robots/Dozer
  are 8-dir). Catalog Eden rows corrected to the full 16-dir arrays + deployed.
- Both are **Eden-only** units (gray Eden shading). Each Init also writes an 8-dir Plymouth
  array reusing the Eden groups — that exists so a Plymouth player can render one *acquired
  via Trade Center* (same sprites, owner palette), not because Plymouth builds them. So they
  stay Eden in our render.
- `op2art_names.ini` (op2artreader) updated: old 8-dir GeoCon/RepairVehicle entries replaced
  with the full 16-dir label sets (1154–1169, 1130–1145), tagged Eden.

---

## 2026-06-03 — Robo-Dozer added to the unit catalog (was missing)

- Our C++ parade was missing **Robo-Dozer** (the one colony vehicle never extracted — its
  Init was a long-standing "desynced" gap; the old `1126` candidate was actually
  Robo-Surveyor). Decoded from the EXE: vtable `0x4D13C0`, Init `0x409BA0`, pinned 1:1 via
  the `BULL_DOZER` code-name accessor (`0x4475A0`) sharing its vtable — immune to the
  nearest-function mislabel trap. 8-dir doubled: **Eden groups 0–7, Plymouth 343–350**
  (Eden base = group 0, unusual but consistent — CargoTruck eden=8 sits right after).
- Added both faction rows to `unit_anim_catalog.tsv` (deployed; data-only, no rebuild).
  Decompile notes + memory updated; the desynced-Init item is now closed.

---

## 2026-06-03 — Colony-units .opm showcase + direction-casing fix

- **`opu_12_colony.opm`** (144 units) — all colony (non-combat) vehicles, each in all 8
  directions, 1-tile gap between every unit/row. Eden: ConVec, CargoTruck, RoboSurveyor,
  RoboMiner, GeoCon, Scout, RoboDozer, EvacuationTransport, RepairVehicle, Earthworker;
  Plymouth: same minus the two Eden-only (GeoCon, RepairVehicle). Loads in OP2MissionEditor
  ✓ — RoboDozer confirmed (was the one previously-untested type).
- **Direction casing corrected:** the editor's canonical `Direction` is camelCase
  (`SouthEast`/`SouthWest`/`NorthWest`/`NorthEast`); it reads case-insensitively but emits
  camelCase. Fixed `OPM_FORMAT.md`'s Direction table and regenerated `opu_12_showcase.opm`
  with the canonical casing. Also added a **weapon×chassis validity matrix** to
  `OPM_FORMAT.md` (bombs Starflare/Supernova: Panther+Tiger only).

---

## 2026-06-03 — Two .opm test missions (curated + full showcase); fixed editor load

- **Fixed the OP2MissionEditor load error.** Diff against a known-good file
  (`cSDKPieChart.opm`) showed my `.opm` was structurally identical at every level — the
  one value-level difference was unit `BarVariant`/`BarYield` = `"Random"` (mine-only
  enum fields) where working files use **`null`**; the editor's enum parser rejects
  `"Random"` there. Both files now set them `null`. Also switched to the Newtonsoft
  layout (TethysGame-first, declaration order) matching the SDK 3.0 samples, UTF-8 no BOM.
  (Reconfirmed coords are fine — opu_12 is 128×128, all placements in-bounds.)
- **`opu_12.opm`** — curated 2-player mission (bases, weaponised combat vehicles, cargo
  truck, beacons).
- **`opu_12_showcase.opm`** — the full code parade as a mission: **1196 units** — every
  vehicle in all 8 `.opm` directions, both factions, combat chassis × all 11 weapons,
  GuardPost × weapons, cargo trucks × 9 cargo types, plus a one-of-each building block.
  Note `.opm` `Direction` is 8-valued (not our 16), and it can't express cross-faction
  graphic combos (e.g. Plymouth-Lynx + Eden-Starflare) — so the C++ placement showcase
  STAYS as the only way to render those, exactly as called out.
- **Both files load in OP2MissionEditor** ✓ (1100 units in the showcase). Editor-load
  gotchas discovered (the editor renders OP2 art, so it rejects anything OP2 never drew):
  - Unit `BarVariant`/`BarYield` must be **`null`** (not `"Random"`) — else enum-parse fail.
  - **No mine/vent structures** (`CommonOreMine`/`RareOreMine`/`MagmaWell`/`GeothermalPlant`)
    without an underlying beacon/vent tile → `IndexOutOfRangeException` in "Calculating
    Cell Types".
  - Keep placements **off the map edges** (edge-adjacent structure footprints index
    negative cells).
  - **Weapon×chassis must have OP2 art** or "Creating units" throws "Object … is null".
    Bomb weapons (`Starflare`/`Supernova`) are valid on **Panther + Tiger only** (Tiger
    confirmed via a user editor-save); not on Lynx/Spider/Scorpion/GuardPost.
  - Editor output is **DataContract** (tabs, alphabetical keys, leading blank line); our
    curated file round-trips through it cleanly (`opu_test.opm`).

- **Authored `OPU/maps/PlainsAndPasses/opu_12.opm`** — a 2-player (Eden/Plymouth) test
  mission on our test map, matching the OP2MissionEditor schema (validated against
  `OPM_FORMAT.md` + the VB model): a small base each (Command Center, factories, smelter,
  power, agridome), weaponised combat vehicles (Lynx+Laser, Panther+Microwave,
  Tiger+RailGun / Lynx+EMP, Tiger+Thor's, Spider+ESG, Scorpion+Stickyfoam), a GuardPost,
  a cargo truck with ore, and 3 beacons. Clean tab-indented JSON, opens in OP2MissionEditor.
  Coordinates are editor-space (the engine applies `LOCATION(X+31, Y-1)`).
- **`OpmLoader` implemented (was a stub):** parses `LevelDetails.MapName` and resolves the
  companion `.map` relative to the `.opm`'s folder → `referencedMapFile`. The
  `MapLoaderRegistry` now **follows `referencedMapFile` in one hop**, so loading a `.opm`
  yields its terrain end-to-end. (Mission content — players/units/beacons — is NOT parsed
  into the world yet; that's the next step and wants a real JSON parser. The C++ placement
  showcase is untouched, as requested.)

- **Fixed whole-rows-in-sync:** our parade indices are sequential, so a showcase column
  (which shares one hull facing) drifted in unison. OP2 keys the beat/mode on the unit's
  scattered POOL index — so we now run the sequential index through an integer hash
  (splitmix-style finalizer) to give each unit an independent phase + mode. Turrets now
  wander out of step.
- **Bombs no longer rotate:** Supernova & Starflare are a bomb on the hull (no turret) and
  are static when idle in OP2 on all 3 chassis. `placeRow` now takes `idleRotate`; the
  showcase passes `!w.bomb`, so bomb turrets keep `turretDef` null and never rotate.

---

## 2026-06-03 — Idle turret rotation + ambient-animation toggles + log cleanup

- **Idle turret rotation (units look "alive"):** military units wander their TURRET
  (hull stays put). Now **OP2-faithful** — decoded `Weapon::AnimateIdleTurret @0x4A7DD0`
  (gated no-target @0x4A7650): it's a **mode-driven wander**, not a clean spin. Each beat
  `((tick^idx)&7)==0` a slowly-changing mode `(idx^(tick>>8))&0xF` picks step-CW /
  step-CCW / drift-toward-hull / drift-toward-random / hold. `advanceUnitTurrets()`
  reproduces this in 16-direction space, re-blitting + re-seating the turret on the (fixed)
  chassis mount offset. Body field is +0x1C, turret +0x2D (separate — confirms the
  hull-holds/turret-cycles look). Full decode in the decompile notes.
- **`[Render]` toggles + sidebar checkboxes:** `AnimationAmbientStructure` (building idle
  animations) and `AnimationAmbientUnit` (unit turret wander), both default on, with
  "Struct Anim"/"Unit Anim" sidebar toggles (like the shadow/grid/label ones). Structure-
  off snaps buildings to their base frame; unit-off rebuilds the parade static (turrets
  back to hull facing). (Debug-menu toggles for now; will move to a Game Prefs menu later.)
- **Map ini fix:** Qt's INI reader treats `\` as an escape, so `\PlainsAndPasses\...`
  failed — switched the `[Debug] Map` value/docs to forward slashes
  (`PlainsAndPasses/opu_12.map`).
- **Removed animation debug-log spam** (per-building anim placement + per-cycle "anim done"
  lines). Export/startup-map logs kept.

---

## 2026-06-03 — Full-map PNG export + configurable startup map

- **Second export button "Export Map"** (sidebar, below "Export PNG"): saves the ENTIRE
  map at 1:1 — every tile, building and unit — to `exports/map-<timestamp>.png`, ignoring
  camera/zoom. Refactored `paintMap` → shared `paintMapContent(p, firstCol,firstRow,
  lastCol,lastRow)` so the viewport paint and the full-map export render identically;
  `exportFullMap()` renders into a full map-sized image (with a ~256 MPx safety cap).
  "Export PNG" still saves just the current viewport.
- **`[Debug] Map` ini setting** — which map loads at startup, relative to
  `<Outpost2>/OPU/maps/` (accepts `/` or `\`, leading slash optional). Replaces the
  hardcoded `Nostalgia/opu_02.map`. Set to `\PlainsAndPasses\opu_12.map`. Logged at startup.
- Render-test roster recap: **46 unique single-tile vehicles** in the main parade
  (44 ground chassis + 2 flying shuttles), plus 66 chassis⊕weapon combat-showcase rows.

---

## 2026-06-02 — Combat showcase: every chassis × every weapon (full matrix)

- Corrected the model (OP2 art reference + EXE selectors): **the turret graphic is
  weapon-determined, every chassis mounts every weapon. Lynx & Panther SHARE one
  graphic; Tiger has its OWN** (usually a dual turret, sometimes a larger single, e.g.
  ESG). The Tiger table sits +16 groups right after the shared/single table.
- Verified **9 weapons** now have a Tiger table — added **EMP (1318), RPG (1411),
  Microwave (1509)** after confirming from EXE bytes they're real 16-rotation tables
  (the prior decompile pass had mislabeled these three as "GuardPost"). The 6 already
  known: Laser, RailGun, ESG, Thor's, Stickyfoam, Acid Cloud.
- **Supernova & Starflare are bomb weapons with no dedicated dual sprite** (the in-game
  "dual" look is just the single stacked) — so Tiger reusing the single graphic is
  correct by design, not a missing table. Supernova's single is the last table in the
  block; the next slot is zero, confirming no dual was ever authored.
- **Bomb-weapon Tiger stacking:** Supernova/Starflare have no dual sprite — per OP2,
  the Tiger draws the single turret TWICE, stacked. Added `PlacedUnit.turretStackDy`;
  paint draws a second turret copy raised by ~⅓ of its art height. Wired for Supernova
  (Tiger); approximate stack offset pending visual confirmation / exact OP2 value.
- **Starflare/Supernova line resolved** (full `.data` scan + vtable reconstruction):
  the EXE has exactly 32 rotation turret tables, ending at 0x4EADD0 — no 33rd. **Supernova**
  (vtable 0x4D4260) has the only rotating turret of the pair (0x4EADD0, g1342) and **no
  Tiger dual**. **Starflare** (vtable 0x4D4220) has **no rotation table at all** — a
  *constant* (non-rotating) bomb sprite, base group **1341**, and is the base art of a
  Starflare→Supernova upgrade pair (per-player swap via 0x56EF48, like EMP).
- Added **Starflare** to the showcase as a bomb weapon: constant g1341 sprite across all
  facings, Tiger-stacked. Lynx/Panther/Tiger rows now cover 11 weapons (Starflare + the 10).
- **Art-confirmed the double-draw (dumped op2_art.prt frames):** every turret group has
  exactly ONE picture per frame. Gun "duals" are a *distinct pre-baked two-barrel image*
  drawn once (Laser single img2403 22×18 → Tiger img2419 22×23). Bomb weapons have NO
  separate Tiger image (Supernova=img2721, Starflare=img2719), so the Tiger twin-bomb can
  only be the single image drawn twice — validating `turretStackDy` as the correct, OP2-
  faithful mechanism for exactly these two weapons. Stack ≈⅓ bomb height ≈ the gun-dual's
  +5px growth. Also fixed a stale comment: PRT image defs are 20 bytes, not 16.
- **In-game verified** against OP2 reference screenshots: a selected unit reads
  "Supernova Lynx" (bomb weapons mount on Lynx → every-chassis model holds), and the Tiger
  rows show the stacked twin-bomb exactly as `turretStackDy` renders it. (OP2 names combat
  units `<Weapon> <Chassis>`; our showcase labels `<Chassis> + <Weapon>` — cosmetic.)
- Showcase now renders **3 chassis × 10 weapons = 30 rows per faction** (Eden +
  Plymouth columns). The turret seats via each chassis's own frame optional-block
  offset, so a single-turret weapon mounts correctly on Lynx, Panther, AND Tiger.
- Selector/table map fully recovered: each weapon's `GetTurretAnimationInfo`
  (`vtable+0x14`, `ret 8`) reads `[rot*4 + tableVA]` → sets `DAT_004ea7b8/bc/c0`
  (anim index + mount x/y). Tables are 8 dwords (8-dir). EMP's multiple tables are a
  per-player **upgrade art swap** (flag `0x56EF48 + player*0xC24`), not single/dual.

---

## 2026-06-02 — Combat-turret showcase: Eden + Plymouth columns

- New parade section: each combat chassis composited with **every weapon turret it
  can mount**, all 16 rotations — now in **two columns, Eden and Plymouth**, right
  of the main parade.
  - **Lynx** ×6 (Laser, RailGun, ESG, Thor's Hammer, Stickyfoam, AcidCloud)
  - **Panther** ×4 (EMP, RPG, Microwave, Supernova)
  - **Tiger** ×6 (Laser, RailGun, ESG, Thor's Hammer, Stickyfoam, AcidCloud)
  - 16 rows × 2 factions, labelled `"<Chassis> + <Weapon> [E|P]"` under each row.
- Factored the column placement into `placeCombatColumn(faction, tag, startX)` and
  call it for `eden` then `plymouth`; columns are `16*dirStep + 4` tiles wide so they
  sit side by side. Turrets are faction-`both`, so the same turret art seats on each
  faction's chassis (the per-frame optional-block offset is read from each chassis,
  so geometry adapts automatically).
- Refactored `placeAllUnits`' inner `placeRow` lambda to take explicit
  `(rowStartX, tileY, body, turret, label)` so the main parade and both showcase
  columns share the exact same OP2-exact seating + composite path. Added a
  `findUnit(name, faction)` catalog lookup.
- Reuses the verified turret seating (chassis frame optional-block offset), so the
  showcase needs no new geometry. Built + deployed to `D:\opu\build\op2remake`.

---

## 2026-06-02 — Turret placement fixed (OP2-exact, from the chassis frame's optional block)

- Combat-unit turrets sat ~7–10px too low. Root cause found in `Unit:Tank.DrawUnit`:
  the turret mount point is the **chassis frame's optional info** —
  `GetFrameOptionalInfo(chassisAnim, frame)` → a signed `(x,y)` we'd been
  parse-skipping (the `.prt` per-frame 2-byte block flagged by the `0x80`
  picture-count bit). Turret is drawn at `unitPixel + offset − turretDisp`.
- **`ArtFile`** now captures that block and exposes `frameTurretOffset(frame,&x,&y)`.
- **`placeAllUnits`** switched to OP2-exact seating: chassis at
  `unitPixel − chassisDisp`, turret at `unitPixel + chassisFrame.turretOffset −
  turretDisp` (per rotation). Also made unit seating displacement-based (replacing the
  bbox-centre heuristic) — which is naturally shadow-stable, so the shadow-toggle
  jump fix comes for free. Offsets: Lynx ~(0,−6), Panther ~(2,−6), Tiger ~(−1,−10).
- Docs: corrected the turret section in `Unit_Rotation_Animation_Architecture.md`,
  decoded `optional1` in `OP2_ART_FORMAT.md`, updated the memory.

---

## 2026-06-02 — Game-object model (`World` / `Unit`) — the save/load foundation

- **`op2sim/world.{h,cpp}`** — the in-world object store, modelled on OP2 (one flat
  `Unit` struct for buildings AND vehicles, a fixed pool, a per-type `UnitType` table)
  but made **handle-based + pointer-free** so it lives in the deterministic sim core:
  - `Unit` POD — owner, tile pos, rotation, HP, command/action, cargo, timers, flags.
  - `World` — slot vector + free-list; `spawn`/`destroy`/`get`/`forEach`; stable
    `UnitId{index, generation}` handles (stale handles resolve to null).
  - `foldInto(Checksummer&)` — folds every live unit into the determinism fingerprint.
  - `serialize()`/`deserialize()` — **pointer-free save/load**: snapshot the pool to
    bytes and restore it; the state checksum matches across the round-trip (no OnSave/
    OnLoad pointer swizzling like OP2 needs). 15 new self-test checks (now 64 total).
- **Docs:** OP2's design captured for remakers in
  `Remake/docs/GAME_OBJECT_STORAGE.md`; our model in `docs/GAME_OBJECTS.md`.
- Foundation for: rendering from the `World` (retire the parade), full savegames, and
  the economy/combat sim ticking on real objects.

---

## 2026-06-02 — Full vehicle roster identified + cargo trucks + grid-aligned parade

- **Whole roster mapped & named** (chassis bases extracted from `Initialize()`, scanning
  BOTH `mov`-immediate forms): ConVec, Lynx/Panther/Tiger, Robo-Miner, Robo-Surveyor,
  Earthworker, GeoCon, RepairVehicle, Scout, Evac Transport, Spider, Scorpion, empty
  Cargo Truck + Inter-Colony Shuttle. Names confirmed **by rendering in-app** (the
  nearest-function labels proved unreliable — several were corrected: Robo-Miner,
  Earthworker, Robo-Surveyor, Cargo Truck, Scout, Evac).
- **Cargo trucks** — decoded the "packed" `0x4DE600` tables (each dword = two `uint16`
  sprite indices → contiguous 16-rotation arrays of composited truck+cargo). Added all
  **20 loaded-truck variants** (10 cargo types × Eden/Plymouth): Food, Rare/Common Ore,
  Rare/Common Metals, Rare/Common Rubble, Starship Components, Wreckage, Gene Bank.
- **Parade polish** — every unit grid-aligned (one tile each + a one-tile gap, centred
  on its tile), row labels moved **under** the units (like buildings) with a row number.
- Decompile note now carries the full chassis table, the 32 turret tables, and the
  cargo base→type map; `LEARNINGS.md` updated with the unit-rendering gotchas.

## 2026-06-02 — Units on the map (rotation sprites, chassis ⊕ turret)

- **Unit rotation system decoded + rendered.** Vehicles rotate (16 directions,
  `rotationIndex = ((rotation+8)>>4)&0xF`). Chassis sprite per rotation comes from
  the vehicle UTI arrays (`+0x24C` eden / `+0x34C` plym), extracted from each
  `Initialize()` (two instruction forms). Combat units composite a **weapon turret**
  on top at the **same origin** (turret offset = 0; the sprite anchor seats it —
  render-verified). Full decode: `Review/decompiled/Unit_Rotation_Animation_Architecture.md`.
- **`unit_anim_catalog.tsv`** — chassis (ConVec, Lynx, Panther, Tiger, RoboSurveyor,
  RepairVehicle, Cargo/AirTransport, Spider, Scorpion, +2 non-contiguous builders),
  20 weapon turrets, and the 2 shuttle bodies — each as its explicit 16-rotation
  group array (NOT a formula: 8-dir bodies double sprites, some arrays are
  non-contiguous). `render/unitcatalog.{h,cpp}` parses it.
- **`TilesetView::placeAllUnits()`** lays the catalog out as a parade below the
  building showcase: every unit shown in all 16 directions, combat units with their
  turret composited. Copied next to the exe via CMake POST_BUILD.

---

## 2026-06-02 — Building animations: one-at-a-time (Normal + Ambients), grid-exact placement, garage/light-tower fixes

- **Grid-exact placement via the group pixel-displacement.** The `.prt` group
  header carries a pixel-displacement we'd been parse-skipping. The frame's logical
  origin (0,0) maps to `unitPixel − displacement` (unitPixel = footprint centre), so
  `originX = round((tileX + fpX/2)·T) − dispX` (same for Y). Picture offsets are
  baked into each composited frame, so **docks/floor pads land on their designed
  tiles** (fixed the off-grid Common Ore Smelter dock). Replaced the old
  bottom-centre heuristic. Exposed via `ArtFile::groupDisplacement/groupSelectionBox`.
- **Garage** — the catalog's Normal pointed at the *damaged* sprite. Clean look =
  the **construction group's last frame** (Eden g2038, Plymouth g430); plus the
  separate **floor/dock pad** drawn under the body (Eden g2039, Plymouth g431).
  Renderer special-cases it (static last frame + floor underlay).
- **Plymouth LightTower** — same trap (pNorm=187 was the damaged sprite). Clean
  tower = construction last frame **g186**. Swept the whole catalog for
  Normal==DamagedLight; only these two had it.
- **Ambient animations play ONE at a time — NOT layered.** A building has
  `Normal` + optional `Ambient1`/`Ambient2` groups, each a *complete* animation
  sharing the same base. Per `Unit.DoAmbientAnimation` (@0x483580, decoded from the
  exe): the engine plays one at a time, triggering an ambient only when
  `(pixelY + unitIndex·8 + tick) % 100 == 0` (~once/100 ticks, staggered per unit).
  (First tried simultaneous overlay-layering; that was wrong/too noisy.)
  `PlacedBuilding::anims = [Normal, Amb1, Amb2]`, `curAnim` is the single one drawn.
- **Inter-animation pause + rotation (restores AnimationGap).** The building plays
  an animation through, **holds its rest frame for `[Render] AnimationGap` ticks
  (default 60)**, then rotates to the next via a `playOrder` interleaving Normal
  between the ambients (`[0,1,0,2,…]`). Restores the "pause before repeating" feel
  AND guarantees every ambient is shown — a continuous loop was hiding the lower
  lights-strip ambients (e.g. Eden AdvancedLab g601). Staggered so buildings don't
  animate in unison. (Showcase pacing; OP2's exact trigger is the per-position table.)
- **Overview:** `building_animations.md` lists all 38 buildings × both factions with
  their Normal/Ambient groups + frame counts (26/38 have ≥1 ambient).

## 2026-06-02 — Tile grid + PNG export

- **Grid toggle** (sidebar "Render → Grid"): draws the 32px tile grid in map
  space, **over terrain, under buildings**. Cosmetic 1px pen (crisp at any zoom),
  subtle semi-transparent black.
- **Export PNG** button: renders the current viewport (terrain + grid + buildings
  + shadows) to `exports/viewport-<timestamp>.png` next to the exe; "Saved!"
  feedback. `TilesetView::exportViewport()`.

## 2026-06-02 — Building shadows (OP2-accurate)

- Buildings now cast their real OP2 drop-shadows (the art's type 4/5 shadow
  pictures), composited **under** the body (pictures sorted by `pictureOrder`).
- **The shadow format:** type 4/5 images are **1bpp masks**, row stride
  `((w+31)/32)*4` (32-bit aligned) — **NOT** the stored 8bpp-style pitch. Set bit
  = ~50% black. Decoding with the wrong stride scrambles the solid mask into a
  fake "dither" (the source of a long debugging loop). Authoritative ref:
  OP2ArtReader `CPrtFile.vb` `RenderShadow`. Corrects the `OP2_ART_FORMAT.md`
  "1bpp, layout unverified" note.
- **Sidebar "Render → Shadows" toggle** — re-composites all buildings on/off at
  runtime; initial state from `[Game] StationaryShadows`.

## 2026-06-02 — Animation rework: uniform rate + inter-loop gap + debug log

- **Uniform pacing** (was per-frame art delays, which raced low-delay buildings):
  the decompile shows `SetAnimation` holds every frame a uniform `animationDelay`
  (total = `numFrames * animationDelay`), not the per-frame bytes. So all
  buildings advance ≤1 frame/tick, capped so no loop is faster than
  `kMinLoopTicks` (≈1.25 s) — long groups (SF) keep their rate, short ones
  (AdvLab) stop looking frantic.
- **`[Render] AnimationGap`** ini key: ticks a building idles on frame 0 between
  loops (building "rests", then animates again). Scales with game speed; staggered
  per building so they don't puff in unison. `0` = continuous.
- **File logger** (`logger.{h,cpp}`): timestamped log to `logs/op2remake-*.txt`.
  Traces two structures' loop timing for debugging the pacing.

## 2026-06-02 — All 34 buildings on the map (building catalog)

- Extracted the authoritative **building → sprite-group** mapping from
  `Outpost2.exe` (`UnitTypeInfo:<Name>.Initialize()` hardcodes the art groups;
  `animationIndex = prtGroup + 101`; 8-state `BuildingAnimationTypeIndex` enum ×
  Eden/Plymouth). Output: **`building_anim_catalog.tsv`** (all 34 buildings,
  footprints + groups). See `D:\Oupost2Decompile\Review\ART_ANIMATION_MAPPING.md`.
- **`BuildingCatalog`** loader + `TilesetView` places **all 34 buildings** in a
  grid, each playing its operational animation (replaced the hardcoded demo SF).
- `art_group_catalog.tsv`: all 2,079 `.prt` groups classified (frames, size, delays).

## 2026-06-01 — AnimationSpeed ini setting (live-tunable)

- New `[Render] AnimationSpeed` ini key (sprite animation ticks per game tick).
  `TilesetView` reads it at startup (was the hardcoded `kAnimTicksPerGameTick`);
  tune it + relaunch, no rebuild. Default 5. Still scales with game speed.
- Package (exe + ini) updated at `D:\opu\build\op2remake`.

## 2026-06-01 — Faster animation + window title

- `kAnimTicksPerGameTick` 2 -> 3 (animation ~1.5x faster; still game-speed-tied).
- Window title -> "Outpost 2: Divided Destiny".
- Release exe updated in `D:\opu\build\op2remake`.

## 2026-06-01 — Animation tied to game speed (like OP2)

- Building animation now advances on **GameClock ticks** (QML `Connections` on
  `gameClock.ticked` -> `TilesetView::advanceAnimation()`), not a fixed
  wall-clock timer. So it follows the game-speed slider: slow at speed 1, fast
  at speed 10 — exactly like OP2.
- Per-frame art delay still honoured (accumulator vs `frameDelays_`). OP2 advances
  ~2x per game tick (`kAnimTicksPerGameTick=2`, empirical — the decompile's
  gameSpeed->ms/tick is an adaptive EWMA with no clean constant; tunable).
- First step of renderer-driven-by-the-game-clock (Phase 3 flavour).
- Release exe updated in the shareable package `D:\opu\build\op2remake`.

## 2026-06-01 — Faster building animation (~2x)

- Animation clock 25ms->12ms/tick (`kAnimTickMs`, ~83 Hz) so the building
  animates ~2x faster, matching OP2 at game speed 10 (group 73 smoke loop
  ~9.3s -> ~4.5s). Still a fixed rate — TODO: drive off the sim tick so it
  follows the game-speed slider.
- Updated the Release exe in the shareable package at `D:\opu\build\op2remake`.

## 2026-06-01 — Configurable game-data path + shareable build

- `[Game] Outpost2=` ini key: path to an Outpost 2 1.4.1 / OPU install. All game
  data (OP2_ART.BMP, tilesets, sprites, the map) is now read relative to it —
  `Config.outpost2`; `TilesetView` reads it from the ini next to the exe. No
  copyrighted OP2 assets are bundled — users point this at their own install.
- Paths are now exe-relative: `outpost2.ini` is read from
  `applicationDirPath()` and POST_BUILD-copied next to the exe (alongside
  OP2Utility.dll).
- Produced a self-contained Windows build at `D:\opu\build\op2remake` via
  `windeployqt --release --qmldir Outpost2 --compiler-runtime` (Qt DLLs +
  platform plugin + MinGW runtime + OP2Utility.dll + outpost2.ini, ~82 MB).
  Zip-and-share ready; recipients set `Outpost2=` to their install path.

## 2026-06-01 — More outpost2.ini settings

- Added `[Game]` keys: `StationaryShadows` (buildings cast shadows),
  `MobileShadows` (units cast shadows), `Name` (player name). Read by `Config`
  (`stationaryShadows`/`mobileShadows`/`name`), exposed to QML as `config`.
- NOT wired to rendering yet: building shadows are still skipped (the 1bpp
  type-4/5 shadows produced artifacts). When proper drop-shadows land,
  `StationaryShadows`/`MobileShadows` will gate them. `Name` is for later
  (player/save/MP).

## 2026-06-01 — Fix sprite shadow artifacts + add zoom-in level

- **Shadow artifacts**: building frames include 1bpp shadow pictures (image
  types 4/5, e.g. group 73's img1766-1768) that were compositing as dark
  speckle. `frameToQImage` now **skips shadow pictures by default**
  (`includeShadows=false`). Proper drop-shadows (drawn under the sprite onto
  terrain) are a TODO.
- **Zoom**: now 3 discrete levels — 0.5x (out) / 1x / 2x (**in**). Mouse wheel
  up zooms in, down zooms out, anchored under the cursor (still INI-gated).
- Built + linked.

## 2026-06-01 — Structure Factory with art-timed smoke animation

- Parse per-frame animation timing: the frame's 2nd byte `& 0x7F` = ticks to
  hold the frame (confirmed by the format spec). `ArtFile::frameDelay(frame)`.
- `TilesetView` now draws the Eden Structure Factory (group 73) and advances its
  frames at the **art-defined speed** — a 40 Hz tick accumulator holds each
  frame for its `delay` (group 73 ≈ 7-8 ticks/frame, ~9.3 s smoke loop). TODO:
  drive off the real sim tick (so it follows game speed) once wired.
- (Lights on/off are a separate building-state concern, per the original — not
  handled here; group 73 is the operational "smoke" animation.)
- Built + linked.

## 2026-06-01 — Animated building on the map

- `ArtFile::frameToQImage(frame)` — composites a frame's positioned pictures
  (images at x/y offsets) into one ARGB32 QImage with origin anchor.
- Learned the art model: a building's live look is a `.prt` **group**
  (animation), each frame composed of ~30 small image pieces; smoke/lights are
  pieces that move/appear across frames. Found by searching groups that
  reference the building's image index. Eden Structure Factory: group 73 (50
  frames, operational), group 72 (construction), 65/66/196 (damaged/destroyed).
- `TilesetView` now pre-composites a building group's frames at load and cycles
  them (~10 fps) on the terrain at a fixed tile — first **animated** object on
  the map, scrolling + zooming with the camera.
- Hardcoded group/tile for now — next: building-type -> group table + sim-driven
  placement (+ per-frame animation timing from the .prt instead of fixed fps).
- Built + linked.

## 2026-06-01 — Sprite/art loader (op2_art.prt + OP2_ART.BMP)

Loads OP2 sprite art into renderable QImages.

- New `ArtFile` (`render/artfile.{h,cpp}`, render layer). Parses `op2_art.prt`:
  palettes (CPAL/PPAL/head/data — colors stored R,G,B,reserved), 16-byte image
  defs (pitch/dataPtr/h/w/type/palette), and the group->frame->picture animation
  hierarchy (variable-length frame headers with high-bit optional blocks). Keeps
  the OP2_ART.BMP pixel blob.
- `imageToQImage(i)` -> ARGB32 (8bpp via palette, **index 0 = transparent**;
  1bpp shadow types 4/5 -> semi-transparent). Animation navigation accessors
  (group->frame->picture) for compositing unit frames later.
- Verified against the REAL files: **9 palettes, 5390 images, 2079 groups,
  24185 frames, 160922 pictures** — exact match to the reference parser. Sample
  sprites rendered to PNG look correct (right palette colors + clean
  transparency).
- Built into the app (not wired to on-map rendering yet).
- Ref: OP2ArtReader (VB.NET) at D:\opu\op2artreader; spec
  `C:\Users\User\Desktop\op2_file_formats.htm`.
- NOTE: `_sprite_samples/` PNGs in the repo are throwaway verification output —
  safe to delete.

## 2026-06-01 — Morale section parser

Dedicated typed parser for the multi-section `morale.txt` (the generic `Sheet`
loader can't model its layout).

- New `op2::sim::MoraleData` (`sim/morale.{h,cpp}`). Parses the tabular levels
  section into typed `MoraleLevel`s (name + 8 fields: ptsTotal, powBonus,
  rschBonus, factBonus, foodBonus, defectRate, fertRate, dieRate), and every
  other `NAME -> value` block (events, decay rate, conditions, difficulty) into
  one `bonuses` map. Accessors: `levels()`, `levelByName()`, `bonus(name,def)`,
  `hasBonus()`. `load(path)` / `parse(string)`.
- Self-test +6 checks (49 total). Verified vs the REAL morale.txt:
  **5 levels + 67 bonuses** (= 17 events + decay + 43 conditions + 6 difficulty),
  matching the decompile package's breakdown. Spot-checks correct
  (UTOPIA die=620, KID_DIES=-8, EVENT_DEC_RATE=90, DIFF_HARD_PLY=92).
- This closes the sheet-loader morale caveat. Data pipeline (maps/techs/sheets)
  loaders are now complete; not wired into the sim yet.

## 2026-06-01 — Sheet (stat-table) loader

Generic loader for the OPU `sheets/*.txt` tab-separated stat files.

- New `op2::sim::Sheet` (`sim/sheet.{h,cpp}`, pure std C++ in op2sim). One parser
  for all six sheets: auto-detects 1- vs 2-row headers (combines the two into
  column names, e.g. "Common"+"Ore Cost" -> "Common Ore Cost"); skips `;`
  comments + trailing checksum garbage + empty-first-cell rows. Query by column
  name or index: `cell`, `cellInt`, `findRow`, `columnIndex`.
- `load(path)` / `parse(string)` split for testing.
- Self-test +8 checks (43 total). Verified against the REAL files — col×row
  counts match the decompile package exactly: building 32×42, vehicles 19×17,
  weapons 14×29, mines 7×9, space 7×23. Spot-checks correct (BIG_TANK Hit=800;
  RAIL_GUN Concussion/Penetration 80/40).
- NOTE: morale.txt is a MULTI-SECTION file (morale levels + event/condition/
  difficulty bonus sub-tables). The generic loader reads it (9×72) but lumps the
  sub-tables; section-aware morale parsing is a TODO for the morale system.
- Not wired to UI/sim yet. (Reference: OP2SheetsReader, VB.NET, D:\opu\op2sheetsreader.)

## 2026-06-01 — Tech-tree loader

Loader for the OPU `base/techs/*.txt` technology-tree files.

- New `op2::sim::TechTree` (`sim/techtree.{h,cpp}`, pure std C++ in op2sim).
  Parses `BEGIN_TECH "name" id … END_TECH` blocks: CATEGORY, COST, EDEN_COST,
  PLYMOUTH_COST, MAX_SCIENTISTS, LAB, DESCRIPTION, TEASER, IMPROVE_DESC,
  REQUIRES (repeatable), UNIT_PROP (repeatable). `;` comments skipped; leading
  zeros on ids dropped (03408 -> 3408). `byId()` lookup.
- `load(path)` (file) split from `loadFromString()` (testable).
- Self-test extended (+8 checks, 35 total). Verified against the REAL files:
  edentek 135 / multitek 137 / ply_tek 137 / tutortek 95 = **504 techs** —
  matches the decompile package's documented count exactly.
- Not wired into the UI yet — ready for the research/tech system (Phase 3).
  (Reference: OP2TechTree, a VB.NET viewer, at D:\opu\op2techtree.)

## 2026-06-01 — Minimap + one-step zoom-out

- **Minimap**: new `MinimapView` (`render/minimapview.*`, QQuickPaintedItem)
  renders a downscaled map + a white camera **view box**; click/drag jumps the
  camera. Fills the 192×128 sidebar panel. No zoom/map-mode buttons yet.
  - `TilesetManager::tileAverageColor()` (cached) averages each tile; TilesetView
    `buildMinimap()` makes a mapW×mapH image (one averaged pixel per tile).
  - MinimapView reads from TilesetView via a `source` property; repaints on the
    new `TilesetView::cameraChanged()` signal.
- **Zoom**: one zoom-out level (`zoom_` = 1.0 or 0.5) via the mouse wheel
  (QML `WheelHandler` → `TilesetView::zoomAt`, anchored under the cursor).
  Camera/culling/hover/clamp now account for zoom (painter scale+translate).
  Gated by `zoomEnabled` ← `outpost2.ini` `[Game] Zoom` (default 1). Disabling
  resets to 1×.
- Built + linked (Qt MinGW 13.1).

## 2026-06-01 — outpost2.ini config

Settings now come from `outpost2.ini` (at `D:\opu\op2remake\`, path hardcoded
in main.cpp for now).

- `[Game]` keys: `GameSpeed` (1..10), `ScrollRate` (px/tick), `ScrollWindow`
  (0/1 = keep scrolling off-window, OP2 default 1).
- New `Config` QObject (`Outpost2/config.{h,cpp}`) reads the INI via `QSettings`
  (native IniFormat; `;` comments supported), exposed to QML as `config`.
- Applied at startup: `gameClock.setGameSpeed(config.gameSpeed)`; `TilesetView`
  `Component.onCompleted` seeds `scrollSpeed`/`edgeScrollOutside` from config
  (initial values — the sidebar can still change them live).
- Verified: QSettings reads 10/10/1 correctly; full app builds + links.
- TODO: locate the INI next to the exe; persist runtime changes back to it.

## 2026-06-01 — Scroll options: edge-past-window toggle + scroll speed

- **Scroll past window edge** (OP2 behavior): when the cursor leaves the window
  past an edge, the map keeps scrolling that way. Toggleable via
  `TilesetView.edgeScrollOutside` (default ON). When OFF, edge-scroll is bounded
  to inside the window.
- **Scroll speed** setting: `TilesetView.scrollSpeed` (px/tick, edge + arrow
  keys), replaces the fixed constants.
- **Sidebar UI**: a "Scroll" group under the time readout — a clickable
  "Past window: ON/OFF" toggle and a "Speed: − N +" stepper (custom QML, no
  QtQuick.Controls dependency).
- Built + linked (Qt MinGW 13.1).

## 2026-06-01 — RTS edge-scroll + arrow-key scroll (replaces drag)

Switched the camera to OP2/RTS-style controls.

- **Edge scroll**: a ~60 Hz timer checks the cursor against the WINDOW geometry
  (`window()->mapFromGlobal(QCursor::pos())`), so the right edge is past the
  sidebar and all 4 window edges scroll. Only when the window is active and the
  cursor is inside it.
- **Arrow keys** scroll too (held-key state via keyPress/Release, auto-repeat
  releases ignored). `TilesetView` gets `focus: true` in QML.
- **Removed click-drag** panning.
- Camera still clamped to map bounds; culling + crisp tiles unchanged.
- Built + linked (Qt MinGW 13.1).

## 2026-06-01 — Map camera (drag-to-pan) + live tile coords

Made the game view navigable.

- **Drag-to-pan**: `TilesetView` now handles mouse press/move/release (hand-drag
  camera). Camera = viewport top-left in map pixels; clamped to map bounds.
  Open/closed-hand cursor.
- **Tile culling**: `paintMap()` draws only the tiles visible under the camera
  (firstCol..lastCol / firstRow..lastRow), and sets SmoothPixmapTransform=false
  for crisp tiles (both learned from OP2MapperQt).
- **Live coords**: hover (and drag) report the hovered tile in OP2 1-based
  coordinates (top-left = 1,1; 0,0 when off-map) via `hoverTileX/Y` properties;
  the status bar binds to them (replaces the static "0,0").
- Built + linked (Qt MinGW 13.1). Fix: `QQuickItem::setCursor` needs a QCursor,
  not a Qt::CursorShape.
- Colors confirmed correct in-app (standard BMP palette; no BGR swap needed).

## 2026-06-01 — Load + render a real .map on startup

Wired the whole map path: OP2Utility.dll -> op2maps -> op2::sim::Map -> render.
The app now loads `OPU/maps/Nostalgia/opu_02.map` (128x128) at startup and draws
it in the game view.

- **OP2UtilityC C ABI v2**: added tileset-source enumeration
  (`op2_map_tileset_source_count/name/tile_count`) so cells can resolve their
  well filename. DLL rebuilt (FileVersion 1.1.0.0), restaged to
  `third_party/OP2UtilityC/`, and synced to the fork repo.
- **op2::sim::Map** extended: full CellType enum (0..31, matches OP2Utility),
  Tile now carries {tilesetIndex, imageIndex, cellType}, plus
  `tilesetSources` (well filenames).
- **Op2MapLoader** (op2maps) now real: calls the DLL, fills the Map
  (dimensions, tileset sources, per-cell tileset/image/cellType). op2maps links
  the import lib + includes the C header.
- **TilesetManager**: `wellIndexByName("well0001")` lookup (map references wells
  by name; disk order may differ).
- **TilesetView**: loads the map via the registry on startup; `paintMap()` draws
  each cell's tile (fixed camera at origin, no scroll yet); falls back to the
  tileset grid if the map fails to load.
- **CMake**: POST_BUILD copies `OP2Utility.dll` next to appOutpost2.exe (and the
  maps self-test), since it's implicitly linked.
- Verified: headless load test confirms 128x128, sources well0000..well0012,
  valid per-cell tileset/image/cellType; full app builds + links (Qt MinGW,
  Ninja) with the DLL auto-copied beside the exe. Visual result to confirm in
  Qt Creator.

## 2026-06-01 — Tileset loading + first render (game view)

First pixels: load OP2 tilesets and draw them in the game view, ahead of map
parsing. New render layer under `Outpost2/render/`.

- OP2 tilesets confirmed: 8-bit indexed BMPs, 32px wide, stored TOP-DOWN
  (negative BMP height); each "well" is a vertical strip of N 32x32 tiles.
  Assets at `D:\opu\op2remake\OPU\base\tilesets` (13 wells, 2012 tiles) + 1 map
  `OPU\maps\Nostalgia\opu_02.map`. Path hardcoded for now.
- `TilesetManager` (`render/tileset.*`) — loads `well*.bmp` via QImage (Qt reads
  the top-down 8-bit BMPs + palette directly, no custom parser); a tile is the
  sub-rect (0, index*32, 32, 32). Render layer only — the sim deals in tile
  INDICES, never pixels.
- `TilesetView` (`render/tilesetview.*`) — a `QQuickPaintedItem` (QML_ELEMENT)
  that paints all tiles in a grid; placed filling the game view in Main.qml.
  First-render milestone / tileset preview; becomes the map renderer next.
- CMake: render sources added to appOutpost2; `DEPENDENCIES QtQuick` +
  `target_include_directories(... render)` so qmltyperegistrar's generated
  `#include <tilesetview.h>` resolves (fixes a QQuickItem base-type error).
- Verified: full app builds + links (Qt MinGW 13.1, Ninja); headless check
  confirms all 13 wells load with correct tile counts (269/163/6/359/...).
  Visual result to be confirmed by running in Qt Creator.

## 2026-06-01 — OP2Utility wrapped as a self-contained C-ABI DLL

OP2Utility (the OutpostUniverse C++ lib) is designed as a *static* lib with a
C++ class API (exceptions + STL) and no DLL exports — a raw C++ DLL would be
toolchain-fragile. So we wrapped it behind a stable **C ABI** and built it as a
standalone DLL. Lives at `D:\opu\op2utilitydll` (build dir `D:\opu\build`).

- `capi/op2utility_c.h` — flat C interface: opaque `Op2Map*` handle, POD
  out-params, `Op2Result` error codes, `op2_last_error()`. Map-loading surface
  for now (read .map / saved game; width/height/tile-count/version; per-cell
  cellType / tileMappingIndex / tileset / image / lavaPossible). Extend with
  more `extern "C"` fns later (VOL, sprites).
- `capi/op2utility_c.cpp` — wraps `OP2Utility::Map`; every entry point catches
  all exceptions -> error code + message. Nothing C++ crosses the boundary.
- `CMakeLists.txt` — vendor-builds all OP2Utility sources + the wrapper into a
  SHARED lib (`op2utility.dll` + import lib `libop2utility.dll.a`). MinGW link
  uses `-static-libgcc -static-libstdc++ -static` so the DLL is SELF-CONTAINED.
- Built with Qt MinGW GCC 13.1 (Ninja) -> `D:\opu\build\op2utility.dll` (~3.4MB).
- Pure-C `smoketest.c` PASSES with a minimal PATH (build dir only) — confirms
  the DLL loads with no MinGW runtime deps and the exception->error-code path
  works.

To consume from op2remake (next step): include `capi/op2utility_c.h`, link
`libop2utility.dll.a`, ship `op2utility.dll` beside the app, and have
`Op2MapLoader::load()` call `op2_map_read` then fill `op2::sim::Map`.

## 2026-06-01 — Map-loader scaffolding (3 formats, pluggable)

Boilerplate for loading 3 map formats, each an isolated drop-in/drop-out unit.

- New **`Map`** type in op2sim (`sim/include/op2/sim/map.h`) — pure tile grid
  (width/height + Tile{tileIndex, cellType}); the common target all loaders
  produce. CellType enum has the canonical 0..11 passability/cliff values
  (12+ walls/tubes TODO from OP2Utility).
- New **`op2maps`** static library (`Outpost2/maps/`) — NOT firewalled (does
  file I/O + future external deps). Depends on op2sim; linked into the app.
  - `IMapLoader` interface + `MapLoaderRegistry` (dispatch by extension;
    add/remove formats in `withDefaults()` — the single source of truth).
  - Three stub loaders, one file each, returning a clean "not implemented"
    LoadResult with a TODO block pointing at the source project:
      * `Op2MapLoader`        .map  — original binary, via OP2Utility.
      * `OpmLoader`           .opm  — OP2MissionEditor JSON (mission + a
                                       referenced .map via LoadResult.referencedMapFile).
      * `Op2MapperJsonLoader` .json — OP2Mapper JSON (self-contained tiles+cells).
- Self-test (`op2maps_selftest`, CTest): 10 checks — registry size, extension
  helper, per-format routing, unknown-extension handling, stub/error paths.
  Builds clean under Qt MinGW 13.1 `-Wall -Wextra`; all pass.
- Real parsing deferred: will wire OP2Utility + a JSON parser after reviewing
  the project sources + sample files the user is providing.

## 2026-06-01 — Game speeds (1–10), default 10, real-time calibration

The placeholder clock ran at a flat 100ms/tick (far too slow). Added the OP2
game-speed system to `GameClock`.

- 10 speeds (1 = slowest .. 10 = fastest), `gameSpeed` Q_PROPERTY (read/write/
  notify) + `setGameSpeed()` with clamping. Default = 10.
- Tick pacing now derives from speed. Calibrated against the original: ~11 marks
  in 30 s at speed 10 = 1100 ticks / 30 s ≈ 36.67 ticks/sec (~27 ms/tick).
  ticks/sec scales linearly with speed: `tps = 36.67 * speed/10` (lower speeds
  approximate, pending more calibration points).
- UI: added "Speed: N" to the sidebar readout, and number-key hotkeys
  (1–9 = speeds 1–9, 0 = speed 10) via QML Shortcuts.
- Validated: `gameclock.cpp` syntax OK vs Qt headers; `Main.qml` qmllint exit 0
  (only benign unqualified-access notes for the runtime `gameClock` property).

## 2026-06-01 — Sidebar game-time readout (Mark / Tick / real-world timer)

Added a time readout to the sidebar (under the minimap): user-facing **Mark**,
internal **Tick**, and real-world elapsed **Time**.

- New `GameClock` QObject (`Outpost2/gameclock.{h,cpp}`) — APP layer, NOT op2sim
  (it uses Qt + QTimer + QElapsedTimer + wall clock, all banned in the sim core).
  Wraps the pure `op2::sim::Clock`; exposed to QML as the `gameClock` context
  property (set in `main.cpp`). CMake: added the files to `appOutpost2`.
- Tick advancement is a PLACEHOLDER `QTimer` (100ms) until the real game loop /
  pacer drives the clock. Real-world timer via `QElapsedTimer`, shown mm:ss /
  h:mm:ss.
- Mark ratio CONFIRMED = 100 ticks per displayed Mark, via a memory reader on
  the original (Game Tick 735 -> Current Mark 7). This user-facing Mark
  (`floor(tick/100)`) is intentionally SEPARATE from the sim's internal
  `Time() = tick>>2` (4 ticks/mark), which schedules triggers and is unchanged.
  Display ratio lives in one constant `kTicksPerDisplayMark`.
- `gameclock.cpp` syntax-checks clean against Qt headers; `Main.qml` qmllint OK
  (only benign unqualified-access warnings for the runtime context property).

---

## 2026-06-01 — Headless tick loop + cmdlog replay harness

The Phase 1 capstone: the deterministic outer loop that turns "we have a
checksum" into "we can prove determinism." New files in `op2sim`:
`cmdlog.h/.cpp`, `replay.h/.cpp`, plus a sample fixture.

- **`CommandPacket` + `Cmdlog`** (`cmdlog.*`) — a command record (tick, opcode,
  player, payload) and the portable JSONL fixture format (read/write). Includes
  the full opcode-name table (0x00..0x36) + `<START_GAME>` marker, and a small
  hand-rolled flat-JSON parser (no JSON dependency). File I/O is harness-only —
  the per-tick sim never touches disk.
- **`Replay`** (`replay.*`) — the headless tick loop. Drives ticks in order,
  dispatches commands on their tick, steps the sim via callbacks
  (`onCommand`/`onTick`/`checksumFn`), and samples checksums at a cadence. No
  rendering, no wall clock. Mirrors the Python reference `Replay`.
- **`Replay::diff`** — compares two checksum histories and returns the exact
  ticks that diverge (the desync locator).
- **Sample fixture** `sim/tests/fixtures/sample.jsonl` for manual use.
- Self-test extended to 27 checks. Proves: same fixture+seed -> identical
  checksum history; different seed -> divergence detected; an extra command
  shifts the stream only *after* it (tick 0 still agrees); JSONL round-trips
  all fields. Builds clean under Qt MinGW 13.1 `-Wall -Wextra`; all pass.

This completes the Phase 1 determinism backbone (PRNG + clock + checksum +
replay). Every future sim system plugs into `Replay` via the callbacks and is
regression-tested by checksum parity from day one.

---

## 2026-06-01 — UI: minimap panel

Added the minimap to the top of the sidebar in `Outpost2/Main.qml`.

- Fixed **192×128** panel, centered horizontally, 8px from the top of the
  sidebar (the 212px sidebar leaves a ~10px margin each side).
- Placeholder only — no zoom controls or other widgets yet.
- Exposed as `minimapWidth`/`minimapHeight` `Window` properties.
- `qmllint` clean.

---

## 2026-06-01 — State checksum (determinism fingerprint)

Added the sim's state-checksum layer — for lockstep divergence detection and
replay regression. New files: `sim/include/op2/sim/checksum.h`,
`sim/src/checksum.cpp` (added to the `op2sim` target).

- **`hashKernel(byteCount, data)`** — BIT-EXACT port of the engine kernel
  `FUN_0040C050`: XOR each little-endian u32 into the accumulator, right-rotate
  by 1; 0..3 trailing bytes folded by a prologue; result 0 maps to 1; endian-
  stable. The original's unused `prev` arg is omitted. Verified against 5 known
  engine vectors (e.g. dword=1 -> 0x80000000, {1,1} -> 0xC0000000).
- **`Checksummer`** — our streaming accumulator (same XOR+rotate step):
  `addU32 / addI32 / addBytes / value() / reset()`. This is what sim systems
  fold their state into.
- **`checksumCoreState(clock, prng)`** — current whole-sim fingerprint (tick +
  PRNG state); grows as subsystems (units, players, map) are added.
- Decision: this is NOT meant to match the original's full `ChecksumGame` VALUE
  bit-for-bit — that needs the original's exact struct byte layouts, which a
  clean-room clone doesn't reproduce. We keep the KERNEL faithful so original-
  parity stays possible later; our checksum only needs internal consistency.
- Self-test extended to 18 checks (kernel vectors, Checksummer determinism +
  order-sensitivity, core-state stability/sensitivity). Builds clean under Qt
  MinGW 13.1 with `-Wall -Wextra`; all pass.

---

## 2026-06-01 — UI shell: full HUD frame layout

Reshaped `Outpost2/Main.qml` from the default Hello-World window into the OP2
HUD frame, using `QtQuick.Layouts`. Structure: a full-width top bar, then a body
row of [left bar | (game view + status bar) | sidebar].

- **Top bar** (full width) — title strip ("OUTPOST 2").
- **Left bar** — thin fixed-width HUD border on the left edge.
- **Game view** (center, flexes) — placeholder for the map/world renderer.
- **Status bar** (bottom of the game-view column) — coords left, resources right.
- **Sidebar** (right) — FIXED width, full body height. Holds (later) the
  minimap, command buttons, and comm panel.
- Dimensions measured from the real OP2 HUD, exposed as tweakable `Window`
  properties: `topBarHeight = 22`, `leftBarWidth = 12`, `sidebarWidth = 212`,
  `statusBarHeight = 42`.
- Validated with `qmllint` (clean).

---

## 2026-06-01 — Phase 1 start: deterministic core (PRNG + tick clock)

Set up the simulation-core foundation as a standalone, Qt-free static library.

**Added the `op2sim` static library** (`Outpost2/sim/`)
- Separate CMake target so it physically cannot link Qt — this *is* the
  "sim firewall." Integer-only, headless, fully reproducible.
- Wired into the root build: root `CMakeLists.txt` now calls `enable_testing()`,
  `add_subdirectory(sim)`, and links `op2sim` into `appOutpost2`.

**PRNG** — `sim/include/op2/sim/prng.h` (`op2::sim::Prng`)
- Bit-exact port of the engine's dual 32-bit LFSR (taps at bits 13 & 16,
  XOR-combined), from `FUN_0046EFB0/F000/F060` in Outpost2.exe.
- `getRand(n)` uses Lemire's bias-free `(rand * n) >> 32` map (NOT `% n`).
- Seed 0 rewritten to `0x10000001` to avoid the all-zero LFSR death state.
- 8-byte `State` snapshot (`state()` / `setState()`) for save/load + net resync.
- Note: replicated the engine's "always advance state, even for getRand(0)"
  behavior — differs from the Python reference op2_prng.py (which early-returns).
- Used portable bit-fold parity instead of a popcount intrinsic, for
  cross-compiler determinism.

**Tick clock** — `sim/include/op2/sim/clock.h` (`op2::sim::Clock`)
- Single monotonic tick counter; all sim time derives from it (no wall clock).
- `time() == tick >> 2` (1 mark = 4 ticks), matching `TethysGame::Time()`.
- Per-mark gating: `isMarkBoundary()` is true on ticks 3,7,11,… — the gate the
  trigger pass (`FUN_00403230`) uses so triggers fire ONCE PER MARK, not per tick.

**Self-test** — `sim/tests/sim_selftest.cpp` (target `op2sim_selftest`, CTest)
- Regression backbone for Phase 1. Checks: same-seed determinism, seed-0
  handling, state round-trip, getRand range, and clock mark-gating.
- Compile-time asserts on the mark gate live in `src/op2sim.cpp`.
- Verified: builds clean under Qt MinGW 13.1 with `-Wall -Wextra`; all checks
  pass (0 failures). Regression anchor — seed 12345 first 4 rand32:
  `0x181c981c 0x0c0e4c0e 0x06072607 0x03039303`.

**Docs**
- Added root `README.md` and this `CHANGES.md`.
- Added `CONCEPTS.md` explaining the PRNG, Lemire map, and tick/mark gating.

### Decisions on record (see project memory)
- Goal: clean-room **source clone**, NOT binary-ABI-compatible.
- Stack: **C++ + Qt** (cross-platform; decompile is C++ so logic ports verbatim).
- Build order: determinism-first; graphics is a later phase.

### Next up
- State checksum (so two runs can be *proven* identical).
- Headless tick loop + cmdlog replay harness (gives the checksum something to run).
