# Outpost 2 — Remake

A clean-room, open-source remake of the 1997 RTS **Outpost 2: Divided Destiny**.
This is a from-scratch reimplementation of the *game* (its rules, formulas, and
behavior) — **not** a binary-compatible engine. It does not reproduce the
original's binary ABI, and existing compiled mission DLLs will not load.

Built from the reverse-engineering spec in `D:\Oupost2Decompile\Remake\`.

## Stack

- **Language:** C++20
- **UI / app shell:** Qt 6 (Qt Quick)
- **Build:** CMake
- **Platforms:** Windows / Linux / macOS (cross-platform via Qt)

## Layout

```
REMAKE/
├── README.md          ← you are here
├── STATUS.md          ← at-a-glance: what works today, what's next
├── ROADMAP.md         ← the phased build plan + current status
├── CHANGES.md         ← dev log (what we did, newest first)
├── CONCEPTS.md        ← plain-English explanations of the engine concepts
├── LEARNINGS.md       ← gotchas & hard-won lessons (read before touching art/build)
├── DIVERGENCES.md     ← intentional deviations from OP2, with rationale
├── docs/              ← design notes per subsystem (panels, selection, skins, …)
├── outpost2.ini       ← [Game]/[Render]/[UI]/[Debug] settings (sits next to the exe)
├── building_anim_catalog.tsv ← building → sprite-group map (from the EXE)
├── unit_anim_catalog.tsv     ← unit → rotation-sprite map (from the EXE)
├── art_group_catalog.tsv     ← all 2,079 .prt groups, classified
└── Outpost2/          ← the application + source
    ├── CMakeLists.txt
    ├── main.cpp        ← Qt app entry point
    ├── Main.qml        ← window/layout shell; panels in their own .qml files
    │                      (DebugPanel, ResourcesPanel, SpacePanel, SelectionPanel)
    │                      + the Theme.qml singleton (OP2 palette/metrics)
    ├── logger.{h,cpp}  ← file log -> logs/op2remake-*.txt
    ├── render/         ← the QQuickPaintedItem game view + art/catalog loaders
    │                      (read-only over the World; writes go via sim commands)
    ├── maps/           ← op2maps: pluggable map-format loaders (.map/.opm; no Qt)
    └── sim/            ← op2sim: the deterministic simulation core (NO Qt) —
        │                  PRNG, clock, checksum, World, command dispatch,
        │                  cmdlog/replay, sheet/techtree/morale parsers
        ├── include/op2/sim/
        ├── src/
        └── tests/             sim_selftest.cpp (the regression backbone)
```

### The sim firewall

`sim/` (the `op2sim` library) is the deterministic game core. It is **forbidden
from depending on Qt, floating-point math, or any wall-clock/OS time source** —
integer-only and fully reproducible. It is a standalone CMake target precisely
so it *cannot* link Qt. Everything time-based runs off the sim tick, never a real
clock. This is what makes multiplayer lockstep and replays possible.

## Building

Open `Outpost2/CMakeLists.txt` in Qt Creator (Qt 6.11 / MinGW kit), or:

```
cmake -S Outpost2 -B build
cmake --build build
```

## Running the tests

The simulation core ships a self-test (the regression backbone):

```
ctest --test-dir build
# or build & run the `op2sim_selftest` target directly
```

## Status

See **[STATUS.md](STATUS.md)** for the at-a-glance snapshot. In short:

- **Deterministic sim core** (`op2sim`) — PRNG, tick clock + per-mark gating,
  state checksum, headless tick loop + cmdlog replay, the `World` object pool,
  and a **command dispatch layer** (the UI mutates the World only via
  CommandPackets — the lockstep seam). Self-tested; current check counts in
  [STATUS.md](STATUS.md).
- **Data pipeline complete** — loaders for `.map` maps (via the OP2UtilityC DLL),
  the 504-tech tree, the stat sheets, morale, and the **building→ and unit→sprite-group
  catalogs extracted from the EXE**, all verified against the real OPU files.
- **Renders a real map + all 38 buildings + the full vehicle roster** — the Qt app
  loads an OP2 `.map` and places every building grid-exact (via the sprite group's
  pixel-displacement, so docks/floor pads land on-tile), with OP2 drop-shadows and
  faithful **one-at-a-time** Normal/Ambient animations (matching
  `Unit.DoAmbientAnimation`). Every **vehicle** is shown in all 16 rotations
  (combat units = chassis ⊕ weapon turret composited), including the 20 cargo-laden
  truck variants. Plus RTS edge/arrow scrolling, minimap, zoom, a tile-grid toggle,
  a shadow toggle, and PNG export of the viewport.
- **Not running yet** — nothing consumes the loaded content for *gameplay*; the
  buildings + units are a catalog showcase, not sim-driven. The simulation
  (units/economy/combat) isn't wired in. We're at Phase 2 → Phase 3.

See [ROADMAP.md](ROADMAP.md) for the phased plan, [CHANGES.md](CHANGES.md) for
the running log, [CONCEPTS.md](CONCEPTS.md) for what each piece is and why it
matters, and [LEARNINGS.md](LEARNINGS.md) for the gotchas we hit (read it before
touching the art/render code or the build).

## Sharing a build (Windows)

The build does not bundle OP2 assets — it reads them from an Outpost 2 1.4.1 /
OPU install pointed to by `outpost2.ini`'s `[Game] Outpost2=`.

To make a self-contained, zip-and-share folder:

```
cmake -S Outpost2 -B build_rel -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_rel
# bundle Qt + MinGW runtime next to the exe:
windeployqt --release --qmldir Outpost2 --compiler-runtime build_rel/appOutpost2.exe
```

The result (exe + Qt DLLs + `platforms\qwindows.dll` + MinGW runtime +
`OP2Utility.dll` + `outpost2.ini`) runs on any Windows box with no Qt installed.
**The recipient sets `Outpost2=` in `outpost2.ini` to their OP2 install folder.**

## License / assets

The remake source is original work. **No Outpost 2 game content is
redistributed** — you must own a licensed copy (GOG release recommended); the
game loads assets from your own install.
