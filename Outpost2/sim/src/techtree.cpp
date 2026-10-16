#include "op2/sim/techtree.h"

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

std::string firstToken(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(0, i);
}

int parseInt(const std::string& s) {
    try {
        return std::stoi(trim(s), nullptr, 10); // base 10: "03408" -> 3408
    } catch (...) {
        return 0;
    }
}

// Text between the first and last double-quote, or the trimmed input if unquoted.
std::string unquote(const std::string& s) {
    const std::size_t a = s.find('"');
    const std::size_t b = s.rfind('"');
    if (a != std::string::npos && b != std::string::npos && b > a) {
        return s.substr(a + 1, b - a - 1);
    }
    return trim(s);
}

} // namespace

// Parses the OP2 tech-tree grammar: a flat list of
//   BEGIN_TECH "Name" <id>
//     CATEGORY <n> / COST <n> / REQUIRES <id> / UNIT_PROP <unit> <prop> <val> / ...
//   END_TECH
// blocks. ';' lines and blank lines are comments; unknown keywords are ignored
// (forward-compatible with sheet additions). One pass, line by line.
bool TechTree::loadFromString(const std::string& content) {
    techs_.clear();
    idIndex_.clear();

    std::istringstream in(content);
    std::string line;
    Tech cur;
    bool inTech = false;

    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == ';') continue;

        const std::string kw = firstToken(t);
        const std::string rest = trim(t.substr(kw.size()));

        if (kw == "BEGIN_TECH") {
            cur = Tech{};
            const std::size_t q1 = rest.find('"');
            const std::size_t q2 = (q1 == std::string::npos)
                                       ? std::string::npos
                                       : rest.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.name = rest.substr(q1 + 1, q2 - q1 - 1);
                cur.id = parseInt(firstToken(trim(rest.substr(q2 + 1))));
            }
            inTech = true;
        } else if (kw == "END_TECH") {
            if (inTech) {
                idIndex_[cur.id] = techs_.size();
                techs_.push_back(std::move(cur));
                inTech = false;
            }
        } else if (inTech) {
            if (kw == "CATEGORY") cur.category = parseInt(rest);
            else if (kw == "COST") cur.cost = parseInt(rest);
            else if (kw == "EDEN_COST") cur.edenCost = parseInt(rest);
            else if (kw == "PLYMOUTH_COST") cur.plymouthCost = parseInt(rest);
            else if (kw == "MAX_SCIENTISTS") cur.maxScientists = parseInt(rest);
            else if (kw == "LAB") cur.lab = parseInt(rest);
            else if (kw == "DESCRIPTION") cur.description = unquote(rest);
            else if (kw == "TEASER") cur.teaser = unquote(rest);
            else if (kw == "IMPROVE_DESC") cur.improveDesc = unquote(rest);
            else if (kw == "REQUIRES") cur.prereqs.push_back(parseInt(firstToken(rest)));
            else if (kw == "UNIT_PROP") {
                std::istringstream ts(rest);
                TechUnitProp up;
                std::string valTok;
                ts >> up.unit >> up.property >> valTok;
                up.value = parseInt(valTok);
                if (!up.unit.empty()) cur.unitProps.push_back(std::move(up));
            }
            // Unknown keywords are ignored.
        }
    }
    return true;
}

bool TechTree::load(const std::string& path, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "techtree: cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return loadFromString(ss.str());
}

const Tech* TechTree::byId(int id) const {
    const auto it = idIndex_.find(id);
    return (it == idIndex_.end()) ? nullptr : &techs_[it->second];
}

} // namespace op2::sim
