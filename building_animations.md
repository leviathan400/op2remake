# Building animations overview

Generated from `building_anim_catalog.tsv` + `op2_art.prt`. Each OP2 building has a `Normal` animation group plus optional `Ambient1`/`Ambient2` groups. These are **complete animations that share the same base**, and the building plays **exactly one at a time** — it loops Normal, then periodically switches to a single Ambient and reverts (it does NOT run them together). A building can have **up to 3 animation groups per faction**.

`group(frames)` — a `.prt` animation group and its frame count. `Normal` = the operating building (its own moving part); `Amb1/Amb2` = extra ambient animations the building switches to one at a time. `-` = none.

| id | building | fp | faction | Normal | Amb1 | Amb2 | layers |
|----|----------|----|---------|--------|------|------|--------|
| 0x15 | CommonOreMine | 2x1 | Eden | g574(41f) | - | - | 1 |
| | | | Plymouth | g594(61f) | g1848(33f) | - | 2 |
| 0x16 | RareOreMine | 2x1 | Eden | g575(51f) | - | - | 1 |
| | | | Plymouth | g577(47f) | - | - | 1 |
| 0x17 | GuardPost | 1x1 | Eden | - | - | - | 0 |
| | | | Plymouth | - | - | - | 0 |
| 0x18 | LightTower | 1x1 | Eden | g648(41f) | g649(41f) | - | 2 |
| | | | Plymouth | g186(last) | - | - | 1 |
| 0x19 | CommonStorage | 1x2 | Eden | g2059(27f) | - | - | 1 |
| | | | Plymouth | g2061(27f) | - | - | 1 |
| 0x1A | RareStorage | 1x2 | Eden | g2060(33f) | - | - | 1 |
| | | | Plymouth | g2062(36f) | - | - | 1 |
| 0x1B | Forum | 2x2 | Eden | - | - | - | 0 |
| | | | Plymouth | g2033(30f) | - | - | 1 |
| 0x1C | CommandCenter | 3x2 | Eden | g1845(87f) | - | - | 1 |
| | | | Plymouth | g1854(56f) | - | - | 1 |
| 0x1D | MHDGenerator | 2x2 | Eden | - | - | - | 0 |
| | | | Plymouth | g940(40f) | g1863(47f) | - | 2 |
| 0x1E | Residence | 2x2 | Eden | g610(56f) | g611(63f) | g1864(62f) | 3 |
| | | | Plymouth | g612(47f) | g613(49f) | g1849(82f) | 3 |
| 0x1F | RobotCommand | 2x2 | Eden | g647(68f) | g650(41f) | g651(47f) | 3 |
| | | | Plymouth | g645(72f) | g1852(66f) | - | 2 |
| 0x20 | TradeCenter | 2x2 | Eden | g800(30f) | g1838(41f) | - | 2 |
| | | | Plymouth | g946(51f) | g1837(82f) | - | 2 |
| 0x21 | BasicLab | 2x2 | Eden | g642(41f) | g1857(30f) | - | 2 |
| | | | Plymouth | g2034(60f) | - | - | 1 |
| 0x22 | MedicalCenter | 2x2 | Eden | g602(32f) | g1743(21f) | - | 2 |
| | | | Plymouth | g620(36f) | g621(60f) | g1739(31f) | 3 |
| 0x23 | Nursery | 2x2 | Eden | g596(42f) | g597(36f) | g598(31f) | 3 |
| | | | Plymouth | g606(21f) | g608(46f) | g609(32f) | 3 |
| 0x24 | SolarPowerArray | 3x2 | Eden | g941(66f) | - | - | 1 |
| | | | Plymouth | g942(30f) | g175(121f) | g2063(121f) | 3 |
| 0x25 | RecFacility | 2x2 | Eden | g614(20f) | g615(15f) | g616(16f) | 3 |
| | | | Plymouth | g617(20f) | g618(21f) | g619(11f) | 3 |
| 0x26 | University | 2x2 | Eden | g667(91f) | g668(75f) | g1746(63f) | 3 |
| | | | Plymouth | g937(96f) | g936(68f) | - | 2 |
| 0x27 | Agridome | 3x2 | Eden | g801(35f) | - | - | 1 |
| | | | Plymouth | g646(40f) | - | - | 1 |
| 0x28 | DIRT | 3x2 | Eden | g802(30f) | g803(31f) | g1846(42f) | 3 |
| | | | Plymouth | g1862(30f) | - | - | 1 |
| 0x29 | Garage | 3x2 | Eden | g2038(last) | - | - | 1 |
| | | | Plymouth | g430(last) | - | - | 1 |
| 0x2A | MagmaWell | 2x1 | Eden | g1337(21f) | - | - | 1 |
| | | | Plymouth | - | - | - | 0 |
| 0x2B | MeteorDefense | 2x2 | Eden | g564(82f) | g565(83f) | g566(41f) | 3 |
| | | | Plymouth | - | - | - | 0 |
| 0x2C | GeothermalPlant | 2x1 | Eden | g1842(43f) | - | - | 1 |
| | | | Plymouth | - | - | - | 0 |
| 0x2D | ArachnidFactory | 2x2 | Eden | - | - | - | 0 |
| | | | Plymouth | g1847(75f) | - | - | 1 |
| 0x2E | ConsumerFactory | 3x3 | Eden | g1839(35f) | - | - | 1 |
| | | | Plymouth | - | - | - | 0 |
| 0x2F | StructureFactory | 4x3 | Eden | g73(50f) | - | - | 1 |
| | | | Plymouth | g1860(46f) | g1861(46f) | - | 2 |
| 0x30 | VehicleFactory | 4x3 | Eden | g807(43f) | g2064(28f) | - | 2 |
| | | | Plymouth | g1853(41f) | - | - | 1 |
| 0x31 | StandardLab | 3x2 | Eden | g669(29f) | g670(22f) | g1747(31f) | 3 |
| | | | Plymouth | g671(34f) | g1735(48f) | g1736(45f) | 3 |
| 0x32 | AdvancedLab | 3x3 | Eden | g600(10f) | g601(22f) | g1835(119f) | 3 |
| | | | Plymouth | g643(20f) | g644(27f) | g1836(86f) | 3 |
| 0x33 | Observatory | 2x2 | Eden | g654(61f) | g655(61f) | g656(71f) | 3 |
| | | | Plymouth | - | - | - | 0 |
| 0x34 | ReinforcedResidence | 3x2 | Eden | - | - | - | 0 |
| | | | Plymouth | g640(43f) | g641(61f) | g1777(112f) | 3 |
| 0x35 | AdvancedResidence | 3x3 | Eden | g622(53f) | g623(44f) | g1778(71f) | 3 |
| | | | Plymouth | - | - | - | 0 |
| 0x36 | CommonOreSmelter | 4x3 | Eden | g1007(58f) | g1008(61f) | g1855(35f) | 3 |
| | | | Plymouth | g1009(30f) | g1859(68f) | - | 2 |
| 0x37 | Spaceport | 5x4 | Eden | g963(30f) | g964(30f) | g1844(65f) | 3 |
| | | | Plymouth | g965(78f) | g1741(35f) | g1742(41f) | 3 |
| 0x38 | RareOreSmelter | 4x3 | Eden | g1841(89f) | - | - | 1 |
| | | | Plymouth | g1010(34f) | g1776(61f) | - | 2 |
| 0x39 | GORF | 3x2 | Eden | g805(50f) | g806(45f) | g1744(31f) | 3 |
| | | | Plymouth | g1737(41f) | g1738(48f) | - | 2 |
| 0x3A | Tokamak | 2x2 | Eden | g2035(62f) | - | - | 1 |
| | | | Plymouth | g351(154f) | g652(183f) | g653(172f) | 3 |

**26 of 38 buildings have more than one animation group for at least one faction.** Note: these play **one at a time**, not layered — the building loops Normal and periodically (≈every 100 ticks, staggered per unit; see `Unit.DoAmbientAnimation` @0x483580) switches to one Ambient, plays it once, and reverts. They are *not* composited simultaneously.
