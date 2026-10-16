# Game objects — the `World` model

How the remake stores in-world objects (buildings + units): owner, position, HP,
command, cargo, animation — the state save/load snapshots. Lives in the
deterministic `op2sim` core (`sim/include/op2/sim/world.h`, `sim/src/world.cpp`).

OP2's own design (the model we mirror) is documented for remakers in
`D:\Oupost2Decompile\Remake\docs\GAME_OBJECT_STORAGE.md`.

## Design — mirror OP2, but pointer-free

OP2 packs every object type into one fixed `Unit` struct (0x54), threaded through a
global pool + intrusive linked lists, with per-type constants in `UnitTypeInfo`, and
`OnSave`/`OnLoad` virtuals that swizzle pointer fields ↔ indices. We keep the same
split but make instance state **integer-only and handle-based**, so it sits inside the
firewalled sim core and serialises as plain bytes (no pointer translation):

| OP2 | Ours |
|---|---|
| `Unit` struct (0x54), pointer links | `Unit` POD (`world.h`) — no pointers |
| global fixed pool + `next/prev/playerNext` lists | `World` — slot vector + free-list |
| `unitIndex` (16-bit) references | `UnitId` handle = `{index, generation}` |
| `UnitTypeInfo` (per-type) | `UnitType` table (footprint, maxHP, kind) |
| `OnSave`/`OnLoad` pointer swizzle | `serialize()`/`deserialize()` — plain bytes |

## `Unit` (per-instance state)

Flat, integer-only (deterministic + checksummable + memcpy-cheap): `typeId`, `owner`,
`rotation` (0..255, `>>4` = 16 facings as OP2), `tileX/tileY`, `hitPoints`,
`command/action`, `cargo` + `cargoAmount`, `reloadTimer`/`actionTimer`, `frame`, and a
`flags` bitfield (`IsEden`, `IsDead`, `IsEMPed`, `Disabled`, …). Sprite groups are NOT
here — they live in the render-layer catalogs keyed by the same `typeId`.

## `World` (the pool)

A slot vector + free-list — O(1) `spawn`/`destroy`, stable handles:

- `spawn(Unit)` → `UnitId`; reuses a freed slot when available.
- `destroy(UnitId)`; `get(UnitId)` returns `nullptr` for a dead/stale handle.
- `forEach(fn)` — visit live units in slot order (deterministic).
- `foldInto(Checksummer&)` — fold every live unit into the determinism fingerprint.
- `serialize()` / `deserialize()` — snapshot/restore the whole pool.

**Stale-handle safety via generations.** Each slot has a generation, bumped on reuse;
a `UnitId` carries the generation it was minted with, so a handle into a freed-and-
reused slot resolves to `null` (no dangling references). A default `UnitId` is invalid.

## Save / load — essentially free

Because no field is a pointer, a save is just the slot array + free-list + counters,
written little-endian field-by-field (`serialize()`), with a magic + version header.
`deserialize()` validates and restores — and the **state checksum matches across the
round-trip** (a self-test asserts this), so a load is provably identical state. This is
the same snapshot we'll extend with player/map/research/tick/RNG state for full
savegames. Determinism + the checksum/replay infrastructure already in the sim core
mean save/load doubles as lockstep-resync.

## Where it plugs in

- **Authoritative state:** `World` in `op2sim` (no Qt, no floats — firewalled, ticked,
  checksummed, replayable).
- **Types:** `UnitType` table from the stat sheets + the building/unit catalogs
  (populated at mission load by `TilesetView::populateType`).
- **Mutations from outside:** ONLY through the command layer (`command.h`):
  `applyCommand(world, packet)` decodes + performs a CommandPacket (ctMoIdle/
  ctMoUnidle so far) — the render/UI layer builds packets, never writes World
  state itself. That one-way rule is what makes lockstep + cmdlog replay work.
  (`debugSetDisabled` is the layer's explicit backdoor for the not-yet-simulated
  Disabled condition.)
- **Rendering:** in `.opm` mission mode `TilesetView::placeFromWorld()` derives the
  scene from the `World` — `PlacedBuilding`/`PlacedUnit` are a render cache built
  from `(Unit, UnitType)`, not the source of truth. (A plain `.map` still shows
  the static catalog parade.)

## Status

- [x] `Unit` / `UnitType` / `UnitId` / `World` (spawn, destroy, handles, forEach)
- [x] Deterministic checksum fold
- [x] Pointer-free save/load round-trip (`serialize`/`deserialize`)
- [x] `UnitType` table populated from sheets/catalogs at mission load
- [x] Renderer reads from `World` for `.opm` missions (`placeFromWorld`)
- [x] Command dispatch layer (`command.h`): ctMoIdle/ctMoUnidle packets; UI is
      read-only over the World
- [x] Covered in `op2sim_selftest` (World pool + command-dispatch sections)
- [ ] Player/map/research state in the same snapshot for full savegames
- [ ] Sim ticks the `World` (movement/economy) — commands beyond Idle/Unidle
