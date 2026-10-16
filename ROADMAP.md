# Outpost 2 Remake — Roadmap

The phased build plan. Determinism first, graphics later — we build a simulation
we can *prove* correct before we put a face on it.

Status legend: ✅ done · 🔨 in progress · ⬜ not started

---

## Guiding decisions (locked)

- **Goal:** a clean-room **source clone** of Outpost 2 — the game's rules and
  behavior, rebuilt from scratch. **Not** binary-ABI-compatible; original
  compiled mission DLLs will not load (missions get re-authored later).
- **Stack:** **C++20 + Qt 6 (Qt Quick) + CMake**, cross-platform.
- **Spec source:** the reverse-engineering package at
  `D:\Oupost2Decompile\Remake\` is treated as a *behavioral spec* — we port
  algorithms, formulas, and tuning data, not byte layouts.

## The one principle everything hangs on — the sim firewall

The simulation core (`op2sim`) is **integer-only, headless, and forbidden from
touching Qt, floating-point, or any wall-clock/OS time**. It's a separate CMake
target so it *cannot* link Qt. Everything time-based runs off the sim tick.
This is what makes multiplayer lockstep, replays, and save/load possible — and
it's the same rule no matter which phase we're in.

```
┌─ platform (Qt shell: window, input, audio, timing, net) ─┐  app layer
├─ render   (viewport: tiles + sprites, reads sim state)   ┤  app layer
├─ ui       (HUD chrome: sidebar, menus, reports)          ┤  app layer
├──────────────────── the firewall ───────────────────────┤
└─ op2sim   (PRNG · clock · checksum · replay · …)         ┘  pure sim core
```

---

## Phases

| # | Phase | Status |
|---|---|:---:|
| 0 | Scaffold & determinism contract | ✅ |
| 1 | Deterministic core & replay harness | ✅ |
| 2 | Data pipeline (sheets, techs, maps, assets) | 🔨 |
| 3 | Simulation systems (economy → combat → path → triggers) | ⬜ |
| 4 | Renderer & viewport | ⬜ |
| 5 | UI chrome | 🔨 (started early, opportunistically) |
| 6 | Missions & AI | ⬜ |
| 7 | Netcode (lockstep) | ⬜ |

> The first *playable-with-graphics* moment is Phase 4 — late on purpose. By then
> the hard part (a correct, tested simulation) is already done, instead of being
> debugged through a renderer. That inversion is why this plan doesn't stall.

---

### Phase 0 — Scaffold & determinism contract ✅
- ✅ CMake project, Qt Quick app, `op2sim` static library.
- ✅ The sim firewall: `op2sim` is a standalone target that does not link Qt.
- ⬜ A fixed-point type for the few places fractional math is needed (deferred
  until a system actually needs it; sim is pure-integer so far).
- ⬜ CI building on Win/Linux/Mac. (Local Qt MinGW build works today.)

### Phase 1 — Deterministic core & replay harness ✅
- ✅ **PRNG** — dual-LFSR (taps 13/16) + Lemire range map (`prng.h`).
- ✅ **Tick clock** + per-mark gating, `Time() = tick>>2` (`clock.h`).
- ✅ **State checksum** — faithful `FUN_0040C050` kernel + our streaming
  `Checksummer` (`checksum.*`). Internally consistent (not bit-exact vs the
  original — a clean-room choice).
- ✅ **Headless tick loop + cmdlog replay** — `Replay` drives ticks, dispatches
  commands, samples checksums; `diff()` locates divergence; JSONL fixtures
  (`cmdlog.*`, `replay.*`).
- ✅ **Command dispatch** — `applyCommand(World&, CommandPacket)` (`command.*`) is the only
  mutation path into the World from outside the sim; ctMoIdle/ctMoUnidle wired (the UI's
  Idle/Activate flows as real packets).
- ✅ 78 self-test checks pass (`op2sim_selftest`); +10 in `op2maps_selftest`.

**Done when:** a recorded command stream replays to a stable per-tick checksum. ✓

### Phase 2 — Data pipeline 🔨
Give the sim real numbers. The `parse_*.py` tools in the spec are the reference.
- 🔨 Map loaders — pluggable `op2maps` lib + `op2::sim::Map`.
  - ✅ `.map` working via **OP2UtilityC** (our self-contained C-ABI DLL fork of
    OP2Utility). Fills tiles {tilesetIndex, imageIndex, cellType} + tilesetSources.
  - ✅ `.opm` (OP2MissionEditor) — terrain (companion `.map`) **plus** units/buildings/
    beacons/markers/wreckage parsed and spawned into the `World` (`render/missionloader`).
  - ⬜ `.json` (OP2Mapper) loader still a stub.
- ✅ Sheet loader (`op2::sim::Sheet`) — generic TSV parser for the 5 flat stat
  sheets (building/vehicles/weapons/mines/space). Plus `op2::sim::MoraleData`
  for the multi-section morale.txt (5 levels + 67 modifiers). 🔨 **First wiring:**
  building HP (`building.txt`) is now stamped onto spawned structures' `hitPoints`
  (name→Code_Name map). Vehicles/other stats still to wire.
- ✅ Tech tree loader (`op2::sim::TechTree`) — parses all 504 techs (REQUIRES
  graph + UNIT_PROP overrides) from `base/techs/*.txt`. Not wired to UI yet.
- ⬜ `.vol` reader to load the player's own installed assets.

**Done when:** the headless sim loads a real map + real stats and ticks on them.

### Phase 3 — Simulation systems ⬜
Build game logic, cheapest-to-verify first (each plugs into `Replay`):
0. 🔨 **Object-property model (pulled forward).** Every `Unit` now carries `kind`
   (Building/Vehicle/Weapon/Other) and status flags — structures: player-`Idle` +
   condition-`Disabled` (reasons: power/workers/scientists/damage/EMP); units:
   afflictions (`IsEMPed`/`IsStickyfoamed`). In the checksum + save format (v2). Not
   yet *driven* by the sim — the UI sets Idle via ctMoIdle/ctMoUnidle command packets
   (`command.h`) and pokes Disabled via the layer's debug backdoor — but the data
   model and the command path are in place.
1. ⬜ Economy & morale (pure formulas; `Player::Recalc`, 84-DWORD morale table).
2. ⬜ Units & the action ring (1025-slot model, 8-way rotation).
3. ⬜ Combat (integer damage formula, no RNG → trivially testable).
4. ⬜ Pathfinding (greedy + bounded A*).
5. ⬜ Disasters & triggers (3-stage lifecycle; 17 trigger types).

**Done when:** a full match plays out headlessly — checksummed and reproducible.

### Phase 4 — Renderer & viewport 🔨
- ✅ Tileset loading (`well*.bmp` via QImage) + map render in `TilesetView`
  (`QQuickPaintedItem`, software for now). Tile culling; crisp scaling.
- ✅ Camera: RTS edge-scroll (window-relative; optional scroll-past-window) +
  arrow keys + scroll-speed setting. Live 1-based tile coords in the status bar.
- ✅ Minimap render (averaged-tile image) + white view box, click/drag to jump.
- ✅ Zoom: one zoom-out level (mouse wheel, anchored), gated by `outpost2.ini`.
- ✅ Sprite loading from `OP2_ART` — `ArtFile` parses op2_art.prt + OP2_ART.BMP and
  composites frames. **All 38 buildings + full vehicle roster** mapped (type→sprite-group,
  extracted from the EXE) and drawn: buildings grid-exact with drop-shadows + one-at-a-time
  Normal/Ambient animation; vehicles in 16 rotations (chassis⊕turret); GuardPosts
  (base+turret); ore beacons/vents/fumaroles/markers/wreckage; building status overlay lights.
- ✅ **Draws from sim state** — in `.opm` mission mode the scene is derived from the `World`
  (`placeFromWorld`), not a hardcoded scene. (A plain `.map` still shows the catalog parade.)
- ⬜ Migrate the world layer to the GPU scene graph (`QSGNode`) for realtime.
- ⬜ Selection, fog of war.
- ⚠️ **Manageability:** `render/tilesetview.cpp` has grown large (showcase + mission render +
  animation + camera/input). Split into per-responsibility translation units (see CHANGES).

**Done when:** you watch a Phase-3 match play out on screen.

### Phase 5 — UI chrome 🔨
The half of OP2 that's interface. (Started early for visible momentum.)
- ✅ HUD frame: top bar, left bar, game view, status bar, fixed sidebar.
- ✅ Minimap panel (192×128 placeholder).
- ✅ Game-time readout (Mark / Tick / real-world timer) via `GameClock`.
- ✅ 10 game speeds (default 10), number-key hotkeys.
- ⬜ Command-button row, build menus, comm/messages panel, JUMP button.
- ⬜ Reports, tech/research UI, mission briefings.
- 🔨 Input → command-packet generation (feeding the same packets the sim drains):
  first commands flow — IDLE/ACTIVATE and the status click-toggle build real
  ctMoIdle/ctMoUnidle packets applied by `op2::sim::applyCommand`. Move/build/attack
  arrive with their Phase-3 systems.

**Done when:** a skirmish is playable end-to-end with mouse + keyboard.

### Phase 6 — Missions & AI ⬜
- ⬜ A clean internal mission/group/trigger API (mirroring ScGroup / FightGroup /
  Pinwheel / the 17 trigger types, in our own structs).
- ⬜ Then choose authoring model: source-coded, data-driven, or scripted.

**Done when:** a re-authored mission with a working computer opponent is playable.

### Phase 7 — Netcode (lockstep) ⬜
- ⬜ Lockstep UDP; ship the command stream, gate ticks, cross-check checksums.

**Done when:** two clients play a synchronized multiplayer match. (De-risked
early — determinism was already proven in Phase 1.)

---

## Current status (2026-06-10)

**Phase 1 backbone complete.** The deterministic core (PRNG → clock → checksum →
replay) is built and self-tested (78 checks), so every future system is regression-tested
by checksum parity from day one. The **command dispatch layer** is in: the UI is read-only
over the `World`, with every gameplay mutation flowing as a CommandPacket through
`applyCommand` — the exact seam Phase 7's lockstep netcode and cmdlog recording need.

**Content pipeline + rendering well ahead (Phases 2/4/5 pulled forward).** The app loads a
real map (`.map` via OP2UtilityC, or a `.opm` mission) and renders terrain + the full
building/vehicle/GuardPost art set, with animations, drop-shadows, idle turret wander, and
building status overlay lights. **`.opm` missions load their objects into the deterministic
`World` and the scene is rendered from it** (sim-backed, not a hardcoded showcase). The
object-property model has begun (`UnitKind`, structure Idle/Disabled status). HUD frame,
minimap, game-time readout (10 speeds, proper fixed-cadence loop), RTS camera, scene-switch
+ export buttons, and sidebar toggles are in. Camera is still a software `QQuickPaintedItem`;
GPU scene-graph migration is later.

**Next big rocks:** (1) the real `UnitType` table (footprint/HP/kind from the stat sheets),
(2) the sim *tick* mutating the World (movement → economy → combat), (3) keep the render layer
manageable — split `tilesetview.cpp`.

## Open questions / caveats carried forward

- **Bit-exact vs. faithful.** Matching the *original's* literal checksum/replay
  needs its exact struct byte layouts — a clean-room clone doesn't reproduce
  those. We target internally-consistent determinism; original-recording parity
  is an optional later effort.
- **Tick rate.** Measured ~40 ticks/sec at game speed 10 (memory reader),
  matching the documented ~40 Hz nominal. (A spec doc says "60 ticks/sec" but
  that's not borne out empirically.) The real engine paces ticks with an
  adaptive EWMA and may run uncapped on fast hardware; our per-speed fixed
  interval is a placeholder until the real game loop is built.
- **"Mark" is overloaded.** Three scales: our `tick` (~40 Hz logic step; op2a's
  docs call *this* a "mark"), `Time() = tick>>2` (trigger unit), and the
  player-facing **Mark = tick/100** shown in the UI. Don't conflate them.
- **Map / save formats** are marked best-effort in the spec — validate on first
  use (Phase 2).
