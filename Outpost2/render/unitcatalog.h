#pragma once
#include <QString>

#include <vector>

// =============================================================================
// UnitCatalog  —  vehicle/turret type → op2_art.prt rotation sprite groups
// =============================================================================
//
// Parses unit_anim_catalog.tsv, extracted from Outpost2.exe. Unlike buildings
// (which animate in place), units ROTATE: each row holds the 16 directional
// sprite groups (rotation 0..15) for one chassis body, weapon turret, or flying
// body, per faction. Full method + findings:
// D:\Oupost2Decompile\Review\decompiled\Unit_Rotation_Animation_Architecture.md
//
// Key facts the data encodes:
//   * rotationIndex = ((rotation + 8) >> 4) & 0xF   (16 directions)
//   * group(r) is taken straight from groups[r] — NOT a base+offset formula:
//     8-directional bodies double each sprite, and some arrays are non-contiguous.
//   * Combat units = a `chassis` body + a `turret` drawn at the SAME origin
//     (each sprite's own frame anchor seats it; no extra mount offset).

struct UnitDef {
    QString kind;     // "chassis", "turret", or "flying"
    QString name;
    QString faction;  // "eden", "plymouth", or "both"
    int dirs = 16;    // 8 or 16 (informational; groups[] is always 16 entries)
    int groups[16];   // .prt group per rotation 0..15 (-1 = none)

    UnitDef() { for (int i = 0; i < 16; ++i) groups[i] = -1; }
};

class UnitCatalog {
public:
    // Returns false if the file is missing or empty.
    bool load(const QString& tsvPath);
    const std::vector<UnitDef>& units() const { return units_; }

    // First turret whose name starts with `chassis` (e.g. "Laser_Lynx" for
    // chassis "Lynx"), for composing a combat unit. Null if none.
    const UnitDef* turretForChassis(const QString& chassisName) const;

private:
    std::vector<UnitDef> units_;
};
