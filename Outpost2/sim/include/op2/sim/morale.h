#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// MoraleData  —  parses the multi-section morale.txt sheet
// =============================================================================
//
// morale.txt is unlike the other sheets: it has one tabular "levels" section
// then several flat NAME->value modifier blocks (separated by blank lines):
//
//   <tab>M_PTS_TOT  M_POW_BONUS  M_RSCH_BONUS ... M_DIE_RATE   <- levels header
//   UTOPIA   90  0  15  -25 ...                                 <- 5 morale levels
//   ...
//   (blank)
//   KID_DIES        -8         <- event bonuses (instant)
//   ...
//   EVENT_DEC_RATE  90         <- decay rate
//   CROWDED_HIGH    -2         <- condition bonuses (steady-state)
//   ...
//   DIFF_EASY       100        <- difficulty multipliers
//
// Levels become a typed list; every other NAME->value entry goes into one
// `bonuses` map keyed by name. Pure std C++; lives in op2sim.

namespace op2::sim {

struct MoraleLevel {
    std::string name;    // UTOPIA, GROOVY, OKAYFINE, UPSET, WAYBAD
    int ptsTotal = 0;    // M_PTS_TOT — lower point threshold for this level
    int powBonus = 0;    // M_POW_BONUS
    int rschBonus = 0;   // M_RSCH_BONUS
    int factBonus = 0;   // M_FACT_BONUS
    int foodBonus = 0;   // M_FOOD_BONUS
    int defectRate = 0;  // M_DEFECT_RATE
    int fertRate = 0;    // M_FERT_RATE
    int dieRate = 0;     // M_DIE_RATE
};

class MoraleData {
public:
    bool load(const std::string& path, std::string* error = nullptr);
    bool parse(const std::string& content); // filesystem-independent (tests)

    const std::vector<MoraleLevel>& levels() const { return levels_; }
    const MoraleLevel* levelByName(const std::string& name) const;

    // Event / condition / difficulty modifiers, keyed by name
    // (e.g. "KID_DIES", "CROWDED_HIGH", "DIFF_EASY", "EVENT_DEC_RATE").
    int bonus(const std::string& name, int def = 0) const;
    bool hasBonus(const std::string& name) const { return bonuses_.count(name) > 0; }
    std::size_t bonusCount() const { return bonuses_.size(); }

private:
    std::vector<MoraleLevel> levels_;
    std::unordered_map<std::string, int> bonuses_;
};

} // namespace op2::sim
