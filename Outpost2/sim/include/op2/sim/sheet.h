#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// Sheet  —  loads an OPU tab-separated stat sheet (base/sheets/*.txt)
// =============================================================================
//
// One generic loader for all six sheets (building, vehicles, weapons, mines,
// morale, space). Format: a header (1 or 2 tab-separated rows) then data rows;
// ';' comments and trailing checksum garbage are skipped.
//
// Header detection: row 1 is always a header. If row 2's first cell is empty,
// it's a second header row and the two are combined into column names (e.g.
// "Common" + "Ore Cost" -> "Common Ore Cost"); otherwise data starts at row 2.
// Data rows with an empty first cell are skipped (filters the binary garbage).
//
// Access cells by column name or index. Pure std C++ (no Qt); lives in op2sim
// because the sim queries these stats. Typed per-sheet wrappers can be layered
// on top later.

namespace op2::sim {

class Sheet {
public:
    bool load(const std::string& path, std::string* error = nullptr);
    bool parse(const std::string& content); // filesystem-independent (tests)

    const std::vector<std::string>& columns() const { return columns_; }
    std::size_t columnCount() const { return columns_.size(); }
    std::size_t rowCount() const { return rows_.size(); }

    int columnIndex(const std::string& name) const; // -1 if absent

    // Cell access ("" if out of range / column not found).
    const std::string& cell(std::size_t row, std::size_t col) const;
    const std::string& cell(std::size_t row, const std::string& column) const;

    // Cell as integer; returns def if missing or non-numeric.
    int cellInt(std::size_t row, const std::string& column, int def = 0) const;

    const std::vector<std::string>& rowCells(std::size_t row) const;

    // First data row whose `column` equals `value`, or -1.
    int findRow(const std::string& column, const std::string& value) const;

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::unordered_map<std::string, int> colIndex_;
};

} // namespace op2::sim
