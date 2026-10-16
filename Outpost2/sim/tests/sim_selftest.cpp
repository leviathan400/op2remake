// Phase-1 determinism self-test. This is the seed of the regression backbone:
// every future sim change must keep these green. Build target: op2sim_selftest
// (also registered with CTest). Returns non-zero on any failure.

#include "op2/sim/checksum.h"
#include "op2/sim/clock.h"
#include "op2/sim/cmdlog.h"
#include "op2/sim/command.h"
#include "op2/sim/prng.h"
#include "op2/sim/morale.h"
#include "op2/sim/replay.h"
#include "op2/sim/sheet.h"
#include "op2/sim/techtree.h"
#include "op2/sim/world.h"

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

using op2::sim::checksumCoreState;
using op2::sim::Checksummer;
using op2::sim::Clock;
using op2::sim::Cmdlog;
using op2::sim::CommandPacket;
using op2::sim::hashKernel;
using op2::sim::opcodeFromName;
using op2::sim::opcodeName;
using op2::sim::Prng;
using op2::sim::Replay;

namespace {

// Stand-in deterministic sim used to exercise the headless tick loop until the
// real game systems exist. Advances the clock + RNG each tick; each command
// perturbs the RNG deterministically by its opcode and payload, so the checksum
// stream is sensitive to BOTH timing and command content.
struct CoreSim {
    Clock clock;
    Prng prng;
    explicit CoreSim(std::uint32_t seed) : prng(seed) {}

    void applyCommand(const CommandPacket& c) {
        prng.getRand(c.op + 1u);
        for (std::uint8_t b : c.data) prng.getRand(static_cast<std::uint32_t>(b) + 1u);
    }
    void step() {
        clock.advance();
        prng.rand32();
    }
    std::uint32_t checksum() const { return checksumCoreState(clock, prng); }
};

// Run a fixture through the headless loop and return its checksum history.
std::vector<std::pair<std::uint32_t, std::uint32_t>>
runCore(const std::vector<CommandPacket>& packets, std::uint32_t seed,
        std::uint32_t maxTicks, std::uint32_t every) {
    CoreSim sim(seed);
    Replay r(packets);
    r.onCommand = [&](const CommandPacket& c) { sim.applyCommand(c); };
    r.onTick = [&](std::uint32_t) { sim.step(); };
    r.checksumFn = [&] { return sim.checksum(); };
    return r.run(maxTicks, every).checksums;
}

} // namespace

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
    std::printf("op2sim self-test\n");

    // 1. Determinism: same seed -> identical stream.
    {
        Prng a(999), b(999);
        bool same = true;
        for (int i = 0; i < 10000; ++i) {
            if (a.rand32() != b.rand32()) { same = false; break; }
        }
        check(same, "same seed produces an identical stream");
    }

    // 2. Seed 0 must not leave the LFSR in its all-zero death state.
    {
        Prng p(0);
        const auto st = p.state();
        check(st.s0 != 0 || st.s1 != 0, "seed 0 avoids the all-zero LFSR state");
    }

    // 3. State snapshot round-trips (save/load + net resync correctness).
    {
        Prng p(7777);
        for (int i = 0; i < 50; ++i) p.rand32();
        const auto saved = p.state();
        std::uint32_t expected[20];
        for (auto& v : expected) v = p.rand32();
        p.setState(saved);
        bool ok = true;
        for (auto e : expected) {
            if (p.rand32() != e) { ok = false; break; }
        }
        check(ok, "8-byte state snapshot round-trips");
    }

    // 4. getRand(n) stays in [0, n).
    {
        Prng p(42);
        bool inRange = true;
        for (int i = 0; i < 100000; ++i) {
            if (p.getRand(100) >= 100) { inRange = false; break; }
        }
        check(inRange, "getRand(100) stays in [0,100)");
    }

    // 5. Regression anchor: first outputs for a fixed seed. These values lock in
    //    OUR port; if you ever capture the real engine's stream, assert against it.
    {
        Prng p(12345);
        std::printf("  info  seed 12345 first 4 rand32:");
        for (int i = 0; i < 4; ++i) std::printf(" 0x%08x", p.rand32());
        std::printf("\n");
    }

    // 6. Clock: trigger gate fires once per mark, on ticks 3,7,11,...
    {
        Clock c;
        bool ok = true;
        for (int i = 0; i < 12; ++i) {
            const std::uint32_t t = c.advance(); // 1..12
            const bool want = (t == 3 || t == 7 || t == 11);
            if (c.isMarkBoundary() != want) { ok = false; break; }
        }
        check(ok, "trigger gate fires on ticks 3,7,11 (once per mark)");

        Clock c2;
        c2.setTick(8);
        check(c2.time() == 2, "Time() == tick>>2 (1 mark = 4 ticks)");
    }

    // 7. Hash kernel — bit-exact vectors from the engine (FUN_0040C050).
    {
        const std::uint8_t b0[4] = {0, 0, 0, 0};
        const std::uint8_t b1[4] = {1, 0, 0, 0};
        const std::uint8_t b8a[8] = {1, 0, 0, 0, 0, 0, 0, 0};
        const std::uint8_t b8b[8] = {1, 0, 0, 0, 1, 0, 0, 0};
        check(hashKernel(0, nullptr) == 1u, "hashKernel(0) -> 1 (empty sentinel)");
        check(hashKernel(4, b0) == 1u, "hashKernel(4x00) -> 1 (zero maps to 1)");
        check(hashKernel(4, b1) == 0x80000000u, "hashKernel(dword=1) -> 0x80000000");
        check(hashKernel(8, b8a) == 0x40000000u, "hashKernel(1,0) -> 0x40000000");
        check(hashKernel(8, b8b) == 0xC0000000u, "hashKernel(1,1) -> 0xC0000000");
    }

    // 8. Checksummer — deterministic, order-sensitive, zero->1.
    {
        Checksummer a, b;
        a.addU32(1); a.addU32(2); a.addU32(3);
        b.addU32(1); b.addU32(2); b.addU32(3);
        check(a.value() == b.value(), "Checksummer: same input -> same value");

        Checksummer c, d;
        c.addU32(1); c.addU32(2);
        d.addU32(2); d.addU32(1);
        check(c.value() != d.value(), "Checksummer: order-sensitive");

        Checksummer e;
        check(e.value() == 1u, "Checksummer: empty -> 1 (zero maps to 1)");
    }

    // 9. Core-state checksum — stable for equal state, sensitive to change.
    {
        Clock c1; Prng p1(123);
        Clock c2; Prng p2(123);
        check(checksumCoreState(c1, p1) == checksumCoreState(c2, p2),
              "checksumCoreState: equal core state -> equal checksum");

        const std::uint32_t before = checksumCoreState(c1, p1);
        p1.rand32(); // advance only the RNG
        check(checksumCoreState(c1, p1) != before,
              "checksumCoreState: RNG advance changes checksum");

        Clock c3; Prng p3(123);
        c3.advance(); // advance only the clock
        check(checksumCoreState(c3, p3) != checksumCoreState(c2, p2),
              "checksumCoreState: tick advance changes checksum");
    }

    // 10. Opcode name <-> id round-trip.
    {
        check(opcodeName(2) == "ctMoMove", "opcodeName(2) == ctMoMove");
        check(opcodeFromName("ctMoBuild") == 6, "opcodeFromName(ctMoBuild) == 6");
        check(opcodeName(0xFFFFFFFEu) == "<START_GAME>", "opcodeName(marker) == <START_GAME>");
    }

    // Build a synthetic fixture for the replay tests.
    auto makeFixture = []() {
        std::vector<CommandPacket> v;
        v.push_back(CommandPacket{0, 0xFFFFFFFEu, -1, {}});                  // start game
        v.push_back(CommandPacket{10, 2, 0, {0x01, 0x05, 0x00, 0x04}});      // ctMoMove
        v.push_back(CommandPacket{50, 6, 1, {0x01, 0x0a, 0x00}});            // ctMoBuild
        v.push_back(CommandPacket{200, 22, 0, {0x14, 0x49, 0x0d}});          // ctMoResearch
        v.push_back(CommandPacket{500, 49, 2, {0x01, 0x02}});                // ctQuit
        return v;
    };
    const auto fixture = makeFixture();

    // 11. Headless replay is deterministic: same fixture + seed -> same history.
    {
        auto h1 = runCore(fixture, 0x1234, 600, 50);
        auto h2 = runCore(fixture, 0x1234, 600, 50);
        check(!h1.empty(), "replay produced a checksum history");
        check(Replay::diff(h1, h2).empty(),
              "replay: same fixture + seed -> identical checksum history");
    }

    // 12. Divergence detection: different seed -> diff finds the divergence.
    {
        auto hA = runCore(fixture, 0x1111, 600, 50);
        auto hB = runCore(fixture, 0x2222, 600, 50);
        const auto divs = Replay::diff(hA, hB);
        check(!divs.empty(), "replay: different seed -> divergence detected");
    }

    // 13. Command sensitivity: an extra command shifts the stream from then on.
    {
        auto modified = fixture;
        modified.push_back(CommandPacket{20, 5, 0, {0xAB}}); // ctMoStop at tick 20
        auto hBase = runCore(fixture, 0x9999, 600, 50);
        auto hMod = runCore(modified, 0x9999, 600, 50);
        const auto divs = Replay::diff(hBase, hMod);
        check(!divs.empty(), "replay: extra command changes the checksum stream");
        // The pre-command sample (tick 0) must still agree.
        bool tick0Agrees = true;
        for (const auto& d : divs) {
            if (d.tick == 0) { tick0Agrees = false; break; }
        }
        check(tick0Agrees, "replay: divergence is localized after the change (tick 0 agrees)");
    }

    // 14. JSONL round-trip preserves every field.
    {
        Cmdlog out;
        out.packets = fixture;
        const char* path = "op2_cmdlog_roundtrip.jsonl";
        bool ok = true;
        try {
            out.writeJsonl(path, "selftest");
            Cmdlog in = Cmdlog::readJsonl(path);
            ok = (in.packets.size() == fixture.size());
            for (std::size_t i = 0; ok && i < fixture.size(); ++i) {
                ok = in.packets[i].tick == fixture[i].tick &&
                     in.packets[i].op == fixture[i].op &&
                     in.packets[i].player == fixture[i].player &&
                     in.packets[i].data == fixture[i].data;
            }
        } catch (...) {
            ok = false;
        }
        std::remove(path);
        check(ok, "cmdlog: JSONL write -> read round-trips all fields");
    }

    // 15. TechTree parser.
    {
        const char* fixture =
            "; a comment\n"
            "BEGIN_TECH \"Tube\" 02114\n"
            "    CATEGORY 0\n"
            "    COST 0\n"
            "END_TECH\n"
            "BEGIN_TECH \"Cybernetic Teleoperation\" 03408\n"
            "    CATEGORY 4\n"
            "    DESCRIPTION \"Allows production of factory kits.\"\n"
            "    COST 300\n"
            "    EDEN_COST 250\n"
            "    MAX_SCIENTISTS 5\n"
            "    LAB 2\n"
            "END_TECH\n"
            "BEGIN_TECH \"Vehicle Factory\" 03502\n"
            "    CATEGORY 0\n"
            "    REQUIRES 03408\n"
            "    COST 0\n"
            "    UNIT_PROP TOKAMAK Production_Capacity 300\n"
            "END_TECH\n";

        op2::sim::TechTree tt;
        tt.loadFromString(fixture);
        check(tt.size() == 3, "techtree: parsed 3 techs");

        const op2::sim::Tech* cy = tt.byId(3408);
        check(cy && cy->name == "Cybernetic Teleoperation", "techtree: name (quoted) parsed");
        check(cy && cy->cost == 300 && cy->edenCost == 250, "techtree: COST + EDEN_COST");
        check(cy && cy->maxScientists == 5 && cy->lab == 2, "techtree: MAX_SCIENTISTS + LAB");
        check(cy && cy->description == "Allows production of factory kits.",
              "techtree: DESCRIPTION unquoted");

        const op2::sim::Tech* vf = tt.byId(3502);
        check(vf && vf->prereqs.size() == 1 && vf->prereqs[0] == 3408,
              "techtree: REQUIRES -> prereq id (leading zeros dropped)");
        check(vf && vf->unitProps.size() == 1 &&
                  vf->unitProps[0].unit == "TOKAMAK" &&
                  vf->unitProps[0].property == "Production_Capacity" &&
                  vf->unitProps[0].value == 300,
              "techtree: UNIT_PROP parsed");

        check(tt.byId(99999) == nullptr, "techtree: byId miss -> nullptr");
    }

    // 16. Sheet parser — two-row header (tab-separated).
    {
        // \t separators; row 2's first cell empty -> 2-row header.
        const char* fixture =
            "Code_Name\tUnit Name\tHit\tCommon\tWeapon\n"
            "\t\tPoints\tOre Cost\tenabled\n"
            "BIG_TANK\tc\t800\t750\tY\n"
            "SCOUT\tm\t125\t250\tN\n"
            "; Garbage so checksum matches original:xyz\n";

        op2::sim::Sheet sh;
        sh.parse(fixture);
        check(sh.rowCount() == 2, "sheet: 2 data rows (header + garbage skipped)");
        check(sh.columnIndex("Hit Points") == 2, "sheet: 2-row header combined ('Hit Points')");
        check(sh.cell(0, "Code_Name") == "BIG_TANK", "sheet: cell by column name");
        check(sh.cellInt(0, "Hit Points") == 800, "sheet: cellInt");
        check(sh.cell(1, "Weapon enabled") == "N", "sheet: combined col 'Weapon enabled'");
        check(sh.findRow("Code_Name", "SCOUT") == 1, "sheet: findRow by column value");
    }

    // 17. Sheet parser — single-row header (mines/morale style).
    {
        const char* fixture =
            "YIELD\tVARIANT\tINITIAL_YIELD_%\n"
            "High\t1\t50\n"
            "Med\t2\t30\n";
        op2::sim::Sheet sh;
        sh.parse(fixture);
        check(sh.rowCount() == 2 && sh.columnCount() == 3, "sheet: single-row header parsed");
        check(sh.cellInt(0, "INITIAL_YIELD_%") == 50, "sheet: single-header cell value");
    }

    // 18. Morale parser — levels table + flat name->value bonus blocks.
    {
        const char* fixture =
            "\tM_PTS_TOT\tM_POW_BONUS\tM_RSCH_BONUS\tM_FACT_BONUS\tM_FOOD_BONUS\tM_DEFECT_RATE\tM_FERT_RATE\tM_DIE_RATE\n"
            "UTOPIA\t90\t0\t15\t-25\t20\t0\t35\t620\n"
            "WAYBAD\t0\t0\t-15\t20\t-10\t10\t107\t180\n"
            "\n"
            "KID_DIES\t\t-8\n"
            "CC_BORN\t\t10\n"
            "\n"
            "DIFF_EASY\t\t100\n"
            "; Garbage so checksum matches original:71|\n";

        op2::sim::MoraleData md;
        md.parse(fixture);
        check(md.levels().size() == 2, "morale: 2 levels parsed");
        const op2::sim::MoraleLevel* u = md.levelByName("UTOPIA");
        check(u && u->ptsTotal == 90 && u->rschBonus == 15 && u->dieRate == 620,
              "morale: level columns (UTOPIA)");
        check(md.bonus("KID_DIES") == -8 && md.bonus("CC_BORN") == 10,
              "morale: event bonuses");
        check(md.bonus("DIFF_EASY") == 100, "morale: difficulty bonus");
        check(md.bonus("MISSING", 7) == 7, "morale: missing bonus -> default");
        check(md.bonusCount() == 3, "morale: bonus count (garbage skipped)");
    }

    // 19. World — game-object pool: spawn/destroy, stable handles, save/load.
    {
        using op2::sim::World;
        using op2::sim::Unit;
        using op2::sim::UnitId;

        World w;
        using op2::sim::UnitKind;
        Unit b{}; b.typeId = 0x2F; b.owner = 1; b.kind = UnitKind::Building; b.tileX = 10; b.tileY = 20; b.hitPoints = 800;
        Unit v{}; v.typeId = 100; v.owner = 1; v.kind = UnitKind::Vehicle; v.rotation = 64; v.tileX = 5; v.tileY = 5; v.hitPoints = 400;

        UnitId hb = w.spawn(b);
        UnitId hv = w.spawn(v);
        check(w.liveCount() == 2, "world: spawn -> liveCount 2");
        check(w.get(hb) && w.get(hb)->tileX == 10 && w.get(hb)->owner == 1,
              "world: handle resolves to spawned unit");
        check(w.get(hb) && w.get(hb)->kind == UnitKind::Building &&
              w.get(hv) && w.get(hv)->kind == UnitKind::Vehicle,
              "world: object kind stored per instance");

        // Destroy + stale-handle safety.
        w.destroy(hb);
        check(w.liveCount() == 1, "world: destroy -> liveCount 1");
        check(w.get(hb) == nullptr, "world: handle to destroyed unit -> nullptr");

        // Free-slot reuse with a fresh generation; the OLD handle stays stale.
        Unit v2{}; v2.typeId = 101; v2.tileX = 99;
        UnitId hv2 = w.spawn(v2);
        check(hv2.index == hb.index, "world: free slot reused");
        check(hv2.generation != hb.generation, "world: reused slot bumps generation");
        check(w.get(hb) == nullptr, "world: stale handle stays null after reuse");
        check(w.get(hv2) && w.get(hv2)->tileX == 99, "world: new handle resolves");

        // forEach visits only live units.
        int seen = 0; w.forEach([&](UnitId, Unit&) { ++seen; });
        check(seen == 2, "world: forEach visits the 2 live units");

        // Checksum: equal worlds -> equal fingerprint.
        Checksummer ca, cb; w.foldInto(ca);
        // Rebuild with w's EXACT sequence (spawn b, spawn v, destroy b, spawn v2)
        // so the slot layout + live set match.
        World w3; UnitId hb3 = w3.spawn(b); w3.spawn(v); w3.destroy(hb3); w3.spawn(v2);
        w3.foldInto(cb);
        check(ca.value() == cb.value(), "world: identical live state -> identical checksum");

        // Save/load round-trip: bytes restore an equal world (same checksum).
        auto bytes = w.serialize();
        World loaded;
        check(loaded.deserialize(bytes), "world: deserialize succeeds");
        check(loaded.liveCount() == w.liveCount(), "world: save/load preserves liveCount");
        Checksummer cl; loaded.foldInto(cl);
        check(cl.value() == ca.value(), "world: save/load preserves the state checksum");
        // Handles survive the round-trip (index+generation are serialised).
        check(loaded.get(hv2) && loaded.get(hv2)->tileX == 99,
              "world: handles still resolve after load");
        check(loaded.get(hv) && loaded.get(hv)->kind == UnitKind::Vehicle,
              "world: kind survives save/load");
        check(loaded.deserialize({1, 2, 3}) == false, "world: malformed buffer rejected");
    }

    // 20. Command dispatch — packets are the only mutation path into the World.
    {
        using op2::sim::applyCommand;
        using op2::sim::debugSetDisabled;
        using op2::sim::makeIdleCommand;
        using op2::sim::Unit;
        namespace UnitFlag = op2::sim::UnitFlag;
        using op2::sim::UnitId;
        using op2::sim::World;

        World w;
        Unit b{}; b.typeId = 1; b.kind = op2::sim::UnitKind::Building; b.hitPoints = 500;
        const UnitId id = w.spawn(b);

        // Idle on, then off, via real ctMoIdle/ctMoUnidle packets.
        check(applyCommand(w, makeIdleCommand(100, 0, id, true)),
              "command: ctMoIdle applies");
        check(w.get(id)->flags & UnitFlag::Idle, "command: ctMoIdle sets the Idle flag");
        check(applyCommand(w, makeIdleCommand(101, 0, id, false)),
              "command: ctMoUnidle applies");
        check(!(w.get(id)->flags & UnitFlag::Idle), "command: ctMoUnidle clears the flag");

        // The packet round-trips through the JSONL cmdlog (ctMoIdle is a real
        // OP2 opcode, so the name table covers it).
        const CommandPacket pkt = makeIdleCommand(100, 0, id, true);
        check(opcodeName(pkt.op) == "ctMoIdle", "command: packet uses the real opcode");
        UnitId decoded;
        check(op2::sim::decodeUnitTarget(pkt, &decoded) && decoded == id,
              "command: payload decodes back to the target handle");

        // Rejections must not half-apply: stale handle, unknown op, short payload.
        w.destroy(id);
        check(!applyCommand(w, makeIdleCommand(102, 0, id, true)),
              "command: stale handle rejected");
        const UnitId id2 = w.spawn(b);
        CommandPacket bad = makeIdleCommand(103, 0, id2, true);
        bad.op = 999;
        check(!applyCommand(w, bad), "command: unknown opcode rejected");
        bad = makeIdleCommand(104, 0, id2, true);
        bad.data.resize(3); // truncated target
        check(!applyCommand(w, bad), "command: malformed payload rejected");
        check(!(w.get(id2)->flags & UnitFlag::Idle),
              "command: rejected packets change nothing");

        // Debug backdoor for the not-yet-simulated Disabled condition.
        check(debugSetDisabled(w, id2, true) && (w.get(id2)->flags & UnitFlag::Disabled),
              "command: debugSetDisabled sets the flag");
        check(debugSetDisabled(w, id2, false) && !(w.get(id2)->flags & UnitFlag::Disabled),
              "command: debugSetDisabled clears the flag");
        check(!debugSetDisabled(w, id, true), "command: debug poke rejects stale handles");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures ? "FAILED" : "PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
