# CLAUDE.md — working notes for AI/code assistants

A clean-room remake of Outpost 2 (1997 RTS). C++20 + Qt 6 Quick + CMake.
Read [README.md](README.md) first; **read [LEARNINGS.md](LEARNINGS.md) before touching
art/render code or the build** — it records the expensive mistakes so you don't repeat them.

## The rules that matter

1. **The sim firewall.** `Outpost2/sim/` (`op2sim`) must never depend on Qt, floating-point
   math, or any wall-clock/OS time source. Integer-only, deterministic, headless. The CMake
   target separation enforces this — don't link Qt into it, ever.
2. **The command layer.** The UI/render layer is read-only over `op2::sim::World`. Every
   gameplay mutation goes through `op2::sim::applyCommand` (`sim/include/op2/sim/command.h`)
   as a `CommandPacket`. Never write World state from the render layer. (Mission *loading*
   is setup and may spawn/setType.)
3. **Building art special cases** live in ONE place: `TilesetView::resolveBuildingArt`
   (`render/tilesetview_showcase.cpp`). Don't re-add per-building branches elsewhere.
4. **Every sim change keeps the self-tests green** and usually adds checks
   (`sim/tests/sim_selftest.cpp`).
5. **Reverse-engineering provenance in comments.** When code mirrors OP2 behavior, cite the
   EXE address / decompile doc (e.g. `Unit.DoAmbientAnimation @0x483580`) like the existing
   code does. The spec lives at `D:\Oupost2Decompile\Remake\` (start with ARCHITECTURE.md).

## Build & test (Windows, Qt 6.11 MinGW)

```powershell
# Tools (not on PATH by default):
#   C:\Qt\Tools\CMake_64\bin\cmake.exe, C:\Qt\Tools\Ninja, C:\Qt\Tools\mingw1310_64\bin
cmake --build Outpost2/build_rel              # incremental build (Ninja, Release)
ctest --test-dir Outpost2/build_rel           # op2sim_selftest + op2maps_selftest
# Run the app: needs Qt DLLs — either windeployqt or C:\Qt\6.11.1\mingw_64\bin on PATH.
# Without them the exe shows a loader dialog and never reaches main() (no log!).
# Startup log: Outpost2/build_rel/logs/op2remake-*.txt
```

Assets come from an OPU 1.4.1 install pointed to by `outpost2.ini` `[Game] Outpost2=`
(currently `D:/opu/op2remake` itself — the OPU/ subtree). `[Debug] Map=` picks the startup map.

## Doc map (keep these current when you change things)

- [STATUS.md](STATUS.md) — what works today (single source for self-test counts)
- [CHANGES.md](CHANGES.md) — dev log, newest first; add an entry for substantive work
- [ROADMAP.md](ROADMAP.md) — phased plan
- [CONCEPTS.md](CONCEPTS.md) — why determinism/PRNG/checksum/cmdlog/command-layer exist
- [DIVERGENCES.md](DIVERGENCES.md) — intentional deviations from OP2 (record any new one)
- [docs/](docs/) — per-subsystem design notes (panels, selection, skins, tech tree)
