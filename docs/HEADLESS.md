# Headless play — `op2inspect`

Drive the simulation without the app: load a scenario, step ticks, look at the units
and the map as text. Built with the rest (`cmake --build Outpost2/build_rel`), no Qt:
`Outpost2/tools/inspect/` links only `op2sim` and `op2maps`.

```
op2inspect load --scenario cmb_duel_laser.json --game-dir "C:/Games/Outpost 2"
op2inspect step --ticks 40
op2inspect region
op2inspect units
op2inspect info
op2inspect order --unit 0 --cmd Idle
```

| Command | Does |
|---|---|
| `load --scenario FILE [--game-dir DIR]` | start a session at tick 0 (`--game-dir` or `$OP2_DIR`: an OPU 1.4.1 install) |
| `step [--ticks N]` | run N ticks (default 1), applying the scenario's orders on their ticks |
| `units` | live units in processing order: spawn index, type, object index, owner, pixel, tile, heading, HP, command/action, Idle/Disabled |
| `region [--x --y --w --h]` (also `sim_region`) | terrain by cell type, with units drawn as their spawn index; default the units' area |
| `info` | scenario, map, tick, RNG state, checksum, live units |
| `order --unit N --cmd Move --x X --y Y` (or `Stop`, `Idle`, `Unidle`) | queue a command packet for the next tick |

Every command takes `--session FILE` (default `op2inspect.session` in the current
directory), so several sessions can run side by side.

## Scenarios

The format is op2-rs's (its `ENGINE_PLAN.md`, "Scenario format"), so its oracle
scenarios (`harness/oracle/scenarios/*.json`) load unchanged: `map`, `seed`, `setcell`
(`rect` + `cell`), `spawn` (`type` from the sheets' `Code_Name`, `owner`, `tile`, `dir`,
`weapon`) and `orders` (`tick`, `unit` = spawn index, `cmd`). Tiles are map-file tiles; a
structure's tile is its footprint's centre. Player 0 is Eden and player 1 Plymouth, as in
op2-rs's harness.

- **Maps** are found beside the scenario, then under `OPU/base/maps/**` and `OPU/maps/**`
  (first match, sorted, case-insensitive), then in the game folder. OPU 1.4.1 ships the
  contents of the original `.vol` archives as loose files under `OPU/`, so no archive
  reader is needed: every map op2-rs's scenarios use (`cm01`, `newworld`, `eden04`,
  `eden09`) is under `OPU/base/maps/`.
- **Unit types** come from `vehicles.txt` and `building.txt` (`OPU/base/sheets`, else
  `OPU/sheets`, else `sheets`): kind, footprint (`X-size`/`Y-size`), hit points, armor,
  power/workers/scientists.
- **Orders**: only what the command layer implements is carried out (`Move`, `Stop`,
  `Idle`, `Unidle`). The rest (`Attack`, `Patrol`, ...) print "not simulated yet" on their
  tick. An order with `"tick": T` is queued when the session is at tick T and taken in the
  step to T+1; the unit acts on it in the step after (op2-rs's FINDINGS "Orders"; not yet
  compared against a recorded run, so this may be one tick off the original's numbering).
- `weapon` is read but not stored: `Unit` has no weapon field yet.

## The session file

`O2IS`, a version, the scenario path, the game folder, the spawn-index → `UnitId` table
and a `Game` save (`sim/include/op2/sim/game.h`). Every call restores the save, runs,
and writes it back, so the CLI exercises save/restore all the time: 500 ticks in one
call and 5 × 100 in five calls end on the same checksum. The map and unit types are not
saved; each call rebuilds them from the scenario and the sheets.

## What it can and can't do yet

Vehicles drive (`sim/include/op2/sim/movement.h`) along OP2's planned routes
(`pathfind.h`): `line_fast.json` takes its Lynx the 30 fast tiles at 24 ticks each, onto
(42, 22) at tick 731, and op2-rs's `obstacle_symmetric`, `obstacle_wall`, `obstacle_north`,
`obstacle_pocket` and `terrain_detour` go round (or, for slow ground, straight through) the
way the original was measured to. Tanks fight (`combat.h`): `cmb_duel_laser` plays out
as in op2-rs's inspector, the hits landing 32, 35, 28, 35, 32, 30 as the original was
measured and both tanks dead by tick 340; `units` shows each gun's weapon, turret, reload
and target, the shots in the air and the explosions. Scenario `Attack` orders (`target` =
spawn index) work; `--cmd Attack` isn't in `order` yet. Each system plugs into `Game::step` (`sim/src/game.cpp`) and shows
up here with no CLI change.
