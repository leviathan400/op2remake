// Self-test for the map-loader plumbing. Validates that the registry routes by
// extension and that each format is wired in. (Parsing itself is stubbed, so
// load() is expected to report not-implemented for now.)

#include "op2/maps/maploader.h"

#include <cstdio>
#include <string>

using op2::maps::fileExtensionLower;
using op2::maps::IMapLoader;
using op2::maps::LoadResult;
using op2::maps::MapLoaderRegistry;

static int g_failures = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  ok    %s\n", msg);
    } else {
        std::printf("  FAIL  %s\n", msg);
        ++g_failures;
    }
}

int main() {
    std::printf("op2maps self-test\n");

    const auto reg = MapLoaderRegistry::withDefaults();
    check(reg.loaders().size() == 3, "registry has 3 built-in formats");

    // Extension helper.
    check(fileExtensionLower("C:\\maps\\eden01.MAP") == ".map", "extension is lowercased");
    check(fileExtensionLower("noext") == "", "no extension -> empty");
    check(fileExtensionLower("dir.v2/name") == "", "dot in directory ignored");

    // Dispatch by extension.
    const IMapLoader* m = reg.find("eden01.map");
    const IMapLoader* o = reg.find("mission.opm");
    const IMapLoader* j = reg.find("world.json");
    check(m && std::string(m->extension()) == ".map", "routes .map");
    check(o && std::string(o->extension()) == ".opm", "routes .opm");
    check(j && std::string(j->extension()) == ".json", "routes .json");
    check(reg.find("thing.png") == nullptr, "unknown extension -> no loader");

    // load() through the registry: matched loader runs (and reports stub),
    // unmatched path fails with a registry-level error.
    const LoadResult matched = reg.load("eden01.map");
    check(!matched.ok && !matched.error.empty(), ".map load reports not-implemented (stub)");

    const LoadResult unmatched = reg.load("thing.png");
    check(!unmatched.ok && unmatched.error.find("no registered loader") != std::string::npos,
          "unknown extension -> registry error");

    std::printf("\n%s (%d failure%s)\n",
                g_failures ? "FAILED" : "PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
