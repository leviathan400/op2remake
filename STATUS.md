# Status — at a glance

_Snapshot of where the Outpost 2 Remake is. For the phased plan see
[ROADMAP.md](ROADMAP.md); for the running log see [CHANGES.md](CHANGES.md);
for intentional deviations from OP2 see [DIVERGENCES.md](DIVERGENCES.md)._

**In one line:** a deterministic sim core is built and self-tested, all the
content loaders work against real OPU files, and the Qt app loads a real `.map`
and renders **all 38 buildings AND the full vehicle roster** (mappings extracted
from the EXE) — buildings grid-exact with OP2 drop-shadows + faithful one-at-a-time
Normal/Ambient animations; vehicles in all 16 rotations (combat = chassis ⊕ turret),
including 20 cargo-laden truck variants. **`.opm` missions now load into the
deterministic `World` and render (animated) FROM it** — the scene is a view of sim
state, not a hardcoded parade. On a mission the app now runs the sim (`Game::step` each
game tick) and your vehicles drive where you click; no economy yet.
We're at the boundary of **Phase 2 → Phase 3**.

## What works today

### Deterministic sim core (`op2sim`) ✅
- [x] Dual-LFSR PRNG + Lemire range map (SetSeed/Rand pinned to exe reference vectors)
- [x] Tick clock + per-mark gating (`Time() = tick>>2`)
- [x] State checksum (engine kernel port + streaming hasher); `checksumSimState` covers the
      World (units + slot layout)
- [x] Headless tick loop + cmdlog replay harness + divergence diff
- [x] **Game-object model** — `World` unit pool + flat `Unit` struct (owner, tile,
      HP, command, cargo, flags) mirroring OP2's design, but handle-based +
      pointer-free → deterministic, checksummable, and **pointer-free save/load**
      (`serialize`/`deserialize`). `world.{h,cpp}`; see `docs/GAME_OBJECTS.md`
- [x] **Object kind** — every `Unit` carries a `UnitKind` (Building / Vehicle / Weapon / Other),
      set at load (building catalog incl. GuardPost → Building, else Vehicle); in the checksum +
      save format (v2). For gameplay rules (e.g. structure-only weapons), selection, AI.
- [x] **UnitType table + stats from sheets** — per-type `UnitType` in the `World` (kind,
      footprint, **maxHitPoints**, **armor**), built at load from the catalog + `building.txt` /
      `vehicles.txt` (name→Code_Name maps). Spawn reads HP/kind from it (single source of truth,
      not re-looked-up). Hover any object (building / GuardPost / vehicle) → "Name HP cur/max".
- [x] **Object properties (begun)** — structures have two independent status flags:
      **player-Idle** (`UnitFlag::Idle`) and **condition-Disabled** (`UnitFlag::Disabled`,
      future reason: power/workers/damage/EMP). Operate only when neither; OP2-accurate (a
      building can be not-idled yet Disabled). The detail panel's IDLE button toggles Idle
      (via the command layer, below). First of the per-object property model the sim will drive.
- [x] **Command dispatch layer** — the UI is **read-only over the `World`**: every gameplay
      mutation is a `CommandPacket` applied by `op2::sim::applyCommand` (`command.{h,cpp}`),
      exactly as a network peer's packet would be. Idle/Activate flows as real
      **ctMoIdle/ctMoUnidle** opcodes (payload = the target's index+generation handle);
      rejected packets (stale handle, bad payload, unknown op) change nothing. The
      not-yet-simulated Disabled condition is poked through the layer's explicit
      `debugSetDisabled` backdoor until the economy sim derives it. This is the apply side
      of the lockstep design — input → packet → sim — so recording/replaying/networking
      these commands later is wiring, not redesign.
- [x] **Object selection + detail panel ("7th view")** — **left-click selects** the object under
      the cursor (building/GuardPost/vehicle); a green selection box outlines it. **Right-click
      deselects**; with one of your vehicles selected, left-click on empty ground sends it
      there (a ctMoMove packet). The sidebar shows
      a detail panel in OP2's green (`#00ff00`): **portrait box** (art g54) + per-object **category
      icon** (`ui/tech/Tech_*.png`, hardcoded category map), title `Name #player-index`, armor class
      + HP bar, Status (state-coloured), Weapon, and the building **Power/Workers/Scientists**
      requirements from building.txt (0→"None"); an IDLE/ACTIVATE action; a **Debug** section (owner,
      Kind, World ID, location). Tracked by the stable World handle (survives re-renders). Live
      usage + storage pending the economy sim. `selectAt`/`clearSelection`/`setSelectedIdle`/
      `selectedInfo`; `docs/SELECTION_PANEL.md`. TODO: armor-class names, per-vehicle categories,
      vehicle command grid (Stop, Attack, …).
- [x] **Building status light ("computer overlay")** — OP2's dot at each building's
      selection-box corner: **gray = player-Idle** (g49), **red = Disabled** (any condition;
      g48 frame 0, 8-frame flashing for later), **green = Active** (g47); idle>disabled>active.
      Shown only on **own + allied** buildings (from the `.opm` IsHuman/Allies → local player +
      friendly set). GuardPosts: no light when active, gray/red centred when off. `[Render]
      Overlay` + sidebar toggle.
- [x] **op2_art → QML image provider** — `image://op2art/<group>/<frame>` exposes any art
      sprite to the UI (e.g. the OP2 button graphic, group 50). `Op2ArtImageProvider`.
- [x] **Command bar + panels** — OP2's 6 menu buttons under the minimap (art groups 40–45;
      frame 0/1/2 = normal/green-selected/disabled), clickable (radio `selectedCmd`). Selecting
      one swaps in a sidebar **panel** — **all six layouts are built** (OP2 order: Gear→Factories,
      Science→Labs, Truck→Resources, Comms, Spaceship→Space, Sierra→Game). Live data: factory/lab
      lists (double-click → focus+select), mine/smelter counts, current Mark. Sim-blocked (Phase 3):
      production/research status, economy values, message log, satellite status, nav/action buttons.
      Game Options has its 4 buttons (g50; actions TODO). The **dev/debug controls live under the Game
      menu** (scrollable). Spec: `docs/COMMAND_PANELS.md`.
- [x] **Interface skins (begun)** — a `ui` image provider draws the **real OP2 HUD bitmaps**,
      color-keying their `#141414` background to transparent so masked areas don't block the view.
      Skin dir (demo or retail art) from `[UI] SkinDir`; `[UI] UseSkin` toggles the whole thing off
      (back to the hand-drawn frame); nothing bundled. **Layout model: stretchable borders** (OP2's
      resize model — corners fixed, borders stretch, any window size). Pieces in: bottom-right
      (`Bitmap_170`) + bottom-left (`Bitmap_169`) corners. Divergence D2; `docs/INTERFACE_SKINS.md`.
- [x] **Headless play** — `op2inspect` (no Qt): load an op2-rs scenario, step, units,
      region, info, Idle/Unidle orders; session = a `Game` save. docs/HEADLESS.md
- [x] **Vehicle movement** (`movement.h`): Move/Stop orders, step timing from the cell
      speed table and Move Points (damage slows), 16.16 velocity, in-place turns over 16,
      waiting for held tiles.
- [x] **Path planning** (`pathfind.h`): OP2's coarse planner (0x0043F6C0) — greedy octant
      walk, turn collapse, obstacle tracing both ways, detour measure. op2-rs's five obstacle
      scenarios go round the walls the way the original does. Gaps: the AI (RCC) corridor
      search, unit bumping (other vehicles never block a plan), night slowdown, x-wrap.
- [x] **Combat** (`combat.h`): Laser, Microwave, Rail Gun and Cannon tanks — turret aim,
      idle turret moods, box-walk target search with line of sight, shots as objects (lead,
      RNG scatter, range clamp, beam wait / projectile flight, linger), splash damage with
      armor, 5-turn death with an explosion object, retaliation, call-for-help, Attack orders
      with chase. op2-rs's `cmb_duel_laser` plays to the measured damage (32, 35, 28, 35, 32,
      30) and both tanks dead by tick 340. Gaps: scorch marks, walls, the status weapons
      (EMP, foam, ESG, acid, Thor, Starflare/Supernova), muzzle offsets, DIRT, alliances.
- [x] **Guard Posts and structure destruction**: posts fire when powered (no colony economy:
      `Powered`/`Linked` are given), linked posts reach +1 and hit ×1.5, retaliate within 75
      ticks, sweep; destroyed structures play out 10 turns, raze their footprint, drop rubble
      with the RNG, and size-2/3 ones explode on their neighbours. `cmb_structure` takes 60 per
      rail-gun shot and deletes the storage 11 ticks after its last hit, as measured.
- [x] **178 self-test checks** (`op2sim_selftest`; +10 in `op2maps_selftest`),
      integer-only, no Qt/float/clock (firewalled)

### Data pipeline (loaders) ✅ — verified against real OPU files
- [x] `.map` maps — via **OP2UtilityC** (our self-contained C-ABI DLL)
- [x] Tech tree — all **504 techs** (`TechTree`)
- [x] Stat sheets — building/vehicles/weapons/mines/space (`Sheet`)
- [x] Morale — 5 levels + 67 modifiers (`MoraleData`)
- [x] Sprites/art — `op2_art.prt` + `OP2_ART.BMP` -> QImages (`ArtFile`):
      5390 images / 24185 frames, palettes + transparency + **1bpp shadows**
- [x] **Building catalog** — authoritative building→sprite-group mapping pulled
      from `Outpost2.exe` (all **38 buildings**, footprints + 8 states × 2 factions);
      `building_anim_catalog.tsv` + `BuildingCatalog`. Animation overview of every
      building (Normal + Ambients, frame counts) in `building_animations.md`
- [x] **Unit catalog** — vehicle/turret rotation sprites extracted from `Outpost2.exe`
      (chassis arrays at UTI +0x24C/+0x34C, weapon-turret tables, shuttle, 20 cargo-laden
      truck variants); `unit_anim_catalog.tsv` + `UnitCatalog`. **Full vehicle roster now
      decoded** incl. the last stragglers — Robo-Dozer (e0/p343), GeoCon & RepairVehicle
      (real 16-dir arrays) — each pinned 1:1 via its internal code-name accessor
      (`BULL_DOZER`/`GEO_METRO`/`REPAIR_TRUCK`), immune to the nearest-function trap.
      Decode + findings in `Review/decompiled/Unit_Rotation_Animation_Architecture.md`

### App / rendering (Qt Quick) ✅
- [x] HUD frame (top bar, sidebar, status bar)
- [x] **Modular QML** — `Main.qml` is the window/layout shell; the sidebar panel bodies are
      their own files (`DebugPanel`, `ResourcesPanel`, `SpacePanel`, `SelectionPanel`, each
      taking the TilesetView as `view` and owning its sub-page state) and the OP2 palette +
      HUD metrics live in the `Theme.qml` singleton (one place to retune a green)
- [x] Loads + renders a real map (tilesets from `well*.bmp`)
- [x] **All 38 buildings, both faction variants, placed + animated** —
      grid-exact seating via the group pixel-displacement (docks/floor pads land
      on-tile); garage shows its clean build + separate floor pad
- [x] **Faithful animation playback** — one animation at a time (per
      `Unit.DoAmbientAnimation`, NOT simultaneous layers): plays an animation, holds
      its rest frame for `[Render] AnimationGap` ticks, then rotates to the next
      (Normal interleaved with each Ambient so all are shown). Staggered per unit,
      uniform frame pacing, game-speed-tied
- [x] **Full vehicle roster rendered** — every unit in all 16 rotations, grid-aligned
      (one tile each + gap), labels under: ConVec, Lynx/Panther/Tiger (chassis ⊕
      weapon turret), Robo-Miner/Surveyor, Earthworker, GeoCon, RepairVehicle, Scout,
      Evac Transport, Spider, Scorpion, Inter-Colony Shuttle, empty Cargo Truck + 20
      cargo-laden variants (10 cargo types × faction). `placeAllUnits()`
- [x] **Combat-turret showcase** — full matrix: every chassis (Lynx/Panther/Tiger) ×
      all 11 weapons (Laser, RailGun, ESG, Thor's Hammer, Stickyfoam, Acid Cloud, EMP,
      RPG, Microwave, Supernova, Starflare), both factions, 16 rotations. OP2-exact turret
      seating (chassis-frame optional-block mount offset); Tiger uses its own dual sprite
      for gun weapons, and the bomb weapons (Supernova/Starflare) draw the single sprite
      twice stacked. Art- and in-game-verified ("Supernova Lynx" reference)
- [x] **Faction-correct turrets** — most weapon turrets are faction-exclusive (one art);
      the both-faction ones split (Starflare Eden g1623 / Plymouth g1341); `findTurret()`
      picks the chassis faction's art. Weapon→faction matrix decoded (Eden: Laser/RailGun/
      AcidCloud/Thor's; Plymouth: ESG/RPG/Microwave/Stickyfoam; both: Starflare/Supernova/EMP)
- [x] **GuardPost showcase** — every weapon's GuardPost, both factions, 16 rotations. A
      GuardPost is ONE combined sprite per direction (disc base + weapon baked in — its own art,
      distinct from vehicle turrets), drawn directly with idle wander; bombs (Starflare/Supernova)
      are a single static disc+bomb sprite. Turret groups read from the EXE; full decode in
      `Review/decompiled/GuardPost_Rendering.md`. `placeGuardPost()` / `resolveGuardPostTurret()`
- [x] **Idle turret rotation** — military units wander their turret (hull holds), reproducing
      OP2's `AnimateIdleTurret` mode-wander; per-unit decorrelated; bombs stay static
- [x] **OP2 drop-shadows** (1bpp masks, under buildings) — sidebar toggle
- [x] **Tile grid** (32px, over terrain / under buildings) — sidebar toggle
- [x] **Ambient-animation toggles** — sidebar + `[Render]` (`AnimationAmbientStructure`,
      `AnimationAmbientUnit`, `AnimationAmbientResource`)
- [x] **Export PNG** (current viewport) + **Export Map** (entire map at 1:1) — sidebar → `exports/`
- [x] RTS camera: edge-scroll (window-relative) + arrow keys
- [x] Scroll options: past-window toggle + speed (sidebar + `outpost2.ini`)
- [x] One-step zoom out + zoom in (mouse wheel, INI-gated)
- [x] Minimap: downscaled map + white view box, click/drag to jump
- [x] Game-time readout (Mark / Tick / real time), 10 game speeds
- [x] File logger → `logs/op2remake-*.txt`
- [x] `outpost2.ini`: `[Game]` (GameSpeed, ScrollRate, ScrollWindow, Zoom,
      StationaryShadows, MobileShadows, Name) + `[Render]` (AnimationSpeed, AnimationGap,
      AnimationAmbientStructure, AnimationAmbientUnit) + `[Debug]` (Map = startup map,
      relative to OPU/maps/, forward slashes)
- [x] **`.opm` mission loading → World → render** — `MissionLoader` (QJsonDocument) parses a
      `.opm`'s players/units; `loadMission()` spawns them into the deterministic `World`;
      `placeFromWorld()` renders FROM the World pool (the scene is a *view of sim state*, not a
      parallel scene). A `.opm` startup map → mission mode; a `.map` → the static parade (kept as
      fallback). Terrain also loads end-to-end (`OpmLoader` resolves the companion `.map`). Three
      test missions authored for opu_12 (curated/showcase/colony).
- [x] **Mission objects animate** — loaded buildings + units run the SAME animation paths as the
      showcase (shared `placeBuildingInstance`; the per-building art special cases —
      Garage/LightTower — live in ONE shared resolver, `resolveBuildingArt`, so the mission and
      showcase paths can't drift): buildings cycle Normal+Ambients, combat units idle-rotate
      their turrets, bombs stack/stay static. Honors the ambient sidebar toggles.
- [x] **Map resources/markers/wreckage render** — `MasterVariant.TethysGame` `Beacons`/`Markers`/
      `Wreckage` parsed into `MissionResource`s and drawn as `PlacedSprite`s: ore beacons (static
      frame by ore type + bar yield), magma vents + fumaroles + wreckage + objective markers
      (Circle/DNA/Beaker — continuous loops). Art frames cross-checked vs OP2MissionEditor's
      extraction; `ArtFile::groupOfFrame` recovers the animation group from a representative frame.
      Mission vehicles move and turn from the running sim (`TilesetView::syncVehicles` re-seats
      them each frame). Not yet: mission weapons aren't sim fields (no combat in the app);
      per-truck cargo amounts, walltubes unparsed.

### Tooling / external ✅
- [x] **OP2UtilityC** — C-ABI DLL fork of OP2Utility
      ([leviathan400/OP2UtilityC](https://github.com/leviathan400/OP2UtilityC))

## Not done yet
- [ ] **Most of the game isn't simulated** — no economy or research, and the app's mission
      units carry no sim weapon yet, so combat runs only headless (`op2inspect`). The sim
      ticks and vehicles drive; placement rules aren't in. (A plain `.map` still shows the
      fixed parade grid.)
- [ ] The app's GameClock keeps its own tick next to the Game's: the debug panel's Tick/Mark
      count from app launch and don't reset when a map loads, while the Game restarts at 0.
      One clock should drive both.
- [ ] `.json` (OP2Mapper) loader still a stub; `.opm` walltubes not parsed (beacons/markers/
      wreckage now render; units' per-cargo amounts not modelled as sim fields yet)
- [ ] Missions / AI, netcode
- [ ] GPU scene-graph renderer (currently software `QQuickPaintedItem`)
- [ ] Hardcoded paths (`D:\opu\op2remake\OPU`, the INI) → game-folder relative

## Next up (candidates)
1. ~~Render from the `World`~~ ✅ first cut done for `.opm` missions (`loadMission`→`World`→
   `placeFromWorld`). Remaining: a proper `UnitType` table (footprints/HP from sheets) and a
   `weapon` field on `Unit` (weapon is currently stashed in `cargo`); parse beacons/walls.
2. Extend the savegame snapshot beyond `World` (player/map/research/tick/RNG).
3. **Start the sim** — economy/morale formula (data is ready) ticking on the `World`.
4. Research panel (tech tree + sheets → UI).
5. **Wall / tube / dozed-terrain rendering (autotiling)** — TODO. Tile depends on the bulldozed
   terrain underneath + adjacent same-type objects (neighbour-bitmask → connected variant; placing
   one updates its neighbours). Placed/stored but not drawn connected yet. See `OP2_BUILDINGS.md`.
