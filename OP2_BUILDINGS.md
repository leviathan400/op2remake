# Outpost 2 — buildings

All 42 buildings, with their **`.opm` name** (the `TypeID` used in OP2MissionEditor `.opm` files) and
their **`building.txt` Code_Name** (the OP2 stat sheet under `OPU/sheets/building.txt`). Footprint and
hit points are from the sheet. (The readable name is the in-game display name; `building.txt` itself
only stores codes, not display names.)

## Command & coordination
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Command Center | `CommandCenter` | `COMMAND` | 3×2 | 2500 |
| Robot Command | `RobotCommand` | `ROBOT_COMMAND` | 2×2 | 2000 |
| Trade Center | `TradeCenter` | `TRADE` | 2×2 | 800 |

## Power
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Tokamak | `Tokamak` | `TOKAMAK` | 2×2 | 1500 |
| MHD Generator | `MHDGenerator` | `ASE` | 2×2 | 1250 |
| Solar Power Array | `SolarPowerArray` | `SOLAR` | 3×2 | 875 |
| Geothermal Plant | `GeothermalPlant` | `GEOTHERMAL` | 2×1 | 1000 |

## Mining, refining & storage
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Common Ore Mine | `CommonOreMine` | `MINE` | 2×1 | 1000 |
| Rare Ore Mine | `RareOreMine` | `MINE_ADV` | 2×1 | 1000 |
| Magma Well | `MagmaWell` | `MAGMA_WELL` | 2×1 | 1000 |
| Common Ore Smelter | `CommonOreSmelter` | `SMELTER` | 4×3 | 2000 |
| Rare Ore Smelter | `RareOreSmelter` | `SMELTER_ADV` | 4×3 | 2000 |
| Common Storage | `CommonStorage` | `STORAGE_ORE` | 1×2 | 500 |
| Rare Storage | `RareStorage` | `STORAGE_RARE` | 1×2 | 500 |

## Factories & production
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Structure Factory | `StructureFactory` | `FACT_STRUCTURE` | 4×3 | 2000 |
| Vehicle Factory | `VehicleFactory` | `FACT_VEHICLE` | 4×3 | 3000 |
| Arachnid Factory | `ArachnidFactory` | `FACT_ANDROID` | 2×2 | 1750 |
| Consumer Factory | `ConsumerFactory` | `FACT_LUXURY` | 3×3 | 1250 |
| Garage | `Garage` | `GARAGE` | 3×2 | 1750 |
| Spaceport | `Spaceport` | `SPACEPORT` | 5×4 | 2000 |
| GORF | `GORF` | `GORF` | 3×2 | 1500 |

## Labs & research
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Basic Lab | `BasicLab` | `LAB` | 2×2 | 1000 |
| Standard Lab | `StandardLab` | `LAB_STANDARD` | 3×2 | 1250 |
| Advanced Lab | `AdvancedLab` | `LAB_ADV` | 3×3 | 1500 |
| Observatory | `Observatory` | `OBSERVATORY` | 2×2 | 875 |

## Habitation & population
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Residence | `Residence` | `RESIDENCE` | 2×2 | 800 |
| Reinforced Residence *(Plymouth)* | `ReinforcedResidence` | `RESIDENCE_P` | 3×2 | 1500 |
| Advanced Residence *(Eden)* | `AdvancedResidence` | `RESIDENCE_E` | 3×3 | 1250 |
| Agridome | `Agridome` | `AGRIDOME` | 3×2 | 1000 |
| Nursery | `Nursery` | `NURSERY` | 2×2 | 1125 |
| University | `University` | `UNIVERSITY` | 2×2 | 1125 |
| Medical Center | `MedicalCenter` | `MED_CENTER` | 2×2 | 1000 |

## Morale & services
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Forum | `Forum` | `FORUM` | 2×2 | 1000 |
| Recreation Facility | `RecFacility` | `RECREATION` | 2×2 | 875 |
| DIRT | `DIRT` | `DIRT` | 3×2 | 2000 |

## Defense & utility
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Guard Post | `GuardPost` | `TOWER_GUARD` | 1×1 | 500 |
| Meteor Defense | `MeteorDefense` | `METEOR_DEF` | 2×2 | 1250 |
| Light Tower | `LightTower` | `TOWER_LIGHT` | 1×1 | 500 |

## Walls & tubes
| Building | `.opm` name | building.txt Code_Name | Footprint | HP |
|----------|-------------|------------------------|:---------:|---:|
| Tube | `Tube` | `TUBE` | 1×1 | 2500 |
| Wall | `Wall` | `WALL` | 1×1 | 300 |
| Lava Wall | `LavaWall` | `WALL_LAVA` | 1×1 | 300 |
| Microbe Wall | `MicrobeWall` | `WALL_MICROBE` | 1×1 | 300 |

> **TODO — wall / tube / dozed-terrain rendering (autotiling).** These aren't drawn as a single
> fixed sprite. The tile graphic is chosen at draw time from:
> 1. **The terrain underneath** — walls/tubes sit on *bulldozed* ground, which has its own tile set
>    (RoboDozer / Earthworker clear the terrain first; the dozed tile depends on the original terrain
>    type).
> 2. **Adjacent objects (connected texture)** — a wall/tube picks an *attached* variant based on which
>    of its N/E/S/W neighbours are the same type (straight / corner / T / cross / end-cap). Placing or
>    removing one must **re-evaluate and update the neighbours' graphics too**, so connections form and
>    break. This is a neighbour-bitmask → tile-variant lookup (like classic RTS wall autotiling).
> Not implemented yet — these are placed/stored but their connected rendering + dozed-terrain layer
> are a dedicated task.

---

**Notes**
- `ASE` is the Code_Name for the **MHD Generator** (a quirk of the sheet).
- `RESIDENCE_E` / `RESIDENCE_P` are the **Eden** / **Plymouth** advanced residences (`AdvancedResidence`
  / `ReinforcedResidence`); the basic `Residence` is shared.
- The `building.txt` "Building Name" / "Produce List Name" columns are single-letter codes, not display
  names — the human-readable names live in the EXE (like the space objects).
- Mapping source in the remake: `kBuildingCodeName` in `Outpost2/render/tilesetview_mission.cpp`
  (`.opm` name → Code_Name); HP/footprint loaded into the `World` `UnitType` table.
