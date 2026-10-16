# Selection detail panel (the "7th view")

When the player selects an object, the lower sidebar shows its **detail panel** — OP2's
selection view, conceptually a 7th panel alongside the 6 command-bar reports
(`docs/COMMAND_PANELS.md`). Goal: reproduce OP2's panel faithfully.

## Interaction

- **Left-click** a building / GuardPost / vehicle in the game view → selects it (topmost
  under the cursor). A green **selection box** outlines its footprint (buildings) or 1×1 cell
  (units). Selecting overrides any open command report. *(Later: left-click on an
  already-selected unit will issue a move/command.)*
- **Right-click** → deselect. Clicking empty ground also deselects.
- The panel shows when something is selected **and** no command report is open
  (`tilesetView.selectedValid && root.selectedCmd === -1`).

## Plumbing

- The panel QML lives in `SelectionPanel.qml` (takes the `TilesetView` as `view`; shown by
  `Main.qml` when something is selected and no command report is open).
- `TilesetView::selectAt(x,y)` / `clearSelection()` / `setSelectedIdle(on)` (Q_INVOKABLE),
  plus the `selectedInfo` (QVariantMap) and `selectedValid` properties the QML binds to.
  `setSelectedIdle` does NOT write the World directly — it issues a real
  **ctMoIdle/ctMoUnidle CommandPacket** through the sim command layer (`op2/sim/command.h`),
  the same path a network peer's or replayed command takes.
- Selection is tracked by the **stable World handle** (`UnitId`), so it survives a
  `placeFromWorld` rebuild (anim toggle, mine survey, status change) — `rebuildSelectedInfo()`
  re-finds the object and refreshes the map, or marks it invalid if it's gone.
- `selectedInfo` fields: `name, owner, idx, gen, cellX, cellY, hp, maxHp, armor, armorName,
  power, workers, scientists, weapon, idle, disabled, status, kind, isBuilding, isGuardPost,
  iconName, friendly`.

## Panel contents

- **Portrait box** — OP2 art group 54 (60×60 frame) with the object's **category icon**
  inside, drawn at native **45×45** (no scaling).
- **Title** — `Name #player-index` in OP2 green `#00ff00`.
- **Armor class** (muted colour, as in OP2) + **HP bar** (`#00ff00` fill, cur/max overlaid).
- **Stat grid** (all `#00ff00`): **Status** (Active green / Idle gray / Disabled red — state
  coloured), **Weapon** (armed units), and the building **Power / Workers / Scientists**
  requirements from `building.txt` (`0` → "None"). Live usage + storage are pending the economy sim.
- **IDLE / ACTIVATE** button (art group 50) — toggles the structure's player-Idle flag (as a
  ctMoIdle/ctMoUnidle command via the sim command layer).
- **Debug section** (intentionally dimmer, not part of OP2): owning **Player** (friendly/enemy),
  **Kind**, the World **ID** (`index (gen N)`), and **Location** (1-based tile coords).

## Category icons

Each object's portrait icon is chosen by a hardcoded **category** (OP2 doesn't store this in
`building.txt`) in `techIconStem()` (`render/tilesetview.cpp`). Icons live in `ui/tech/Tech_*.png`
(45×45, opaque, drawn as-is — no colour-key). Mapping:

- GuardPost → `Tech_Weapon`; **all vehicles → `Tech_Weapon`** (temporary — per-vehicle categories TODO).
- Command Center / Trade Center / Light Tower → `Tech_Eden` or `Tech_Plymouth` by faction.
- Labs → `Tech_Science`; Agridome → `Tech_Food`; power buildings → `Tech_Power`;
  mines/smelters/storage/MagmaWell → `Tech_Metal`.
- Residences/Nursery/Forum/RecFacility/**University**/**ConsumerFactory** → `Tech_Colonists`.
- VehicleFactory/Garage/GORF/RobotCommand → `Tech_Vehicle`; **ArachnidFactory → `Tech_Arachnid`**
  (icon file pending).
- **MedicalCenter/DIRT/MeteorDefense → `Tech_Emergency`**; Spaceport → `Tech_Starship`;
  Observatory → `Tech_Space`. Unknown building → `Tech_Strcture` (the filename's original spelling).

## TODO

- Armor-class **number → name** map (panel prints the raw class number until confirmed).
- Per-vehicle categories (combat vs utility; GeoCon → Power?).
- Live Power/Workers/Scientists usage + storage (economy sim); Production fields.
- Vehicle command-icon grid; factory BUILD/STORAGE actions; drive Idle/Disabled from the sim.
