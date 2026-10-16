#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// TechTree  —  loads an OPU technology-tree text file (base/techs/*.txt)
// =============================================================================
//
// Format (line-based, ';' starts a comment):
//
//   BEGIN_TECH "Cybernetic Teleoperation" 03408
//       CATEGORY        4
//       DESCRIPTION     "..."
//       TEASER          "..."
//       IMPROVE_DESC    "..."
//       COST            300
//       EDEN_COST       300        ; optional faction override
//       PLYMOUTH_COST   350        ; optional faction override
//       MAX_SCIENTISTS  5
//       LAB             2
//       REQUIRES        03408      ; zero or more
//       UNIT_PROP TOKAMAK Production_Capacity 300   ; zero or more
//   END_TECH
//
// Pure std C++ (no Qt). Lives in op2sim because research/tech state is sim data.
// Full format reference (tags, categories, UNIT_PROP properties, limits,
// TECHID/1000 = tech level): docs/TechTreeFormat.md.

namespace op2::sim {

// A UNIT_PROP override applied when the tech completes.
struct TechUnitProp {
    std::string unit;      // e.g. "TOKAMAK"
    std::string property;  // e.g. "Production_Capacity"
    int value = 0;
};

struct Tech {
    int id = 0;            // 5-digit tech id (leading zeros dropped: 03408 -> 3408)
    std::string name;
    int category = 0;
    int cost = 0;          // generic COST (may be negative, e.g. -1 = unavailable)
    int edenCost = -1;     // EDEN_COST override; -1 if absent (fall back to cost)
    int plymouthCost = -1; // PLYMOUTH_COST override; -1 if absent
    int maxScientists = 0;
    int lab = 0;           // 0 = none
    std::string description;
    std::string teaser;
    std::string improveDesc;
    std::vector<int> prereqs;          // REQUIRES ids
    std::vector<TechUnitProp> unitProps;
};

class TechTree {
public:
    // Read + parse a techs .txt file. Returns false (and *error, if given) on
    // failure to open. Parse errors on individual lines are skipped leniently.
    bool load(const std::string& path, std::string* error = nullptr);

    // Parse from an in-memory string (filesystem-independent; used by tests).
    bool loadFromString(const std::string& content);

    const std::vector<Tech>& techs() const { return techs_; }
    std::size_t size() const { return techs_.size(); }
    const Tech* byId(int id) const;     // nullptr if not present

private:
    std::vector<Tech> techs_;
    std::unordered_map<int, std::size_t> idIndex_;
};

} // namespace op2::sim
