#include "op2/sim/morale.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace op2::sim {

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> splitTabs(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (ch == '\t') {
            out.push_back(cur);
            cur.clear();
        } else if (ch != '\r') {
            cur += ch;
        }
    }
    out.push_back(cur);
    return out;
}

int parseInt(const std::string& s) {
    try {
        return std::stoi(trim(s), nullptr, 10);
    } catch (...) {
        return 0;
    }
}

int intAt(const std::vector<std::string>& cells, std::size_t idx) {
    return (idx < cells.size()) ? parseInt(cells[idx]) : 0;
}

} // namespace

bool MoraleData::parse(const std::string& content) {
    levels_.clear();
    bonuses_.clear();

    std::vector<std::string> lines;
    {
        std::istringstream in(content);
        std::string l;
        while (std::getline(in, l)) lines.push_back(l);
    }

    // First content line is the levels header (skip it; field order is fixed).
    std::size_t i = 0;
    for (; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (!t.empty() && t[0] != ';') break;
    }
    if (i >= lines.size()) return false;
    ++i; // skip the levels header

    // Levels section: rows until the first blank line.
    for (; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (t.empty()) { ++i; break; }
        if (t[0] == ';') continue;
        const std::vector<std::string> c = splitTabs(lines[i]);
        MoraleLevel lv;
        lv.name = trim(c.empty() ? std::string() : c[0]);
        if (lv.name.empty()) continue;
        lv.ptsTotal   = intAt(c, 1);
        lv.powBonus   = intAt(c, 2);
        lv.rschBonus  = intAt(c, 3);
        lv.factBonus  = intAt(c, 4);
        lv.foodBonus  = intAt(c, 5);
        lv.defectRate = intAt(c, 6);
        lv.fertRate   = intAt(c, 7);
        lv.dieRate    = intAt(c, 8);
        levels_.push_back(std::move(lv));
    }

    // Remaining sections: flat NAME -> value (value = first non-empty cell after
    // the name). Blank lines / ';' comments separate groups; we lump them all.
    for (; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (t.empty() || t[0] == ';') continue;
        const std::vector<std::string> c = splitTabs(lines[i]);
        const std::string name = trim(c.empty() ? std::string() : c[0]);
        if (name.empty()) continue;
        int value = 0;
        for (std::size_t k = 1; k < c.size(); ++k) {
            const std::string v = trim(c[k]);
            if (!v.empty()) { value = parseInt(v); break; }
        }
        bonuses_[name] = value;
    }

    return !levels_.empty();
}

bool MoraleData::load(const std::string& path, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "morale: cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str());
}

const MoraleLevel* MoraleData::levelByName(const std::string& name) const {
    for (const MoraleLevel& l : levels_) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

int MoraleData::bonus(const std::string& name, int def) const {
    const auto it = bonuses_.find(name);
    return (it == bonuses_.end()) ? def : it->second;
}

} // namespace op2::sim
