#include "op2/maps/maploader.h"

#include <cctype>

#include "op2/maps/op2maploader.h"
#include "op2/maps/op2mapperjsonloader.h"
#include "op2/maps/opmloader.h"

namespace op2::maps {

std::string fileExtensionLower(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    if (slash != std::string::npos && dot < slash) return ""; // dot is in a dir name
    std::string ext = path.substr(dot);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

bool IMapLoader::canLoad(const std::string& path) const {
    return fileExtensionLower(path) == extension();
}

void MapLoaderRegistry::registerLoader(std::unique_ptr<IMapLoader> loader) {
    if (loader) loaders_.push_back(std::move(loader));
}

IMapLoader* MapLoaderRegistry::find(const std::string& path) const {
    for (const auto& loader : loaders_) {
        if (loader->canLoad(path)) return loader.get();
    }
    return nullptr;
}

LoadResult MapLoaderRegistry::load(const std::string& path) const {
    IMapLoader* loader = find(path);
    if (!loader) {
        LoadResult r;
        r.ok = false;
        r.error = "no registered loader for: " + path;
        return r;
    }
    LoadResult r = loader->load(path);

    // Chain: a format that carries no tiles but references a separate .map (e.g. a
    // .opm naming its companion .map) gets resolved here in one hop. The referenced
    // file is a .map/.json (which never sets referencedMapFile), so this can't loop;
    // we still guard by only following when this result has no tiles of its own.
    if (!r.ok && !r.referencedMapFile.empty() && r.map.width() == 0) {
        const std::string ref = r.referencedMapFile;
        IMapLoader* inner = find(ref);
        if (inner) {
            LoadResult m = inner->load(ref);
            m.referencedMapFile = ref; // keep provenance: the .map the .opm pointed to
            if (!m.ok) m.error = "via " + path + " -> " + ref + ": " + m.error;
            return m;
        }
        r.error = "referenced map has no loader: " + ref;
    }
    return r;
}

MapLoaderRegistry MapLoaderRegistry::withDefaults() {
    MapLoaderRegistry reg;
    // To drop a format, delete its line. To add one, register it here.
    reg.registerLoader(std::make_unique<Op2MapLoader>());
    reg.registerLoader(std::make_unique<OpmLoader>());
    reg.registerLoader(std::make_unique<Op2MapperJsonLoader>());
    return reg;
}

} // namespace op2::maps
