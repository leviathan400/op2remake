#include "op2/sim/sheet.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace op2::sim {

namespace {

const std::string kEmpty;

std::string trim(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Split on tabs, keeping empty cells and dropping carriage returns.
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

} // namespace

bool Sheet::parse(const std::string& content) {
    columns_.clear();
    rows_.clear();
    colIndex_.clear();

    std::vector<std::string> lines;
    {
        std::istringstream in(content);
        std::string l;
        while (std::getline(in, l)) lines.push_back(l);
    }

    // First non-blank, non-comment line is header row 1.
    std::size_t i = 0;
    for (; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (!t.empty() && t[0] != ';') break;
    }
    if (i >= lines.size()) return false;

    const std::vector<std::string> header1 = splitTabs(lines[i]);
    ++i;

    // A second header row if the next line's first cell is empty.
    std::vector<std::string> header2;
    bool twoRow = false;
    if (i < lines.size()) {
        const std::vector<std::string> next = splitTabs(lines[i]);
        if (!next.empty() && trim(next[0]).empty()) {
            header2 = next;
            twoRow = true;
            ++i;
        }
    }

    // Build combined column names.
    for (std::size_t c = 0; c < header1.size(); ++c) {
        std::string name = trim(header1[c]);
        if (twoRow && c < header2.size()) {
            const std::string sub = trim(header2[c]);
            if (!sub.empty()) name = name.empty() ? sub : (name + " " + sub);
        }
        columns_.push_back(name);
        if (!name.empty() && colIndex_.find(name) == colIndex_.end()) {
            colIndex_[name] = static_cast<int>(c);
        }
    }

    // Data rows.
    for (; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (t.empty() || t[0] == ';') continue;
        std::vector<std::string> cells = splitTabs(lines[i]);
        if (cells.empty() || trim(cells[0]).empty()) continue; // garbage / sub-rows
        for (std::string& c : cells) c = trim(c);
        rows_.push_back(std::move(cells));
    }

    return !columns_.empty();
}

bool Sheet::load(const std::string& path, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "sheet: cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str());
}

int Sheet::columnIndex(const std::string& name) const {
    const auto it = colIndex_.find(name);
    return (it == colIndex_.end()) ? -1 : it->second;
}

const std::string& Sheet::cell(std::size_t row, std::size_t col) const {
    if (row >= rows_.size() || col >= rows_[row].size()) return kEmpty;
    return rows_[row][col];
}

const std::string& Sheet::cell(std::size_t row, const std::string& column) const {
    const int c = columnIndex(column);
    if (c < 0) return kEmpty;
    return cell(row, static_cast<std::size_t>(c));
}

int Sheet::cellInt(std::size_t row, const std::string& column, int def) const {
    const std::string& v = cell(row, column);
    if (v.empty()) return def;
    try {
        return std::stoi(v, nullptr, 10);
    } catch (...) {
        return def;
    }
}

const std::vector<std::string>& Sheet::rowCells(std::size_t row) const {
    static const std::vector<std::string> empty;
    return (row < rows_.size()) ? rows_[row] : empty;
}

int Sheet::findRow(const std::string& column, const std::string& value) const {
    const int c = columnIndex(column);
    if (c < 0) return -1;
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        if (cell(r, static_cast<std::size_t>(c)) == value) return static_cast<int>(r);
    }
    return -1;
}

} // namespace op2::sim
