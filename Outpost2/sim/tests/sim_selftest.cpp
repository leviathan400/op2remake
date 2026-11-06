// Phase-1 determinism self-test. This is the seed of the regression backbone:
// every future sim change must keep these green. Build target: op2sim_selftest
// (also registered with CTest). Returns non-zero on any failure.

#include "op2/sim/checksum.h"
#include "op2/sim/clock.h"
#include "op2/sim/cmdlog.h"
#include "op2/sim/command.h"
#include "op2/sim/combat.h"
#include "op2/sim/game.h"
#include "op2/sim/movement.h"
#include "op2/sim/pathfind.h"
#include "op2/sim/prng.h"
#include "op2/sim/morale.h"
#include "op2/sim/replay.h"
#include "op2/sim/sheet.h"
#include "op2/sim/techtree.h"
#include "op2/sim/world.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
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

    // 5. Reference vectors: SetSeed @0x46EFB0 and Rand @0x46F000 as decoded from
    //    Outpost2.exe (GOG 1.2), first 4 rand32 then one getRand(1000). Seed
    //    0x80001234 pins the arithmetic `sar` in SetSeed (a logical shift gives
    //    0x091B091A first); seeds < 0x10000 leave state0 at 0.
    {
        struct Vec { std::uint32_t seed, s0, s1, out[4], r1000; };
        const Vec vecs[] = {
            {1,           0x00000000u, 0x00010001u, {0x80008000u, 0x40004000u, 0x20002000u, 0x90001000u}, 281},
            {12345,       0x00000000u, 0x30393039u, {0x181C981Cu, 0x0C0E4C0Eu, 0x06072607u, 0x03039303u}, 505},
            {0x80001234u, 0xFFFF8000u, 0x12341234u, {0xF6E5091Bu, 0xFB73048Eu, 0x7DBA8240u, 0xBEDB412Fu}, 372},
            {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0x80000001u, 0xC0000003u, 0xE0000007u, 0xF000000Fu}, 968},
            {0,           0x10001000u, 0x00010001u, {0xA000A000u, 0x00000001u, 0xA000A002u, 0x90011004u}, 281},
        };
        bool seeded = true, stream = true;
        for (const Vec& v : vecs) {
            Prng p(v.seed);
            if (p.state().s0 != v.s0 || p.state().s1 != v.s1) seeded = false;
            for (std::uint32_t e : v.out) {
                if (p.rand32() != e) stream = false;
            }
            if (p.getRand(1000) != v.r1000) stream = false;
        }
        check(seeded, "SetSeed matches the exe (arithmetic shift, seed 0 -> 0x10000001)");
        check(stream, "Rand/GetRand stream matches the exe reference vectors");
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
        Unit b{}; b.typeId = 0x2F; b.owner = 1; b.kind = UnitKind::Building; b.px = 336; b.py = 656;
        Unit v{}; v.typeId = 100; v.owner = 1; v.kind = UnitKind::Vehicle; v.rotation = 64; v.px = 176; v.py = 176;

        UnitId hb = w.spawn(b);
        UnitId hv = w.spawn(v);
        check(w.liveCount() == 2, "world: spawn -> liveCount 2");
        check(w.get(hb) && w.get(hb)->px == 336 && w.get(hb)->owner == 1,
              "world: handle resolves to spawned unit");
        check(w.get(hb) && w.get(hb)->kind == UnitKind::Building &&
              w.get(hv) && w.get(hv)->kind == UnitKind::Vehicle,
              "world: object kind stored per instance");

        // Destroy + stale-handle safety.
        w.destroy(hb);
        check(w.liveCount() == 1, "world: destroy -> liveCount 1");
        check(w.get(hb) == nullptr, "world: handle to destroyed unit -> nullptr");

        // A freed index isn't reused at once (128-tick delay, see 19d); the old
        // handle stays stale.
        Unit v2{}; v2.typeId = 101; v2.px = 99;
        UnitId hv2 = w.spawn(v2);
        check(hb.index == 1 && hv.index == 2 && hv2.index == 3, "world: indices start at 1, fresh ones count up");
        check(w.get(hb) == nullptr, "world: stale handle stays null");
        check(w.get(hv2) && w.get(hv2)->px == 99, "world: new handle resolves");

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
        check(loaded.get(hv2) && loaded.get(hv2)->px == 99,
              "world: handles still resolve after load");
        check(loaded.get(hv) && loaded.get(hv)->kind == UnitKind::Vehicle,
              "world: kind survives save/load");
        check(loaded.deserialize({1, 2, 3}) == false, "world: malformed buffer rejected");

        // The slot layout is part of the checksum: the same live unit in slot 0
        // vs slot 1 must not hash equal (handles and later spawns differ).
        World wa; wa.spawn(v);
        World wb; const UnitId tmp = wb.spawn(b); wb.spawn(v); wb.destroy(tmp);
        Checksummer sa, sb; wa.foldInto(sa); wb.foldInto(sb);
        check(sa.value() != sb.value(), "world: same units in other slots -> different checksum");

        // The whole-sim checksum sees the World, not just clock + RNG.
        {
            Clock ck; Prng pr(5);
            World w1; w1.spawn(v);
            World w2; Unit hurt = v; hurt.damage += 1; w2.spawn(hurt);
            check(op2::sim::checksumSimState(ck, pr, w1) != op2::sim::checksumSimState(ck, pr, w2),
                  "checksumSimState: a unit's damage changes the checksum");
            check(op2::sim::checksumSimState(ck, pr, w1) == op2::sim::checksumSimState(ck, pr, w1),
                  "checksumSimState: equal state -> equal checksum");
        }

        // Corrupt allocator state is rejected (spawn would write out of range or
        // over a live unit). Layout: 12-byte header, the slots (slot 0
        // included), ring head, tail, object count, the ring's entries, then the
        // processing order.
        {
            World wd; const UnitId a = wd.spawn(b); wd.spawn(v); wd.destroy(a); // index 1 freed
            const auto good = wd.serialize();
            World t;
            check(t.deserialize(good), "world: valid allocator state accepted");
            auto patch = [&](std::size_t at, std::uint32_t val) {
                auto bad = good;
                for (int k = 0; k < 4; ++k) bad[at + k] = static_cast<std::uint8_t>(val >> (8 * k));
                return bad;
            };
            // 12 header + 3 slots + head, tail, count (12) + 1 ring entry + 1 order entry + the human mask.
            const std::size_t slotSize = (good.size() - 12 - 12 - 4 - 4 - 1) / 3;
            const std::size_t ringEntry = 12 + 3 * slotSize + 12;
            check(!t.deserialize(patch(ringEntry, 7)), "world: ring entry past the slots rejected");
            check(!t.deserialize(patch(ringEntry, 2)), "world: ring entry naming a live unit rejected");
            check(!t.deserialize(patch(ringEntry, 0)), "world: ring entry naming index 0 rejected");
            check(!t.deserialize(patch(8, 0x00FFFFFFu)), "world: slot count beyond the buffer rejected");
            auto longer = good; longer.push_back(0);
            check(!t.deserialize(longer), "world: trailing bytes rejected");
            check(!t.deserialize(patch(ringEntry + 4, 1)), "world: processing order naming a dead slot rejected");
        }

        // Hit points are derived: the type's max minus the damage, floored at 0.
        {
            World wh;
            op2::sim::UnitType tt; tt.maxHitPoints = 400;
            wh.setType(100, tt);
            Unit hit = v; hit.damage = 150;
            const UnitId id = wh.spawn(hit);
            check(wh.hitPoints(*wh.get(id)) == 250, "world: hitPoints = max - damage");
            wh.get(id)->damage = 900;
            check(wh.hitPoints(*wh.get(id)) == 0, "world: hitPoints never below 0");
        }
    }

    // 19d. Object index allocation, as Outpost2.exe does it (0x439A10, 0x439C20).
    {
        using op2::sim::Unit;
        using op2::sim::UnitId;
        using op2::sim::World;
        Unit u{}; u.kind = op2::sim::UnitKind::Vehicle;

        // Lazy reuse: a freed index comes back only once freed + 128 < tick.
        {
            World w;
            const UnitId a = w.spawn(u);
            w.setTick(10); w.destroy(a);
            w.setTick(138);
            const UnitId x = w.spawn(u);
            w.setTick(139);
            const UnitId y = w.spawn(u);
            check(a.index == 1 && x.index == 2 && y.index == 1,
                  "alloc: freed at 10, not reused at 138, reused at 139");
            check(y.generation != a.generation && w.get(a) == nullptr, "alloc: the reused index gets a new generation");
        }
        // A reference on the dead unit's index holds it back.
        {
            World w;
            const UnitId a = w.spawn(u);
            w.addRef(a);
            w.destroy(a);
            w.setTick(1000);
            const UnitId x = w.spawn(u);
            w.releaseRef(a); // a stale handle still releases
            const UnitId y = w.spawn(u);
            check(x.index == 2 && y.index == 1, "alloc: a referenced index waits until released");
        }
        // The scan takes the first eligible entry from the head; the ones before
        // it keep their places and order.
        {
            World w;
            const UnitId a = w.spawn(u), b = w.spawn(u), c = w.spawn(u);
            w.addRef(a);
            w.destroy(a); w.destroy(b);   // ring: a (held), b
            w.setTick(200); w.destroy(c); // ring: a, b, c (c too recent at 300)
            w.setTick(300);
            const UnitId x = w.spawn(u);
            w.releaseRef(a);
            const UnitId y = w.spawn(u);
            const UnitId z = w.spawn(u);
            check(x.index == b.index && y.index == a.index && z.index == 4,
                  "alloc: first eligible from the head; skipped entries stay in order");
        }
        // The exe advances the ring head mod 0x3FF but the tail mod 0x400, so after
        // 1023 reuses the head jumps 1022 -> 0 over the entry the tail just wrote at
        // 1023: the ring looks empty and a fresh index is taken.
        {
            World w;
            int firstFresh = -1;
            for (int cycle = 0; cycle < 1100 && firstFresh < 0; ++cycle) {
                w.setTick(static_cast<std::uint32_t>(cycle) * 200);
                const UnitId id = w.spawn(u);
                if (cycle > 0 && id.index != 1) firstFresh = cycle;
                w.destroy(id);
            }
            check(firstFresh == 1024, "alloc: ring head wraps at 0x3FF (exe quirk): fresh index on cycle 1024");
        }
        // At most 0x3FF objects; the next spawn fails.
        {
            World w;
            UnitId lastId;
            for (std::uint32_t k = 0; k < World::kMaxObjects; ++k) lastId = w.spawn(u);
            check(lastId.index == World::kMaxObjects && !w.spawn(u).valid(),
                  "alloc: indices 1..0x3FF, then spawn fails");
        }
        // Allocator state is saved and checksummed: a restored world hands out the
        // same next index.
        {
            World w;
            const UnitId a = w.spawn(u); w.spawn(u);
            w.setTick(5); w.destroy(a);
            World r;
            check(r.deserialize(w.serialize()), "alloc: allocator state round-trips");
            w.setTick(200); r.setTick(200);
            check(w.spawn(u).index == r.spawn(u).index && r.get(UnitId{1, 2}) != nullptr,
                  "alloc: a restored world reuses the same index");
            World w2; w2.spawn(u); w2.spawn(u); // same units, nothing freed
            Checksummer x, y; w.foldInto(x); w2.foldInto(y);
            check(x.value() != y.value(), "alloc: allocator history changes the checksum");
        }
        // The heartbeat and idle mood follow the index.
        check(op2::sim::isHeartbeatTick(5, 5) && op2::sim::isHeartbeatTick(5, 13) &&
                  !op2::sim::isHeartbeatTick(5, 6) && op2::sim::isHeartbeatTick(1, 9),
              "heartbeat: (index ^ tick) & 7 == 0");
        int beats = 0;
        for (std::uint32_t t = 0; t < 64; ++t) beats += op2::sim::isHeartbeatTick(3, t);
        check(beats == 8, "heartbeat: once every 8 ticks");
        check(op2::sim::idleMood(3, 512) == 1 && op2::sim::idleMood(3, 767) == 1 && op2::sim::idleMood(3, 768) == 0,
              "idle mood: (index ^ tick >> 8) & 15, changing every 256 ticks");
    }

    // 19b. Coordinates and turning (coords.h; facts from the original game).
    {
        using namespace op2::sim;
        check(pixelToTile(0) == 0 && pixelToTile(31) == 0 && pixelToTile(32) == 1 && pixelToTile(-1) == -1,
              "coords: pixel -> tile is floor division by 32");
        check(tileCentrePx(5) == 176, "coords: a tile's centre is +16");
        // A 3x2 structure on centre tile (26,30): footprint (25..27, 29..30), pixel at
        // the footprint centre (848, 960) — on a tile edge in y (even height).
        check(structurePx(26, 3) == 848 && structurePx(30, 2) == 960,
              "coords: structure pixel is its footprint centre");
        check(structureTopLeft(848, 3) == 25 && structureTopLeft(960, 2) == 29,
              "coords: top-left footprint tile from the pixel");
        check(structurePx(7, 1) == tileCentrePx(7), "coords: a 1x1 unit sits on its tile centre");
        check(pixelToTile(structurePx(26, 2)) == 26 && pixelToTile(structurePx(30, 3)) == 30,
              "coords: the pixel's tile is the .opm centre tile");
        check(internalTileX(10, 64) == 42 && fileTileX(42, 64) == 10,
              "coords: engine tile x = file x + 32 on a non-wrapping map");
        check(internalTileX(10, 512) == 10 && wrapTileX(-1, 512) == 511 && wrapTileX(512, 512) == 0 &&
                  wrapTileX(-1, 64) == -1,
              "coords: 512-wide maps have no offset and wrap at 512");
        check(headingOfDir(2) == Heading::South && kDirSteps[2].dx == 0 && kDirSteps[2].dy == 1 &&
                  kDirSteps[1].dx == 1 && kDirSteps[1].dy == 1,
              "coords: heading 0 = east, 64 = south, clockwise");
        // A Lynx (turn rate 16) takes 2 ticks for a 45-degree corner.
        std::uint8_t h = Heading::East;
        int ticks = 0;
        while (h != 32) { h = turnTowards(h, 32, 16); ++ticks; }
        check(ticks == 2, "turn: 45 degrees at rate 16 takes 2 ticks");
        check(turnTowards(0, 128, 16) == 240, "turn: an exact half turn goes counter-clockwise");
        check(turnTowards(0, 224, 16) == 240 && turnTowards(250, 4, 16) == 4,
              "turn: the shorter way, snapping when within a step");
    }

    // 19c. Processing order: sorted by pixel y, new objects in front of their row,
    //      a stable re-sort after every object ran, spawns mid-tick not run twice.
    {
        using op2::sim::Unit;
        using op2::sim::UnitId;
        using op2::sim::World;
        World w;
        auto at = [](int py) { Unit u{}; u.kind = op2::sim::UnitKind::Vehicle; u.py = py; return u; };
        const UnitId a = w.spawn(at(100));
        const UnitId b = w.spawn(at(50));
        const UnitId c = w.spawn(at(100)); // same row as a: goes in front of it
        auto is = [&](std::initializer_list<UnitId> ids) {
            std::vector<std::uint32_t> want;
            for (const UnitId& id : ids) want.push_back(id.index);
            return w.order() == want;
        };
        check(is({b, c, a}), "order: sorted by pixel y, newer first on an equal row");

        // b drives down below the others; the closing sort moves it to the end and
        // keeps c before a (stable).
        std::vector<std::uint32_t> visited;
        w.processUnits([&](UnitId id, Unit& u) {
            visited.push_back(id.index);
            if (id == b) u.py = 200;
        });
        check(visited == std::vector<std::uint32_t>{b.index, c.index, a.index},
              "order: processUnits runs objects in list order");
        check(is({c, a, b}), "order: stable re-sort by pixel y after the tick");

        // Objects made while another is processed: one above the cursor isn't run
        // this tick, one below is, and nobody runs twice.
        visited.clear();
        UnitId above, below;
        w.processUnits([&](UnitId id, Unit&) {
            visited.push_back(id.index);
            if (id == a) { above = w.spawn(at(10)); below = w.spawn(at(300)); }
        });
        check(visited == std::vector<std::uint32_t>{c.index, a.index, b.index, below.index},
              "order: a mid-tick spawn joins without shifting the current object");
        check(is({above, c, a, b, below}), "order: mid-tick spawns are in place for the next tick");

        // Destroying the object being processed (or one before it) skips nobody.
        visited.clear();
        w.processUnits([&](UnitId id, Unit&) {
            visited.push_back(id.index);
            if (id == c) { w.destroy(c); w.destroy(above); }
        });
        check(visited == std::vector<std::uint32_t>{above.index, c.index, a.index, b.index, below.index},
              "order: destroys during processing skip no one");
        check(is({a, b, below}), "order: destroyed objects leave the list");

        // The order is saved, restored and checksummed.
        World r;
        check(r.deserialize(w.serialize()) && r.order() == w.order(), "order: survives save/load");
        Checksummer c1, c2; w.foldInto(c1);
        World w2; w2.deserialize(w.serialize());
        w2.get(a)->py = 400; w2.sortOrder(); // same units, a different list
        w2.get(a)->py = w.get(a)->py;
        w2.foldInto(c2);
        check(c1.value() != c2.value(), "order: a different processing order changes the checksum");
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
        Unit b{}; b.typeId = 1; b.kind = op2::sim::UnitKind::Building;
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

    // 21. Game — the whole sim in one object: step order, save/restore mid-run.
    {
        using op2::sim::Game;
        using op2::sim::Unit;
        using op2::sim::UnitId;
        Game g(7);
        Unit b{}; b.kind = op2::sim::UnitKind::Building; b.px = 336; b.py = 336;
        const UnitId id = g.world().spawn(b);
        g.queue(op2::sim::makeIdleCommand(0, 0, id, true));
        check(g.step().empty() && g.tick() == 1 && (g.world().get(id)->flags & op2::sim::UnitFlag::Idle),
              "game: a queued packet applies on the next step, then the clock advances");
        g.queue(op2::sim::makeIdleCommand(1, 0, UnitId{99, 1}, true));
        check(g.step().size() == 1 && g.queued().empty(), "game: rejected packets are returned, the queue empties");

        // Save mid-run with a packet queued; the restored game continues identically.
        g.queue(op2::sim::makeIdleCommand(2, 0, id, false));
        const auto bytes = g.serialize();
        Game r;
        check(r.deserialize(bytes) && r.tick() == g.tick() && r.checksum() == g.checksum(),
              "game: save/restore keeps tick, RNG, queue and World");
        for (int k = 0; k < 50; ++k) { g.step(); r.step(); }
        check(r.checksum() == g.checksum() && !(r.world().get(id)->flags & op2::sim::UnitFlag::Idle),
              "game: a restored game runs on identically");
        auto cut = bytes; cut.pop_back();
        check(!Game().deserialize(cut), "game: a truncated save is rejected");
    }

    // 22. Movement — the timings op2-rs's harness measured on the original
    //     (harness/oracle/FINDINGS.md "Speed", "Turning", "Orders", "Paths").
    {
        using namespace op2::sim;
        // Type 0: Lynx (wheeled, Move Points 3, Turn Rate 16); type 1: Tiger (tracked, 5, 8).
        auto makeGame = [](CellType ground) {
            Game g;
            g.map().resize(40, 40);
            for (int y = 0; y < 40; ++y)
                for (int x = 0; x < 40; ++x) g.map().at(x, y).cellType = ground;
            UnitType lynx; lynx.kind = UnitKind::Vehicle; lynx.track = Track::Wheeled; lynx.moveSpeed = 3; lynx.turnRate = 16; lynx.maxHitPoints = 300;
            UnitType tiger = lynx; tiger.track = Track::Tracked; tiger.moveSpeed = 5; tiger.turnRate = 8;
            g.world().setType(0, lynx);
            g.world().setType(1, tiger);
            return g;
        };
        auto spawnAt = [](Game& g, std::uint16_t type, int x, int y, std::uint8_t dir) {
            Unit u; u.typeId = type; u.kind = UnitKind::Vehicle; u.rotation = dir;
            u.px = tileCentrePx(x); u.py = tileCentrePx(y);
            return g.world().spawn(u);
        };
        // Ticks from the order (given at tick 0) until the unit stands on the goal's centre.
        auto arrival = [](Game& g, UnitId id, int gx, int gy) {
            g.queue(makeMoveCommand(g.tick(), 0, id, gx, gy));
            for (int k = 0; k < 2000; ++k) {
                g.step();
                const Unit* u = g.world().get(id);
                if (u->stepTicks == 0 && u->px == tileCentrePx(gx) && u->py == tileCentrePx(gy)) return static_cast<int>(g.tick());
            }
            return -1;
        };
        // 3 tiles; the order is taken in cycle 1 and the unit drives from cycle 2.
        auto perTile = [&](CellType ground, std::uint16_t type, int dx, int dy) {
            Game g = makeGame(ground);
            const std::uint8_t dir = headingOfDir(dx == 0 ? (dy > 0 ? 2 : 6) : (dy == 0 ? (dx > 0 ? 0 : 4) : 1));
            const UnitId id = spawnAt(g, type, 10, 10, dir);
            const int t = arrival(g, id, 10 + 3 * dx, 10 + 3 * dy);
            return t < 0 ? -1 : (t - 1) / 3;
        };
        check(perTile(CellType::FastPassible1, 0, 1, 0) == 24 && perTile(CellType::MediumPassible1, 0, 1, 0) == 30 &&
                  perTile(CellType::SlowPassible1, 0, 1, 0) == 39 && perTile(CellType::DozedArea, 0, 1, 0) == 18,
              "move: Lynx straight 24 fast, 30 medium1, 39 slow1, 18 dozed ticks per tile");
        check(perTile(CellType::MediumPassible2, 0, 1, 0) == 33 && perTile(CellType::SlowPassible2, 0, 1, 0) == 36,
              "move: Lynx straight 33 medium2, 36 slow2");
        check(perTile(CellType::FastPassible1, 0, 1, 1) == 32 && perTile(CellType::MediumPassible1, 0, 1, 1) == 40 &&
                  perTile(CellType::SlowPassible1, 0, 1, 1) == 52,
              "move: Lynx diagonal 32 fast, 40 medium1, 52 slow1");
        check(perTile(CellType::FastPassible1, 1, 1, 0) == 40 && perTile(CellType::FastPassible1, 1, 1, 1) == 56,
              "move: Tiger 40 straight, 56 diagonal on fast ground");
        {
            // Mixed ground: fast -> medium1 27, medium1 -> slow1 34, slow1 -> fast 31.
            Game g = makeGame(CellType::FastPassible1);
            g.map().at(11, 10).cellType = CellType::MediumPassible1;
            g.map().at(12, 10).cellType = CellType::SlowPassible1;
            const UnitId id = spawnAt(g, 0, 10, 10, Heading::East);
            check(arrival(g, id, 13, 10) == 1 + 27 + 34 + 31, "move: mixed steps 27, 34, 31 (fast/medium1/slow1)");
        }
        {
            // A 45-degree corner costs a Lynx 2 ticks; the step starts the tick after.
            Game g = makeGame(CellType::FastPassible1);
            const UnitId id = spawnAt(g, 0, 10, 10, Heading::East);
            check(arrival(g, id, 11, 11) == 1 + 2 + 32, "turn: a 45-degree corner costs 2 ticks at rate 16");
            Game h = makeGame(CellType::FastPassible1);
            const UnitId b = spawnAt(h, 0, 10, 10, Heading::East);
            h.queue(makeMoveCommand(0, 0, b, 5, 10));
            h.step(); h.step();
            check(h.world().get(b)->rotation == 240, "turn: an exact half turn goes counter-clockwise");
        }
        {
            // Velocity floor(2^21 / T) + 1 per axis: 87382 for a 24-tick step, landing on the centre.
            check(stepVelocity(24) == 87382 && stepTicks(3, 4096, 4096, false) == 24 && stepTicks(3, 4096, 4096, true) == 32,
                  "move: step ticks and velocity formulas");
            Game g = makeGame(CellType::FastPassible1);
            const UnitId id = spawnAt(g, 0, 10, 10, Heading::East);
            g.queue(makeMoveCommand(0, 0, id, 11, 10));
            g.step(); g.step(); // order taken, first moving tick
            const int afterOne = g.world().get(id)->px - tileCentrePx(10);
            for (int k = 0; k < 23; ++k) g.step();
            check(afterOne == 1 && g.world().get(id)->px == tileCentrePx(11) && g.world().get(id)->stepTicks == 0,
                  "move: 1.33 px a tick, exactly on the next centre after 24 ticks");
        }
        {
            // The route spreads diagonals: 10 east by 4 south is E SE E SE E E SE E SE E.
            Game g = makeGame(CellType::FastPassible1);
            const UnitId id = spawnAt(g, 0, 5, 5, Heading::East);
            g.queue(makeMoveCommand(0, 0, id, 15, 9));
            std::string route;
            int lastX = 5, lastY = 5;
            for (int k = 0; k < 2000 && (k == 0 || g.world().get(id)->command == Opcode::ctMoMove); ++k) {
                g.step(); // the first step takes the queued order
                const Unit* u = g.world().get(id);
                if (u->stepTicks > 0 && (u->destX != lastX || u->destY != lastY)) {
                    route += (u->destY != lastY) ? "SE " : "E ";
                    lastX = u->destX; lastY = u->destY;
                }
            }
            if (route != "E SE E SE E E SE E SE E ") std::printf("  info  route was: %s\n", route.c_str());
            check(route == "E SE E SE E E SE E SE E ", "path: diagonals spread through the route (octant walk)");
        }
        {
            // A held tile: the mover waits on its centre; when it frees, it goes on.
            Game g = makeGame(CellType::FastPassible1);
            const UnitId a = spawnAt(g, 0, 5, 5, Heading::East);
            const UnitId b = spawnAt(g, 0, 6, 5, Heading::South);
            g.queue(makeMoveCommand(0, 0, a, 8, 5));
            for (int k = 0; k < 50; ++k) g.step();
            const bool waited = g.world().get(a)->px == tileCentrePx(5) && g.world().get(a)->command == Opcode::ctMoMove &&
                                g.world().get(a)->rotation == Heading::East;
            g.queue(makeMoveCommand(g.tick(), 0, b, 6, 8));
            for (int k = 0; k < 200; ++k) g.step();
            check(waited && g.world().get(a)->px == tileCentrePx(8), "move: waits for a held tile, then drives on");
        }
        {
            // A blocked tile on the way is driven round, and the order ends on the goal.
            Game g = makeGame(CellType::FastPassible1);
            g.map().at(7, 5).cellType = CellType::Impassible1;
            const UnitId a = spawnAt(g, 0, 5, 5, Heading::East);
            g.queue(makeMoveCommand(0, 0, a, 9, 5));
            for (int k = 0; k < 400; ++k) g.step();
            check(g.world().get(a)->px == tileCentrePx(9) && g.world().get(a)->py == tileCentrePx(5) &&
                      g.world().get(a)->command == 0,
                  "move: drives round impassable ground to the goal");
        }
        {
            // A new order or a Stop mid-step doesn't interrupt the step.
            Game g = makeGame(CellType::FastPassible1);
            const UnitId a = spawnAt(g, 0, 5, 5, Heading::East);
            g.queue(makeMoveCommand(0, 0, a, 9, 5));
            for (int k = 0; k < 11; ++k) g.step(); // 10 ticks into the first step
            g.queue(makeStopCommand(g.tick(), 0, a));
            for (int k = 0; k < 100; ++k) g.step();
            check(g.world().get(a)->px == tileCentrePx(6) && g.world().get(a)->command == 0,
                  "move: Stop lets the step in progress finish");
            Game h = makeGame(CellType::FastPassible1);
            const UnitId b = spawnAt(h, 0, 5, 5, Heading::East);
            h.queue(makeMoveCommand(0, 0, b, 9, 5));
            for (int k = 0; k < 11; ++k) h.step();
            h.queue(makeMoveCommand(h.tick(), 0, b, 6, 2));
            bool reachedCentre = false;
            for (int k = 0; k < 400; ++k) {
                h.step();
                const Unit* u = h.world().get(b);
                if (u->px == tileCentrePx(6) && u->py == tileCentrePx(5)) reachedCentre = true;
            }
            check(reachedCentre && h.world().get(b)->px == tileCentrePx(6) && h.world().get(b)->py == tileCentrePx(2),
                  "move: a new order mid-step finishes the step, then follows the new route");
        }
        {
            // Save mid-step; the restored game drives on identically.
            Game g = makeGame(CellType::SlowPassible1);
            const UnitId a = spawnAt(g, 0, 5, 5, Heading::East);
            g.queue(makeMoveCommand(0, 0, a, 20, 12));
            for (int k = 0; k < 57; ++k) g.step();
            Game r = makeGame(CellType::SlowPassible1);
            const bool ok = r.deserialize(g.serialize());
            for (int k = 0; k < 300; ++k) { g.step(); r.step(); }
            check(ok && r.checksum() == g.checksum() && r.world().get(a)->px == g.world().get(a)->px,
                  "move: save/restore mid-step continues identically");
        }
        {
            Game g = makeGame(CellType::FastPassible1);
            Unit b{}; b.kind = UnitKind::Building; b.px = 336; b.py = 336;
            const UnitId s = g.world().spawn(b);
            check(!applyCommand(g.world(), makeMoveCommand(0, 0, s, 1, 1)), "move: a structure doesn't take a Move");
        }
    }

    // 23. Coarse path planner (pathfind.h; PlanCoarsePath 0x0043F6C0). The wall
    //     cases are op2-rs's measured ones (FINDINGS "Paths"): the unit drives
    //     straight to the wall, follows its face and takes the shorter end, the
    //     south one when both are equal; it doesn't route round slow ground.
    {
        using namespace op2::sim;
        check(direction8(10, 4) == 0 && direction8(9, 4) == 1 && direction8(1, -1) == 7 && direction8(-1, 1) == 3 &&
                  direction8(0, -3) == 6 && direction8(-5, 0) == 4 && direction8(1, 3) == 2 && direction8(-1, -3) == 6,
              "path: octant picker by integer slope (tan 22.5 / 67.5 x 256)");
        {
            std::uint8_t a[2] = {0, 2}, b[2] = {0, 4}, c[2] = {1, 4}, d[2] = {0, 1}, e[3] = {0, 0, 4};
            check(collapseTurns(a, 2) == 1 && a[0] == 1 && collapseTurns(b, 2) == 0 && collapseTurns(c, 2) == 1 &&
                      c[0] == 2 && collapseTurns(d, 2) == 2 && collapseTurns(e, 3) == 1 && e[0] == 0,
                  "path: turn collapse (E,S -> SE; E,W cancel; SE,W -> S; 45 degrees kept)");
        }
        // A 40x40 field with a vertical wall at x = 10 over rows y0..y1.
        auto field = [](int y0, int y1) {
            std::vector<std::uint8_t> blocked(40 * 40, 0);
            for (int y = y0; y <= y1; ++y) blocked[y * 40 + 10] = 1;
            return blocked;
        };
        auto plan = [](const std::vector<std::uint8_t>& blocked, int sx, int sy, int gx, int gy) {
            return planCoarsePath(sx, sy, gx, gy, ClipRect{0, 0, 39, 39}, [&](int x, int y) {
                return x >= 0 && y >= 0 && x < 40 && y < 40 && !blocked[y * 40 + x];
            });
        };
        // Walk a plan: the tiles it visits.
        auto walk = [](int x, int y, const CoarsePlan& p) {
            std::vector<std::pair<int, int>> tiles{{x, y}};
            for (std::uint8_t s : p.steps) { x += kDirSteps[s].dx; y += kDirSteps[s].dy; tiles.push_back({x, y}); }
            return tiles;
        };
        auto minMaxY = [](const std::vector<std::pair<int, int>>& t) {
            int lo = 99, hi = -1;
            for (auto [x, y] : t) { lo = std::min(lo, y); hi = std::max(hi, y); }
            return std::pair<int, int>{lo, hi};
        };
        {
            // Symmetric wall (rows 3..17 round row 10): the south end.
            const auto w = field(3, 17);
            const CoarsePlan p = plan(w, 5, 10, 15, 10);
            const auto t = walk(5, 10, p);
            const auto [lo, hi] = minMaxY(t);
            bool wallOk = true;
            for (auto [x, y] : t) if (x == 10 && y >= 3 && y <= 17) wallOk = false;
            check(p.reachesGoal && t.back() == std::pair<int, int>(15, 10) && wallOk && hi >= 18 && lo >= 10,
                  "path: symmetric wall: round its south end, to the goal");
            // Straight to the wall first, then down its face: the last east step and the
            // trace's first south step collapse into one SE (the plan cuts that corner).
            check(t.size() > 5 && t[3] == std::pair<int, int>(8, 10) && t[4] == std::pair<int, int>(9, 11) &&
                      t[5] == std::pair<int, int>(9, 12),
                  "path: drives straight to the wall, then along its face");
        }
        {
            // The north end is nearer (rows 6..17 round row 10): north.
            const auto w = field(6, 17);
            const CoarsePlan p = plan(w, 5, 10, 15, 10);
            const auto [lo, hi] = minMaxY(walk(5, 10, p));
            check(p.reachesGoal && lo <= 5 && hi <= 10, "path: asymmetric wall: round the nearer (north) end");
        }
        {
            // An unreachable goal (walled in): the plan stops near it, finitely.
            std::vector<std::uint8_t> box(40 * 40, 0);
            for (int k = 18; k <= 22; ++k) { box[18 * 40 + k] = box[22 * 40 + k] = box[k * 40 + 18] = box[k * 40 + 22] = 1; }
            const CoarsePlan p = planCoarsePath(5, 20, 20, 20, ClipRect{0, 0, 39, 39}, [&](int x, int y) {
                return x >= 0 && y >= 0 && x < 40 && y < 40 && !box[y * 40 + x];
            });
            const auto t = walk(5, 20, p);
            const int ex = t.back().first, ey = t.back().second;
            check(!p.reachesGoal && ex >= 16 && ex <= 24 && ey >= 16 && ey <= 24 && p.steps.size() < 600,
                  "path: an unreachable goal ends next to its walls");
        }
        // End to end: a Lynx drives round the wall at the measured speed, on slow
        // ground it goes straight through (the plan ignores terrain cost).
        {
            auto makeG = [](CellType ground) {
                Game g;
                g.map().resize(40, 40);
                for (int y = 0; y < 40; ++y)
                    for (int x = 0; x < 40; ++x) g.map().at(x, y).cellType = ground;
                UnitType lynx; lynx.kind = UnitKind::Vehicle; lynx.track = Track::Wheeled; lynx.moveSpeed = 3; lynx.turnRate = 16; lynx.maxHitPoints = 300;
                g.world().setType(0, lynx);
                return g;
            };
            Game g = makeG(CellType::FastPassible1);
            for (int y = 3; y <= 17; ++y) g.map().at(10, y).cellType = CellType::Impassible1;
            Unit u; u.kind = UnitKind::Vehicle; u.px = tileCentrePx(5); u.py = tileCentrePx(10);
            const UnitId id = g.world().spawn(u);
            g.queue(makeMoveCommand(0, 0, id, 15, 10));
            int maxY = 0;
            for (int k = 0; k < 3000 && (k == 0 || g.world().get(id)->command == Opcode::ctMoMove); ++k) {
                g.step();
                maxY = std::max(maxY, pixelToTile(g.world().get(id)->py));
            }
            const Unit* r = g.world().get(id);
            check(r->px == tileCentrePx(15) && r->py == tileCentrePx(10) && maxY >= 18 && r->command == 0,
                  "path: a Lynx drives round the wall's south end to its goal");

            Game s = makeG(CellType::FastPassible1);
            for (int x = 8; x <= 12; ++x)
                for (int y = 8; y <= 12; ++y) s.map().at(x, y).cellType = CellType::SlowPassible1;
            const UnitId b = s.world().spawn(u);
            s.queue(makeMoveCommand(0, 0, b, 15, 10));
            bool straight = true;
            for (int k = 0; k < 2000 && (k == 0 || s.world().get(b)->command == Opcode::ctMoMove); ++k) {
                s.step();
                if (s.world().get(b)->py != tileCentrePx(10)) straight = false;
            }
            check(straight && s.world().get(b)->px == tileCentrePx(15), "path: drives straight through slow ground");
        }
    }

    // 24. Combat (combat.h). Measured references are op2-rs's (FINDINGS "Combat",
    //     notes/findings-combat.md): laser hits on a still Lynx in cmb_duel_laser
    //     did 32, 35, 28, 35, 32, 30.
    {
        using namespace op2::sim;
        check(bearing(1, 0) == 0 && bearing(0, 1) == 64 && bearing(-1, 0) == 128 && bearing(0, -1) == 192 &&
                  bearing(1, 1) == 32 && bearing(-1, -1) == 160 && bearing(96, 32) == 13 && bearing(-96, -32) == 142 &&
                  bearing(10, 1) == 4 && bearing(10, -1) == 252,
              "combat: bearing truncates toward zero (east double-width, 45s exact)");
        check(cos256(0) == 64 && cos256(64) == 0 && sin256(64) == 64 && cos256(128) == -64 && sin256(0) == 0 &&
                  cos256(32) == 45,
              "combat: the exe's trunc(64 cos) table");
        check(armorValue(0) == 256 && armorValue(1) == 180 && armorValue(5) == 40, "combat: armor class to multiplier");
        check(octagonal(10, 4) == 12 && octagonal(-4, 10) == 12, "combat: octagonal distance max + min/2");

        // A 64x64 field of fast ground with the duel's two laser Lynxes.
        auto makeDuel = [](std::uint32_t seed) {
            Game g(seed);
            g.map().resize(64, 64);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) g.map().at(x, y).cellType = CellType::FastPassible2;
            UnitType lynx; lynx.kind = UnitKind::Vehicle; lynx.track = Track::Wheeled; lynx.moveSpeed = 3;
            lynx.turnRate = 16; lynx.maxHitPoints = 300; lynx.armor = 1; lynx.sight = 7;
            g.world().setType(0, lynx);
            UnitType truck = lynx; truck.moveSpeed = 4; truck.turnRate = 4;
            g.world().setType(1, truck);
            WeaponType laser; laser.kind = static_cast<std::uint8_t>(WeaponKind::Laser); laser.radius = 16;
            laser.range = 4; laser.turretRate = 11; laser.penetration = 35; laser.reload = 35;
            g.world().setWeaponType(0, laser);
            g.world().setHumanPlayers(0x01);
            return g;
        };
        auto tank = [](Game& g, std::uint8_t owner, int x, int y, std::uint8_t dir, std::uint16_t type = 0) {
            Unit u; u.typeId = type; u.kind = UnitKind::Vehicle; u.owner = owner; u.rotation = dir;
            u.weapon = type == 0 ? 1 : 0;
            u.px = tileCentrePx(x); u.py = tileCentrePx(y);
            return g.world().spawn(u);
        };
        {
            Game g = makeDuel(1);
            const UnitId a = tank(g, 0, 30, 30, 0), b = tank(g, 1, 33, 31, 128);
            std::vector<int> hitsOnB;
            int lastB = 0, deadA = -1, deadB = -1, goneA = -1, goneB = -1, booms = 0;
            for (int t = 1; t <= 420; ++t) {
                g.step();
                const Unit* ua = g.world().get(a);
                const Unit* ub = g.world().get(b);
                if (ub && ub->damage != lastB) { hitsOnB.push_back(ub->damage - lastB); lastB = ub->damage; }
                if (ua && (ua->flags & UnitFlag::IsDead) && deadA < 0) deadA = t;
                if (ub && (ub->flags & UnitFlag::IsDead) && deadB < 0) deadB = t;
                if (!ua && goneA < 0) goneA = t;
                if (!ub && goneB < 0) goneB = t;
                g.world().forEach([&](UnitId, const Unit& o) { if (o.typeId == kExplosionType) ++booms; });
            }
            const bool sequence = hitsOnB.size() >= 6 && hitsOnB[0] == 32 && hitsOnB[1] == 35 && hitsOnB[2] == 28 &&
                                  hitsOnB[3] == 35 && hitsOnB[4] == 32 && hitsOnB[5] == 30;
            if (!sequence) {
                std::printf("  info  hits on B:");
                for (int h : hitsOnB) std::printf(" %d", h);
                std::printf("\n");
            }
            check(sequence, "combat: laser duel hits 32, 35, 28, 35, 32, 30 (op2-rs measured)");
            // Deleted 5 turns after the victim's next turn: 5 ticks after the killing
            // shot when the victim comes after the shot in the list, 6 when before.
            const bool gap = (goneA - deadA == 5 || goneA - deadA == 6) && (goneB - deadB == 5 || goneB - deadB == 6);
            check(deadA > 0 && deadB > 0 && deadA <= 340 && deadB <= 340 && gap,
                  "combat: both destroyed by tick 340; deleted 5 turns after dying");
            check(booms > 0 && g.world().liveCount() == 0, "combat: each leaves an explosion object, which then goes");
        }
        {
            // Save mid-fight (shots in the air), restore, and both games run on identically.
            Game g = makeDuel(1);
            tank(g, 0, 30, 30, 0); tank(g, 1, 33, 31, 128);
            for (int t = 0; t < 47; ++t) g.step();
            Game r = makeDuel(1);
            const bool ok = r.deserialize(g.serialize());
            for (int t = 0; t < 300; ++t) { g.step(); r.step(); }
            check(ok && r.checksum() == g.checksum(), "combat: save/restore mid-fight continues identically");
        }
        {
            // An Attack order drives an armed Lynx into range of an unarmed truck and
            // destroys it; the truck never shoots back.
            Game g = makeDuel(7);
            const UnitId a = tank(g, 0, 10, 20, 0);
            const UnitId b = tank(g, 1, 30, 20, 0, 1);
            g.queue(makeAttackCommand(0, 0, a, b));
            int closest = 99;
            for (int t = 0; t < 2000 && g.world().get(b); ++t) {
                g.step();
                const Unit* ua = g.world().get(a);
                closest = std::min(closest, 30 - pixelToTile(ua->px));
            }
            check(!g.world().get(b) && g.world().get(a)->damage == 0 && closest >= 2 && closest <= 4,
                  "combat: an Attack order chases into range and kills");
        }
        {
            // Retaliation: a Lynx with a 2-tile gun is shot from 3 tiles away (it can't
            // find its attacker; the search box is strict in x, so at exactly 4 tiles
            // a range-4 laser wouldn't find it either); hit, it attacks its attacker,
            // drives into range and hits back.
            Game g = makeDuel(3);
            WeaponType shortLaser = *g.world().weaponType(0); shortLaser.range = 2;
            g.world().setWeaponType(1, shortLaser);
            const UnitId a = tank(g, 0, 20, 20, 0);
            Unit bu; bu.typeId = 0; bu.kind = UnitKind::Vehicle; bu.owner = 1; bu.weapon = 2;
            bu.px = tileCentrePx(23); bu.py = tileCentrePx(20);
            const UnitId b = g.world().spawn(bu);
            bool retaliated = false;
            for (int t = 0; t < 400; ++t) {
                g.step();
                const Unit* ub = g.world().get(b);
                if (ub && ub->command == Opcode::ctMoAttackObj && ub->orderTarget == a) retaliated = true;
            }
            const Unit* ua = g.world().get(a);
            check(retaliated && (!ua || ua->damage > 0), "combat: a unit hit retaliates on its attacker");
        }
        {
            // Line of sight: open ground sees; a rock row, a structure footprint and a
            // wide cliff band hide; a cliff next to either end doesn't.
            Game g = makeDuel(1);
            check(hasLineOfSight(g.world(), g.map(), 10, 10, 14, 12), "los: open ground");
            g.map().at(12, 11).cellType = CellType::Impassible1;
            check(!hasLineOfSight(g.world(), g.map(), 10, 11, 14, 11), "los: Impassible 1 between hides");
            g.map().at(11, 20).cellType = CellType::CliffsHighSide;
            check(hasLineOfSight(g.world(), g.map(), 10, 20, 14, 20), "los: a cliff next to the start doesn't hide");
            for (int x = 11; x <= 13; ++x) g.map().at(x, 30).cellType = CellType::NorthCliffs;
            check(!hasLineOfSight(g.world(), g.map(), 10, 30, 14, 30), "los: a 3-tile cliff band hides the far end");
            UnitType cc; cc.kind = UnitKind::Building; cc.fpX = 2; cc.fpY = 2; cc.maxHitPoints = 1000;
            g.world().setType(3, cc);
            Unit b; b.typeId = 3; b.kind = UnitKind::Building; b.owner = 1;
            b.px = structurePx(12, 2); b.py = structurePx(40, 2);
            g.world().spawn(b);
            check(!hasLineOfSight(g.world(), g.map(), 9, 40, 15, 40), "los: a structure's footprint hides");
        }
    }

    // 25. Guard Posts and structure destruction (combat.h; op2kb COMBAT.md §1,
    //     GuardPost.ScanForTargets 0x0042CF20, Building.Explode 0x00483160;
    //     op2-rs: a linked range-4 post found a truck past 4 tiles, a Common
    //     Storage was deleted 11 ticks after its last hit).
    {
        using namespace op2::sim;
        auto makeBase = [](std::uint32_t seed) {
            Game g(seed);
            g.map().resize(64, 64);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) g.map().at(x, y).cellType = CellType::FastPassible2;
            UnitType lynx; lynx.kind = UnitKind::Vehicle; lynx.track = Track::Wheeled; lynx.moveSpeed = 3;
            lynx.turnRate = 16; lynx.maxHitPoints = 300; lynx.armor = 1;
            g.world().setType(0, lynx);
            UnitType post; post.kind = UnitKind::Building; post.maxHitPoints = 500; post.armor = 4;
            post.powerRequired = 10; post.explosionSize = 2; post.rubbleCommon = 1; post.autoTarget = true;
            g.world().setType(1, post);
            UnitType storage; storage.kind = UnitKind::Building; storage.fpX = 1; storage.fpY = 2;
            storage.maxHitPoints = 400; storage.armor = 3; storage.rubbleCommon = 1; storage.autoTarget = true;
            g.world().setType(2, storage);
            WeaponType laser; laser.kind = static_cast<std::uint8_t>(WeaponKind::Laser); laser.radius = 16;
            laser.range = 4; laser.turretRate = 11; laser.penetration = 35; laser.reload = 35;
            g.world().setWeaponType(0, laser);
            WeaponType med; med.kind = static_cast<std::uint8_t>(WeaponKind::BuildingExplosionMedium); med.radius = 64;
            med.range = 0; med.concussion = 750; med.penetration = 750;
            g.world().setWeaponType(1, med);
            g.world().setHumanPlayers(0x01);
            return g;
        };
        auto vehicle = [](Game& g, std::uint8_t owner, int x, int y, bool armed) {
            Unit u; u.typeId = 0; u.kind = UnitKind::Vehicle; u.owner = owner; u.weapon = armed ? 1 : 0;
            u.px = tileCentrePx(x); u.py = tileCentrePx(y);
            return g.world().spawn(u);
        };
        auto guardPost = [](Game& g, std::uint8_t owner, int x, int y, std::uint32_t flags) {
            Unit u; u.typeId = 1; u.kind = UnitKind::Building; u.owner = owner; u.weapon = 1; u.flags = flags;
            u.px = tileCentrePx(x); u.py = tileCentrePx(y);
            return g.world().spawn(u);
        };
        {
            // Unpowered: it picks the truck and drops it, never firing.
            Game g = makeBase(1);
            const UnitId p = guardPost(g, 1, 20, 20, 0);
            const UnitId t = vehicle(g, 0, 22, 21, false);
            bool picked = false;
            for (int k = 0; k < 300; ++k) {
                g.step();
                if (g.world().get(p)->fireTarget.valid()) picked = true;
            }
            check(picked && g.world().get(t)->damage == 0, "guard post: unpowered, it picks targets but never fires");
        }
        {
            // Powered, it destroys the truck.
            Game g = makeBase(1);
            guardPost(g, 1, 20, 20, UnitFlag::Powered);
            const UnitId t = vehicle(g, 0, 22, 21, false);
            for (int k = 0; k < 600 && g.world().get(t); ++k) g.step();
            check(!g.world().get(t), "guard post: powered, it destroys a truck in range");
        }
        {
            // A truck 4 tiles east and 3 south: outside a range-4 post's box (x strict),
            // inside a linked one's (range 5).
            auto shotAt = [&](std::uint32_t flags) {
                Game g = makeBase(1);
                guardPost(g, 1, 20, 20, flags);
                const UnitId t = vehicle(g, 0, 24, 23, false);
                for (int k = 0; k < 200; ++k) g.step();
                return g.world().get(t)->damage;
            };
            check(shotAt(UnitFlag::Powered) == 0 && shotAt(UnitFlag::Powered | UnitFlag::Linked) > 0,
                  "guard post: linked, it reaches a tile further");
        }
        {
            // Linked shots hit half as hard again (same seed, same first shot).
            auto firstHit = [&](std::uint32_t flags) {
                Game g = makeBase(1);
                guardPost(g, 1, 20, 20, flags);
                const UnitId t = vehicle(g, 0, 22, 20, false);
                for (int k = 0; k < 200 && g.world().get(t)->damage == 0; ++k) g.step();
                return g.world().get(t)->damage;
            };
            const int plain = firstHit(UnitFlag::Powered), linked = firstHit(UnitFlag::Powered | UnitFlag::Linked);
            check(plain > 0 && linked > plain && linked <= plain + plain / 2 + 2, "guard post: a linked shot does x1.5");
        }
        {
            // A storage killed by a Lynx: deleted 10 turns after it dies (10-11 ticks
            // after the killing shot), its 1x2 footprint razed, one rubble piece.
            Game g = makeBase(5);
            const UnitId a = vehicle(g, 0, 20, 20, true);
            Unit s; s.typeId = 2; s.kind = UnitKind::Building; s.owner = 1;
            s.px = structurePx(23, 1); s.py = structurePx(20, 2);
            const UnitId sid = g.world().spawn(s);
            g.queue(makeAttackCommand(0, 0, a, sid));
            int died = -1, gone = -1;
            for (int k = 1; k < 2000 && gone < 0; ++k) {
                g.step();
                const Unit* su = g.world().get(sid);
                if (su && (su->flags & UnitFlag::IsDead) && died < 0) died = k;
                if (!su) gone = k;
            }
            int dozed = 0, rubble = 0;
            for (int y = 19; y <= 20; ++y) {
                const CellType c = g.map().at(23, y).cellType;
                dozed += c == CellType::DozedArea;
                rubble += c == CellType::Rubble;
            }
            check(died > 0 && (gone - died == 10 || gone - died == 11), "structure: deleted 10 turns after it dies");
            check(dozed == 1 && rubble == 1, "structure: footprint razed, one rubble piece placed");
            Game r = makeBase(5);
            const bool ok = r.deserialize(g.serialize());
            check(ok && r.map().at(23, 19).cellType == g.map().at(23, 19).cellType && r.checksum() == g.checksum(),
                  "structure: the razed map is saved and checksummed");
        }
        {
            // A destroyed Guard Post (Explosion Size 2) leaves a medium explosion that
            // wrecks the truck parked beside it.
            Game g = makeBase(2);
            const UnitId a = vehicle(g, 0, 20, 20, true);
            const UnitId p = guardPost(g, 1, 23, 20, 0); // unpowered: it won't shoot back
            const UnitId truck = vehicle(g, 1, 24, 20, false);
            g.queue(makeAttackCommand(0, 0, a, p));
            for (int k = 0; k < 2000 && g.world().get(p); ++k) g.step();
            for (int k = 0; k < 10; ++k) g.step();
            const Unit* tr = g.world().get(truck);
            check(!g.world().get(p) && (!tr || (tr->flags & UnitFlag::IsDead)), "structure: a Guard Post's explosion wrecks its neighbour");
        }
        {
            // The dock tile isn't the structure's (op2-rs: a rail gun did exactly 60 per
            // shot to a Common Storage, its docked half missing): a 1x2 storage with its
            // dock in column 1 holds only its top tile.
            Game g = makeBase(1);
            UnitType st = g.world().type(2); st.dockEden = 1; st.dockPlymouth = 1;
            g.world().setType(2, st);
            Unit s; s.typeId = 2; s.kind = UnitKind::Building; s.owner = 1;
            s.px = structurePx(23, 1); s.py = structurePx(20, 2);
            const UnitId sid = g.world().spawn(s);
            const Unit& su = *g.world().get(sid);
            check(g.world().holdsTile(su, 23, 19) && !g.world().holdsTile(su, 23, 20),
                  "structure: the dock tile isn't held (shots there miss it, vehicles drive on)");
        }
        {
            // Spontaneous explosions take a draw on tick 1179 for a damaged structure.
            auto rngAt1180 = [&](bool damaged) {
                Game g = makeBase(9);
                const UnitId p = guardPost(g, 1, 30, 30, 0);
                if (damaged) g.world().get(p)->damage = 100;
                for (int k = 0; k <= 1179; ++k) g.step();
                return g.prng().state().s1;
            };
            check(rngAt1180(true) != rngAt1180(false), "structure: a damaged one rolls Rand(500) on tick 1179");
        }
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures ? "FAILED" : "PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
