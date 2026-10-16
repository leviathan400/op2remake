#pragma once
#include <QString>

#include <vector>

// =============================================================================
// BuildingCatalog  —  building type → op2_art.prt sprite groups
// =============================================================================
//
// Parses building_anim_catalog.tsv, the authoritative building→art mapping
// extracted from Outpost2.exe. Each building has, per faction, the .prt group
// for all 8 BuildingAnimationTypeIndex states. (-1 = state not set; see the
// writeup in D:\Oupost2Decompile\Review\ART_ANIMATION_MAPPING.md.)

struct BuildingDef {
    int mapId = 0;
    QString name;
    int fpX = 1; // footprint width  (tiles)
    int fpY = 1; // footprint height (tiles)
    // BuildingAnimationTypeIndex: 0=Construction 1=DamagedLight 2=DamagedHeavy
    // 3=Destruction 4=Icon 5=Normal 6=Ambient1 7=Ambient2.
    int eden[8];
    int plym[8];

    BuildingDef() {
        for (int i = 0; i < 8; ++i) {
            eden[i] = -1;
            plym[i] = -1;
        }
    }

    // Group to show a standing building: Normal preferred, then the other
    // faction's Normal (faction-exclusive buildings), then Construction as a
    // last resort (covers GuardPost/Garage, which have no Normal body group).
    int displayGroup() const {
        if (eden[5] >= 0) return eden[5];
        if (plym[5] >= 0) return plym[5];
        if (eden[0] >= 0) return eden[0];
        return plym[0];
    }
};

class BuildingCatalog {
public:
    // Returns false if the file is missing or empty.
    bool load(const QString& tsvPath);
    const std::vector<BuildingDef>& buildings() const { return buildings_; }

private:
    std::vector<BuildingDef> buildings_;
};
