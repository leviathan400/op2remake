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
`rotation` (chassis heading, 0 = east, 64 = south, clockwise; `>>4` = 16 facings) and
`turret` (same scale, starts at 0), `px/py` + `subX/subY`, `damage`,
`command/action`, `cargo` + `cargoAmount`, `reloadTimer`/`actionTimer`, `frame`, and a
`flags` bitfield (`IsEden`, `IsDead`, `IsEMPed`, `Disabled`, …). Sprite groups are NOT
here — they live in the render-layer catalogs keyed by the same `typeId`.

**Position is a pixel, as in OP2** (`coords.h` has the rules and their sources): world
pixels in the map file's frame, y down. A vehicle's pixel is its centre (a tile centre
when standing); a structure's is its footprint centre, so a 2-wide one sits on a tile
edge. The `.opm` Position is the footprint's centre tile, and the top-left tile is
`structureTopLeft(px, size)`. While a unit steps, `subX/subY` hold the 16.16 fraction
(biased 0x8000). The tile is always derived from the pixel, never stored beside it.

**Damage, not hit points.** OP2 stores damage taken (MapObject +0x1E) and reads max HP
from the type, per player, so a max-HP upgrade raises current HP too. `World::hitPoints(u)`
= max − damage, floored at 0.

## `World` (the pool)

A slot vector + free-list — O(1) `spawn`/`destroy`, stable handles:

- `spawn(Unit)` → `UnitId` with OP2's index (below); invalid when 0x3FF objects are live.
- `destroy(UnitId)`; `get(UnitId)` returns `nullptr` for a dead/stale handle.
- `forEach(fn)` — visit live units in slot order (deterministic).
- `processUnits(fn)` / `forEachInOrder(fn)` — the **processing order**, OP2's object
  list: sorted by pixel y, a new object in front of the first one at or below its row
  (0x439B53), a stable re-sort by pixel y after every object has run (0x43E8F0). Objects
  made mid-tick join without shifting the one being processed. It decides who wins a
  contested tile and is also the draw order.
- `foldInto(Checksummer&)` — fold the slot layout, the processing order and every live
  unit into the determinism fingerprint.
- `serialize()` / `deserialize()` — snapshot/restore the whole pool (validated).

## Object indices — OP2's allocator

`UnitId::index` is the object index exactly as OP2 hands it out, decoded from
Outpost2.exe (GOG 1.2) by hand from the bytes:

| | Outpost2.exe |
|---|---|
| Globals | object count `0x54F834`, last index `0x54F838`, freed ring tail `0x54F83C`, head `0x54F840`, ring array `0x54F844`, records `0x54F848` (0x78 bytes each, index at +0x10), list head `0x54F84C` |
| Allocate (`0x439A10`, stdcall, 4 args) | fail if count == 0x3FF; `++count`; scan the ring from head to tail, stepping `& 0x3FF`, for the first entry with `freedTick + 0x80 < tick` (signed compare, current tick `0x56EB1C`) and reference byte `+0x32 == 0`; take it, move the entries between head and it up one, `head = (head+1) % 0x3FF` (`idiv`); none → fresh index `++last`, failing at 0x3FF *without* undoing `++count` |
| Free (`MapObject::~MapObject` `0x439C20`, twin `0x439C90`) | `--count`; `ring[tail] = obj`, `tail = (tail+1) & 0x3FF`; unlink from the list, `next = -1`; `[obj+8]` (the prev link while alive) = the tick it was freed |
| Start | nothing writes `0x54F838`/`0x54F840` by address: zero, so the first index is 1 |

Two exe quirks are kept: the head wraps at 0x3FF while the tail wraps at 0x400, so after
1023 reuses an entry is dropped and a fresh index is taken (self-test 19d); and a failed
fresh allocation leaves the count one higher.

The index sets the 8-tick heartbeat, `isHeartbeatTick(index, tick)` = `(index ^ tick) & 7
== 0` (0x43DF6C, 0x4A7C03), and an idle turret's mood, `idleMood` = `(index ^ tick >> 8) &
15`. So units built later in a game act on ticks that depend on how many objects came
and went before them. Weapon fire takes indices from the same pool in OP2. When shots
and effects arrive they must allocate here too, or every later unit's phase drifts.

`World::setTick` gives spawn/destroy the current tick (OP2 reads its global); the game
loop sets it each tick. `addRef`/`releaseRef` are the +0x32 byte: a target or guard
link keeps a dead unit's index from being reused.

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
