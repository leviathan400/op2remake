# Learnings — gotchas & hard-won lessons

Non-obvious things we discovered the hard way. Read this before touching the
relevant area; each entry is **what bit us → the fix**. Conceptual background
lives in [CONCEPTS.md](CONCEPTS.md); the running log is [CHANGES.md](CHANGES.md).

---

## Art & rendering

### Shadow images (art type 4/5) are 1bpp with a *non-obvious stride*
**Bit us (badly — a multi-round loop):** shadows rendered as speckle, then we
"fixed" the phantom with morphology/box-blur/opacity and got square or blocky
shadows.
**The truth:** type 4/5 images are **1bpp masks**, **MSB-first**, row stride
**`((width + 31) / 32) * 4`** bytes (1bpp packed to a 32-bit boundary). The image
header's `pitch`/ScanLineByteWidth is the **8bpp-style pixel width and must NOT be
used** as the shadow stride. Decoding with the 8bpp pitch scrambles the solid mask
into what *looks* like a 2-on/2-off dither — which is what sent us chasing
smoothing tricks. A set bit halves the destination (~50% black); cleared = clear.
**Lesson:** when replicating a reference renderer's output, **read the reference
source first**. OP2ArtReader `CPrtFile.vb` `RenderShadow()` stated the stride
plainly and would have saved the whole loop.

### Frame pictures must be drawn in `pictureOrder`, not file order
Each picture carries a `pictureOrder` byte (low = under, high = on top). Shadows
are low order and belong **under** the body. Drawing in array order put shadows
on top of the building. Sort a frame's pictures by `pictureOrder` before
compositing.

### Building "operational" animation speed is uniform, not per-frame
The per-frame art-delay byte is **not** the operational loop speed. OP2's
`Unit.SetAnimation(animIndex, animationDelay, …)` holds **every** frame for a
single uniform `animationDelay` (total = `numFrames * animationDelay`). The
per-frame bytes vary wildly across groups (0..8) — honouring them raced the
low-delay buildings. We advance uniformly (≤1 frame/tick), capped so no loop is
faster than ~1.25 s. (Per-frame delays still matter for *construction* ramps.)

### Shadow vs. building base image (e.g. SF: image 52 vs group 73)
A building's in-world look is its **animation group**, never the bare base image.
The base image (e.g. `52`) is just the largest *picture* composited into each
group frame, alongside the dock, the shadow pictures, and animated overlays.
Image 52 alone is missing the dock and shadow — don't use it as the "idle" frame.
Idle = frame 0 of the operational group.

### Grid alignment needs the group's pixel-displacement, not a heuristic
**Bit us:** seating buildings by "bottom-centre on the footprint" put docks/floor
pads at sub-tile positions (the smelter's dock was visibly off-grid). The .prt
**group header carries a pixel-displacement** (the field right after the selection
box — we'd been parse-skipping it). The frame's logical origin (0,0) maps to
`unitPixel − displacement`, with `unitPixel` = footprint centre. So the grid-exact
origin is **`originX = round((tileX + fpX/2)*T) − dispX`** (same for Y). Picture
offsets are baked into the composited frame, so docks/floors then land on their
designed tiles automatically. Captured in `ArtFile::Group` + `groupDisplacement()`.

### Buildings have multiple animation groups but play ONE at a time
**Bit us (twice):** first we rendered only `Normal` (the Eden DIRT showed its piston
but not its bar/dish). Then we over-corrected and composited Normal+Ambient1+Ambient2
*simultaneously* as overlays — also wrong; it's too noisy and isn't what OP2 does.
**The truth (from `Unit.DoAmbientAnimation` @0x483580):** a building plays exactly
ONE animation group at a time. Each group is a COMPLETE animation sharing the same
base (DIRT img 49 is identical in g802/g803/g1846); only the one moving picture
differs. The building loops `Normal`, and only when `(pixelY + unitIndex*8 + tick)
% 100 == 0` — ~once per 100 ticks, *staggered* per unit so none coincide — it
switches to ONE ambient (table-picked), plays it once, and reverts. Gated by the
`bShowAmbientAnimations` global. **Fix:** `PlacedBuilding::anims = [Normal, Amb1,
Amb2]`, `curAnim` selects the single one drawn. The renderer plays one animation
through, **holds the rest frame for `[Render] AnimationGap` ticks (the pause before
repeating), then rotates to the next** via a `playOrder` interleaving Normal between
the ambients (`[0,1,0,2,…]`). This restores the inter-animation pause AND guarantees
every ambient is shown — a continuous loop hid the lower lights-strip ambients (e.g.
Eden AdvancedLab g601). It's a *showcase* pacing; OP2's real trigger is the
per-position table pick (~1/100 ticks). **Lesson:** when behaviour is about
*scheduling* (which animation, when), read the function that drives it — don't infer
it from the art data alone. The picture composition told us the groups share a base;
it did NOT tell us they play together (they don't).

### Garage: clean look = construction's last frame; floor is a separate sprite
The garage writes **no Normal** — an earlier pixel-match guessed g2040/g432 but
those are the **DamagedLight** (damaged-looking) sprites. The clean operating
garage = the **Construction group's last frame** (Eden g2038, Plymouth g430). The
garage also has a **floor/dock pad** (Eden g2039, Plymouth g431 — the yellow
hazard-stripe pad) that vehicles drive onto; it's a distinct sprite drawn UNDER
the body, seated with the same displacement formula but its own group's disp.
Renderer special-cases the garage by name (last-frame static + floor underlay).

### Idle military units wander their TURRET (not a static showcase) — and decorrelate it
**The behaviour (`AnimateIdleTurret` @0x4A7DD0, gated no-target):** an idle combat unit's
turret isn't static and isn't a clean spin — each "beat" a slowly-changing mode picks
step-CW / step-CCW / drift-toward-hull / drift-toward-random / hold. Body angle = unit `+0x1C`,
turret angle = `+0x2D` (separate fields → hull holds while turret cycles). **Bit us:** keying
the beat/mode on our *sequential* parade index made whole showcase columns (which share one
hull facing) drift in unison. OP2 keys on the *scattered* unit pool index — so we hash the
index (splitmix) for an independent per-unit phase. Bombs (Supernova/Starflare) don't rotate.
Gated by `[Render] AnimationAmbientUnit`.

### Bomb weapons on Tiger = the single turret drawn TWICE (no dual sprite)
Gun weapons get a distinct pre-baked Tiger dual sprite (one draw). Supernova/Starflare have
**no** dual sprite, so OP2 draws the single bomb sprite twice, stacked. Confirmed by dumping
`op2_art.prt`: every turret group is **1 picture/frame** (no multi-picture compositing); the
gun "dual" is a wider baked sprite, the bomb "dual" is a runtime double-draw. Our renderer:
`PlacedUnit.turretStackDy` draws the bomb turret a second time, raised ~⅓ its height.

---

## Building / unit → art mapping

### `animationIndex = prtGroupIndex + 101`
Everywhere the engine references a sprite by "animationIndex", the actual
`op2_art.prt` group index is **`animationIndex − 101`** (`0x65`). Holds for both
the building `Initialize()` stores and the unit rotation tables.

### The building→sprite map is hardcoded in the EXE, not in the sheets
`building.txt` has only stats + footprint. The art-group assignment lives in each
`UnitTypeInfo:<Name>.Initialize()` as immediate stores to the UTI struct
(`edenAnimationIndex[]` @ +0x250, `plymouthAnimationIndex[]` @ +0x278), indexed by
an 8-entry `BuildingAnimationTypeIndex` enum. Found via the rule
**`[UnitVtable − 0x10]` = that building's `Initialize()`** (`unit_vtables.tsv`
gives the name). Full method: `Review/ART_ANIMATION_MAPPING.md`.

### `−1` (state not set) is real data, not a bug
Slots 0–4 (Construction/Damage/Destruction/Icon) are generic art set for both
factions; only slot 5 (Normal) + Ambients are faction-specific. So Plymouth-only
buildings have eden Normal `−1` (and vice-versa), and GuardPost/Garage have no
body Normal at all (turret / bay-based). Don't "patch" these.

### Units rotate; chassis arrays live in the UTI (and ARE static)
Units don't animate-in-place — they **rotate** (16 directions,
`rotationIndex = ((rotation+8)>>4)&0xF`; from `Unit:Vehicle.DrawUnit` @0x44ca20).
Each facing's sprite comes from a 16-entry array. An earlier note said "vehicle
`Initialize()` writes nothing" — **wrong**: the chassis array is in the vehicle UTI
at **`+0x24C` (Eden) / `+0x34C` (Plymouth)** (`GetChasisAnimationIndex` @0x44c600
reads `UTI[base + (animType*16 + rot)*4]`), written by `Initialize()` as immediates.
Combat units add a **weapon turret** (one of the 32 `.data` rotation tables,
`0x4E3CC0`–`0x4EADD0`). **Turret seating — CORRECTED (the "offset 0" guess was wrong):**
the turret is drawn at `unitPixel + chassisFrame.turretOffset − turretGroupDisp`, where
`turretOffset` is the **chassis frame's optional block** (a signed x,y per rotation, the
`.prt` per-frame 2-byte block flagged by the picture-count `0x80` bit). Offsets matter
(Lynx ≈ (0,−6), Tiger ≈ (−1,−10)); "offset 0" sat turrets ~7–10px too low.
**Every chassis mounts every weapon:** Lynx & Panther SHARE one turret sprite per weapon;
Tiger has its OWN. For 9 "gun" weapons the Tiger sprite is a distinct pre-baked two-barrel
image (drawn once). **Supernova & Starflare are bomb weapons with NO Tiger sprite** — OP2
draws the single bomb sprite TWICE, stacked (our `turretStackDy`); this is the only true
double-draw. Verified by dumping `op2_art.prt` (1 picture/frame everywhere) + in-game.
Full decode: `Review/decompiled/Unit_Rotation_Animation_Architecture.md`.

### GuardPost = ONE combined sprite per direction (NOT base + turret)
A GuardPost looks like "a Lynx" (fixed base + rotating weapon), but in the art each weapon's
GuardPost group is the **complete** GP for that direction — the blue disc base + weapon **baked
into one sprite** (proof: group 966 Laser = op2_art frame 14125; group 1084 EMP = frame 14722).
There is **no separate base group** at runtime. **Bit us twice:** (1) we drew the *construction*
group underneath as a "base" — construction art only plays *while building*, so it leaked out
beneath the tiny bomb sprites; the fix is to draw nothing but the combined sprite. (2) Bombs need
their own combined disc+bomb GP group (Starflare E g1694 / P g938; Supernova g939 shared) — NOT
the tiny vehicle bomb sprites. GuardPost groups are the weapon's *Init/default* rotation table
(DISTINCT from vehicle turrets), read from the EXE `.data` (Laser 966, RailGun 982, AcidCloud
1068, Thor's **947–962 irregular order**, ESG 1254, RPG 624, Microwave 1011, Stickyfoam 1031, EMP
E1084/P2043). **Seating must be identical at placement and on idle-rotation re-seat** — GP has no
chassis, so the vehicle chassis-frame mount offset is spurious; adding it only on the rotation
re-seat (not at placement) made rotating GPs jitter off-grid while the non-rotating bombs stayed
put (the tell). Fix: skip the mount offset when there's no chassis image; seat purely on the
group displacement (`unitPixel − groupDisp`). Full decode: `Review/decompiled/GuardPost_Rendering.md`.

### Scan BOTH `mov` immediate forms, or you miss half the writes
`Initialize()` writes anim indices two ways: direct `C7 /0` (`mov [reg+disp],imm32`)
**and** the pair `B8 imm; 89 /r` (`mov eax,imm; mov [reg+disp],eax`, where one `eax`
often feeds several slots). A scan for only `C7` finds ConVec but misses Tiger. Walk
the Init's fill region handling both forms (tracking `eax`). Buildings write states
from `+0x250`; **vehicles fill slot 0 (`+0x24C`)** — that's how to tell a vehicle Init
from a building Init in the same scan.

### Use the per-rotation array, NOT a `base+offset` formula
Most chassis are 16 consecutive groups (`base, base+15, … base+1`), but **8-directional
bodies double each sprite** (`base,base,base+7,base+7,…` — Panther, Tiger, robots) and
**some arrays are non-contiguous** (e.g. Earthworker alternates between two group
regions yet renders as a smooth 16-direction rotation). A formula renders these wrong;
store the explicit 16-entry array per unit/faction.

### Nearest-function attribution is unreliable — confirm names by RENDERING
Mapping a `+0x24C` write to "the nearest named function" repeatedly mislabelled units:
the nearby `Get*PropTag`/`GetUnitTypeInfo` accessor often belongs to a *neighbour*.
Wrong guesses we shipped and the user corrected by eye: RoboSurveyor↦Robo-Miner,
GeoCon/ArmBuilder↦Earthworker, Dozer↦Robo-Surveyor, Wreckage↦CargoTruck,
SmallAirTransport↦Scout, plus the 303/263 ↦ Evac. **The group-array *data* is solid;
the *names* must be confirmed against the game.** (Reference renders exist for
buildings but NOT units, so it's eyeball-in-app.)

### The fix for the attribution problem: the internal code-name accessor (1:1 pin)
After the nearest-function mislabels above, the reliable way to pin a vehicle's class is
its **internal code-name accessor** — a tiny vtable slot `mov eax, <codeNameVA>; ret` that
returns a unique string (`BULL_DOZER`, `GEO_METRO`, `REPAIR_TRUCK`, …). That string appears
in exactly ONE vtable, and the `Initialize()` slot is at a fixed offset from it
(codename slot **+0xC**). This 1:1 anchor is immune to the nearest-function trap and closed
out the last unmapped units in one pass each: **Robo-Dozer** (e0/p343, 8-dir), **GeoCon**
(`GEO_METRO`, real 16-dir 1154–1169 — was mis-extracted as 8-dir), **RepairVehicle**
(`REPAIR_TRUCK`, 16-dir 1130–1145). When a `+0x24C` Init is ambiguous, find the class's
code-name accessor, not the nearest label.

### Some turret art is faction-specific — don't flatten everything to "both"
We tagged all weapon turrets faction-"both" (one shared sprite). Wrong for some: weapons are
mostly **faction-EXCLUSIVE** (Eden: Laser/RailGun/AcidCloud/Thor's; Plymouth: ESG/RPG/
Microwave/Stickyfoam) — each has ONE art (its faction's), so its selector has no faction
branch (looks "shared" but isn't). The genuinely **both-faction** weapons DO split:
**Starflare** has distinct art (Eden g1623 / Plymouth g1341), **EMP GuardPost** too (E 1084 /
P 2043); **Supernova** shares (g1342). Faction is chosen in the selector by the per-player
flag `*(int*)(0x56EF48 + player*0xC24)` (**1=Eden, 0=Plymouth**; `GoEden`/`GoPlymouth` set it).
Bit us: Eden chassis drew the yellow Plymouth Starflare. Fix: split those turrets eden/plymouth
and pick by chassis faction (`findTurret`), falling back to the shared sprite otherwise.

### Cargo trucks = 20 composited loaded-truck sets (the "packed" tables decoded)
The `0x4DE600`+ "packed" cargo tables aren't structs — each dword is **two `uint16`
sprite indices**, so a flat `uint16` read gives contiguous 16-rotation arrays of
**fully-composited truck+cargo** sprites. 10 cargo types × Eden/Plymouth = 20 sets,
E/P-paired in memory from `0x4DE5E0`. Full base→cargo table in the decompile note.

### PE file-offset rule for disassembly: `fileOffset = VA − 0x400C00`
(image base 0x400000, `.text` vaddr 0x1000 / rawptr 0x400). The `.udd` label file's
first column is the RVA, not the file offset — reading there lands in the wrong
function. Rotation/UTI tables live in `.data` (vaddr 0xde000 / rawptr 0xdc800), and
`.data`'s virtual size exceeds its raw size (BSS) — bound scans by `SizeOfRawData`.

---

## `.opm` missions / OP2MissionEditor

Authoring `.opm` test missions for opu_12, each failure mode bit us once (the editor
renders OP2 art, so it rejects anything OP2 never drew — and the error fires at a
*different stage* depending on cause):

- **Unit `BarVariant`/`BarYield` must be `null`** (those are mine-only fields). `"Random"`
  on a unit → enum-parse failure on load. Beacons CAN use `"Random"`.
- **No free-standing mine/vent structures** (`CommonOreMine`, `RareOreMine`, `MagmaWell`,
  `GeothermalPlant`) — they need an underlying beacon/vent tile or **"Calculating Cell
  Types" throws `IndexOutOfRangeException`**.
- **Keep placements off the map edges** — an edge-adjacent structure footprint indexes
  negative cells → crash.
- **Weapon×chassis must have OP2 art** or **"Creating units" throws "The Object you want to
  instantiate is null"**. Bombs (Starflare/Supernova) only have Tiger/Panther art; the
  light/robot/guardpost chassis don't → null. (This is the same "no art for impossible
  combos" truth as the turret faction split.)
- **`Direction` is 8-valued, camelCase** (`East, SouthEast, …, NorthEast`) — NOT our 16
  render facings; reads case-insensitively but emit camelCase.
- Two writer styles both load (DataContract: tabs/alphabetical; Newtonsoft: 2-space/decl-
  order) — readers key by NAME, so order/whitespace don't matter; **line breaks were a red
  herring** (the error was always post-parse). Write UTF-8 no BOM.
- Our loader only resolves `LevelDetails.MapName` → the `.map` (terrain). Full schema +
  the weapon×chassis validity matrix live in `D:\opu\op2opmtools\OPM_FORMAT.md`.
- **`.opm` Position is 1-based "game coordinates"; 0-based map cell = `pos − (1,1)`.** The
  editor's `UnitView.GetMapCoordinates` subtracts (1,1) (then inverts Y for Unity's bottom-up
  tilemap — top-down renderers skip the invert). Using raw Position put everything +1,+1 off.
- **A building's `Position` is the footprint CENTRE, not the top-left.** Per the editor
  (`StructureView.GetStructureArea`, src `D:\GITHUB\OP2MissionEditor`): top-left tile =
  `(pos−1) − size/2` (combine with the −1 above). Rendering pos as top-left also left a ~1-tile
  gap between adjacent buildings (Agridome|StructureFactory). Vehicles are 1×1 so cell = pos−1.
  (The big `LOCATION(X+31,Y-1)` is OP2OpmTools' engine-export offset, NOT for rendering.)
- **Self-contained units take no separate turret.** Spider/Scorpion carry a weapon in the
  `.opm` but have no turret graphic — drawing one covers the small body ("no chassis"). Only
  Lynx/Panther/Tiger mount a separate turret sprite.
- **Mission vs parade render must not cross-contaminate.** Sidebar shadow/ambient toggles
  re-composite via `placeAllUnits/Buildings` (the parade); in mission mode they must rebuild
  via `placeFromWorld()` instead, or toggling a shadow swaps the mission for the parade.

---

## Game timing

### Displayed "Mark" = `tick / 100`
The HUD "Current Mark" is `floor(tick / 100)` (confirmed via memory reader). This
is **distinct** from the engine's internal `Time() = tick >> 2` (4 ticks/mark,
used for triggers). Don't conflate them.

---

## Build & toolchain

### `qmltyperegistrar` needs the type's dir on the include path
A `QQuickPaintedItem` subclass registered with `QML_ELEMENT` generates
`#include <basename.h>` by basename. Add its directory to
`target_include_directories` (e.g. `render/`), or registration fails with
"QQuickItem was not declared". `DEPENDENCIES QtQuick` alone does **not** fix it.

### Cross-toolchain DLLs need a C ABI
MSVC and MinGW C++ ABIs are incompatible. To share a C++ library across them
(OP2Utility), wrap it in an **`extern "C"`** API with opaque handles + error
codes (→ OP2UtilityC). For a self-contained MinGW exe/DLL:
`-static-libgcc -static-libstdc++ -static`.

### Two build outputs — don't get fooled by a stale one
There's a **Debug** build (`Outpost2/build/Desktop_…-Debug/`, ~12 MB) and the
shareable **Release** package (`D:\opu\build\op2remake\`, ~0.8 MB exe + Qt DLLs).
When a fix "does nothing", confirm you're running the freshly-built exe (check the
timestamp). Several "no change" reports were stale binaries.

---

## Debugging process (meta)

1. **Read the reference implementation before guessing** a binary/render format.
   The shadow loop would have been one step if we'd opened `CPrtFile.vb` first.
2. **Verify renders visually** — dump to PNG and inspect (not just "it compiles").
   A 5× zoom can also *exaggerate* edge artifacts that vanish at 1× — judge at the
   real scale too.
3. **Don't smooth/morph data you don't understand.** Square/blocky shadows came
   from morphology applied to a mask that was simply mis-strided. Fix the decode,
   not the symptom.
4. **The file logger** (`logs/op2remake-*.txt`) + the two-structure animation
   trace are the fastest way to confirm pacing/behaviour without a debugger.
