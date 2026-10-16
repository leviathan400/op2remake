#pragma once
#include <memory>
#include <string>
#include <vector>

#include "op2/sim/map.h"

// =============================================================================
// Map loading  —  pluggable, one isolated loader per file format
// =============================================================================
//
// Each supported format implements IMapLoader and is registered in the
// MapLoaderRegistry. Formats are fully independent: add one by registering it,
// remove one by not registering it — nothing else changes.
//
// Built-in formats (see the per-loader headers):
//   * .map  — original Outpost 2 binary map        (via OP2Utility)
//   * .opm  — OP2MissionEditor JSON (mission + a referenced .map)
//   * .json — OP2Mapper JSON (full tiles + cell types)
//
// This library is intentionally OUTSIDE op2sim: it does file I/O and will pull
// in external dependencies (OP2Utility, a JSON parser), which must never touch
// the deterministic sim core.

namespace op2::maps {

// Outcome of a load attempt.
struct LoadResult {
    bool ok = false;
    std::string error;        // human-readable reason when !ok
    op2::sim::Map map;        // populated when ok (for formats carrying tiles)

    // Some formats (e.g. .opm) don't contain tiles themselves — they reference
    // a separate .map file. When set, the caller should load this next.
    std::string referencedMapFile;
};

// One file-format loader.
class IMapLoader {
public:
    virtual ~IMapLoader() = default;

    // Display name, e.g. "Outpost 2 .map".
    virtual const char* name() const = 0;

    // The lowercase file extension this loader handles, including the dot
    // (e.g. ".map"). Used for default dispatch.
    virtual const char* extension() const = 0;

    // Whether this loader can handle the given path (default: extension match;
    // a loader may override to sniff file contents).
    virtual bool canLoad(const std::string& path) const;

    // Load the file into a LoadResult.
    virtual LoadResult load(const std::string& path) const = 0;
};

// Holds the set of active loaders and dispatches by path. Add/remove formats
// here — this is the single place that knows which formats exist.
class MapLoaderRegistry {
public:
    void registerLoader(std::unique_ptr<IMapLoader> loader);

    // First registered loader whose canLoad() accepts the path, or nullptr.
    IMapLoader* find(const std::string& path) const;

    // Convenience: find + load. Returns a failed LoadResult if no loader matches.
    LoadResult load(const std::string& path) const;

    const std::vector<std::unique_ptr<IMapLoader>>& loaders() const { return loaders_; }

    // A registry with every built-in format registered.
    static MapLoaderRegistry withDefaults();

private:
    std::vector<std::unique_ptr<IMapLoader>> loaders_;
};

// Lowercase file extension (including the leading dot), or "" if none.
std::string fileExtensionLower(const std::string& path);

} // namespace op2::maps
