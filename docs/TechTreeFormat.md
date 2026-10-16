# Tech-tree file format (reference)

Reference for the OP2 tech-tree `.txt` files and our `TechTree` loader
(`Outpost2/sim/include/op2/sim/techtree.h`, `src/techtree.cpp`). Captured from
user-supplied format notes + verified against the real OPU files.

## The four trees (`OPU/base/techs/`)
| file | mode | techs |
|---|---|---|
| `edentek.txt` | Eden single-player campaign | 135 |
| `ply_tek.txt` | Plymouth single-player campaign | 137 |
| `multitek.txt` | Multiplayer (both factions) | 137 |
| `tutortek.txt` | Tutorial | 95 |

## Grammar
```
BEGIN_TECH "Technology Name" <TECHID>
    CATEGORY        <0-12>
    DESCRIPTION     "..."          ; full research text (≤800 chars)
    TEASER          "..."          ; pre-research text (≤800)
    IMPROVE_DESC    "..."          ; improvement text (≤800)
    REQUIRES        <techId>       ; 0..32 prerequisites
    COST            <n>            ; both factions; OR use the pair below
    EDEN_COST       <n>            ; faction override
    PLYMOUTH_COST   <n>
    MAX_SCIENTISTS  <4-18 | 1>     ; 1 for special/free techs
    LAB             <1|2|3>        ; 1=Basic 2=Standard 3=Advanced
    UNIT_PROP <UNIT> <Property> <int>   ; 0..32 (shares the 32 cap with
                                        ; PLAYER_PROP / FUNCTION_RESULT)
END_TECH
```
`;` starts a comment. Quoted strings have embedded newlines/tabs stripped. Numbers
may have a leading `-` only. Single-value tags overwrite if repeated (except
`REQUIRES`/`UNIT_PROP`/`PLAYER_PROP`/`FUNCTION_RESULT`, which accumulate).

## Cost = research time, not resources
| value | meaning | visibility |
|---|---|---|
| `0` | pre-researched | invisible in lab menu |
| `-1` | disabled (faction restriction) | unavailable |
| `>0` | available; higher = longer | shows in lab / completed |

Asymmetry example: Eden gets Laser free but can't research Microwave (`-1`);
Plymouth is the mirror. Use **either** `COST` alone **or** both `EDEN_COST` +
`PLYMOUTH_COST`.

## TECHID encodes the campaign tech-level
**Tech Level = TECHID / 1000** (must be ≠ 0). Single-player missions unlock tech
levels (01000, 02000, …) to gate progression. So `05317` is tech-level 5.

## CATEGORY → icon
0 none/free · 1 Lab/DB-recovery · 2 Factory/infra · 3 Power · 4 Vehicles ·
5 Food · 6 Mining · 7 Weapons · 8 Spaceport · 9 Morale/population ·
10 Geological/disasters · 11 Medical/emergency · 12 Starship.

## UNIT_PROP properties
`Hit_Points, Armor (0-5), Production_Capacity, Move_Speed, Sight_Range,
Rate_Of_Fire, Power_Required, Storage_Capacity, Production_Rate,
Concussion_Damage, Penetration_Damage, Improved, Workers_Required,
Common_Required, Rare_Required` (+ Build_Time/Storage_Bays/Turn_Rate/Build_Points,
unused in standard files). The `<UNIT>` token uses sheet code-names
(`TOKAMAK, FACT_STRUCTURE, CON_TRUCK, …`).

## Limits / validation (for an editor)
- ≤256 techs/file · name ≤100 · description ≤800 · number buffer ≤32 bytes (int32).
- ≤32 `REQUIRES`, ≤32 upgrade tags per tech.
- `Armor` 0-5 · `MAX_SCIENTISTS` 4-18 (or 1) · `LAB` 1-3 · `COST` -1/0/positive.

## Our loader status
`TechTree` captures **everything used in the real files**: id, name, category,
cost/edenCost/plymouthCost, maxScientists, lab, description/teaser/improveDesc,
`REQUIRES` → `prereqs`, `UNIT_PROP` → `unitProps`. Unknown keywords are skipped
leniently. `PLAYER_PROP`/`FUNCTION_RESULT` are not parsed (mod-only; absent from
all 4 OPU files) — add them only if a mod needs them (their token syntax isn't in
the standard files). No validation is enforced on load (a tech *editor* would add
the table above).
