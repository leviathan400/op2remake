# Concepts — what everything is and why it matters

Plain-English explanations of the engine concepts behind the remake. This is the
"learn the ideas" companion to the code. Each section says **what it is**, **how
it works**, and **why a faithful remake has to get it right**.

> Throughout, "the engine" / "the original" means the 1997 `Outpost2.exe`, as
> documented in the reverse-engineering spec at `D:\Oupost2Decompile\Remake\`.

---

## Why determinism is the whole foundation

Outpost 2 is **lockstep multiplayer**: machines don't send each other snapshots
of the world — they send only the *player commands*, and every machine re-runs
the exact same simulation. For that to stay in sync, the same starting state plus
the same commands must produce a **bit-identical** result on every machine, every
time. The same property gives you free replays and reliable save/load.

This is why the simulation core (`op2sim`) is walled off from Qt, floating-point
math, and the system clock. Anything non-reproducible that leaks into the sim
breaks lockstep. Two rules fall out of this:

1. **One RNG, called in a fixed order.** (Below.)
2. **Time = a tick counter, not the wall clock.** (Below.)

---

## 1. The PRNG (dual-LFSR random number generator)

**What it is.** The single source of every "random" decision in the
simulation — vortex steering, meteor targets, damage rolls, scientist training
outcomes, mining yields, and so on. File: `sim/include/op2/sim/prng.h`.

**LFSR in one paragraph.** A *Linear Feedback Shift Register* is a cheap way to
make random-looking bits. You keep a register of bits; each step you shift them
all over by one and compute the new incoming bit as the XOR of a few fixed
"tap" positions. With good taps it visits almost every possible value before
repeating — looks random, but is perfectly reproducible.

**What OP2 actually does.** It runs **two** 32-bit LFSRs at once (stronger than
one):

- `state0` shifts **left**, feedback bit goes into the low bit (LSB);
- `state1` shifts **right**, feedback bit goes into the high bit (MSB);
- both tap **bits 13 and 16** (the mask `0x12000`);
- the output number is `state0 XOR state1` — two out-of-phase generators mixed.

The feedback bit is the parity (XOR) of the tapped bits. The original computes it
as `popcount(...) & 1`; we use a portable bit-fold parity that gives the
identical result on every compiler (a popcount intrinsic could vary — and
"varies by platform" is exactly what we can't allow).

**Two fidelity details we copied exactly:**
- **Seeding** spreads the 32-bit seed across the two registers (high half →
  `state0`, low half → `state1`) so they start out of phase, and rewrites a seed
  of `0` to `0x10000001`. An all-zero LFSR is "dead" — XOR of zeros stays zero
  forever.
- The whole RNG is **just 8 bytes** of state. That's why save/load and network
  resync are trivial: snapshot two 32-bit words (`Prng::State`).

**Why it must be exact.** Every machine draws from this one stream in the same
order. Change the algorithm — or even the call order — and two machines diverge:
desync in multiplayer, broken replays. This is the most order-sensitive code in
the engine.

---

## 2. The Lemire range map

**What it is.** The bridge from "a raw 32-bit random number" to "a number in
`[0, n)`" — e.g. `getRand(100)` for a percentage roll.

**The trap it avoids.** The obvious approach, `rand32() % n`, is subtly biased:
unless `n` divides 2³² evenly, some results come up slightly more often than
others (*modulo bias*). A human never notices, but it changes the exact sequence
of outcomes — which would desync us from the original's behavior.

**What OP2 does instead** (Lemire's multiplicative map):

```
result = (uint64_t(rand32) * n) >> 32
```

Read it as: treat `rand32` as "how far through the range `[0, 2³²)` are we,"
multiply by `n`, keep the integer part. It lands uniformly in `[0, n)`, uses no
division, and has far less bias than `%`. Notably modern for 1996 code.

**Why it must be exact.** For any `n` that isn't a power of two, `% n` and the
Lemire map return *different* values for the same `rand32`. Substituting `%`
would silently break compatibility. We also copied one quirk: the engine
**always advances the RNG**, even for `getRand(0)` (the multiply just yields 0),
so the stream stays identical no matter the bound.

---

## 3. The tick clock and per-mark gating

**What it is.** The simulation's heartbeat: a single counter, `tick`, that drives
*all* time-based game logic. File: `sim/include/op2/sim/clock.h`.

**The key rule.** The sim reads only this tick — never the real clock. Wall-clock
time and on-screen frame rate are handled separately, up in the Qt/render layer.
That separation is what makes the sim reproducible: the world is a pure function
of `(starting state + the stream of commands)`. Render as fast or slow as you
like; the simulation outcome is unchanged.

**Ticks vs. "marks."** OP2 has a coarser unit called a *mark*: **1 mark = 4
ticks**. So the game-facing time is `tick >> 2` (this is exactly what the
original's `TethysGame::Time()` returns).

**Per-mark gating — the easy thing to get wrong.** Mission triggers do **not**
run every tick. In the original, the trigger pass starts with, in effect:

```c
if ((tick & 3) != 3) return;
```

So triggers evaluate **once per mark**, on the *last* tick of each group of four
— ticks 3, 7, 11, 15, … That's what `Clock::isMarkBoundary()` represents (and we
verify it at compile time). If a remake fired triggers every tick instead, it
would run mission logic **4× too often**: objectives completing too fast, timers
off by 4×, waves spawning wrong. A one-line gate with outsized consequences —
which is why it's a first-class method.

---

## 4. The state checksum

**What it is.** A short number (a 32-bit hash) computed from *all* sim-relevant
state. If two machines have simulated identically, their checksums match; if they
disagree, the simulations have drifted apart. File:
`sim/include/op2/sim/checksum.h`.

**Why it exists.** Two jobs, both about catching determinism bugs:
- **Lockstep multiplayer:** peers periodically swap checksums. A mismatch is a
  *desync* — caught immediately instead of as mysterious "we're seeing different
  things" reports.
- **Replay regression:** re-run a recorded command stream and the per-tick
  checksums must reproduce exactly. Any divergence pinpoints the tick where a
  non-deterministic code path crept in.

**How it works.** It's a cheap, order-sensitive hash. The engine's kernel (which
we ported bit-for-bit) does one step per 32-bit word: XOR the word into a running
accumulator, then rotate the accumulator right by one bit. Order-sensitive means
hashing `(1, 2)` differs from `(2, 1)` — exactly what you want for catching "the
units updated in a different order" bugs. A result of 0 is bumped to 1, so "0"
can mean "no checksum computed yet."

We feed state in through a small `Checksummer` (add each field, then read the
value). Right now `checksumCoreState()` folds in the tick and the PRNG state; it
grows to cover units, players, and the map as we build them.

**A deliberate scope choice.** We did *not* try to match the original game's full
checksum *value* bit-for-bit. Doing so would mean reproducing its exact in-memory
struct byte layouts — which a clean-room clone intentionally doesn't. So we kept
the *kernel* faithful (leaving the door open to original-parity later if we ever
want it) but our checksum only needs to be **internally consistent**: identical
runs of *our* engine produce identical checksums. That's all divergence detection
and replay regression require.

---

## 5. The headless tick loop & cmdlog replay

**What it is.** The simulation's outer loop with *nothing attached to a screen*:
it advances ticks, applies the player commands due on each tick, steps the sim,
and samples checksums — and it can do all of that driven by a *recording*. Files:
`sim/include/op2/sim/cmdlog.h` and `replay.h`.

**Cmdlog (command log).** Because the game is deterministic and lockstep only
exchanges *commands* (not world state), a recording of "which command happened on
which tick" is enough to reproduce an entire game. We store these as a portable,
hand-editable text format (JSONL — one command per line): tick, opcode, player,
and payload bytes. A `CommandPacket` is one such record.

**Replay (the harness).** `Replay` is the headless loop. You give it a list of
commands and three hooks:
- `onCommand` — called for each command, on its tick (your sim applies it);
- `onTick` — called once per tick (your sim steps forward);
- `checksumFn` — called periodically (returns your current state checksum).

It runs ticks in order and hands back the checksum history. Crucially, the
harness *doesn't simulate anything itself* — it's pure sequencing. The actual
simulation plugs in through the hooks. (The real game systems aren't built yet,
so the self-test wires in a tiny stand-in sim.)

**Why this is the backbone.** Two runs of the same recording with the same seed
must produce the *same checksum history*. That's the determinism proof. And when
they *don't* match, `Replay::diff()` reports the exact tick where they first
diverge — turning "multiplayer desynced somewhere" into "tick 4173, here." Every
future system (units, economy, combat) gets regression-tested for free: replay a
fixture before and after a change; if the checksums shift, you changed behavior.

The long game: record a real session from the original `Outpost2.exe` (it has a
built-in `/cmdlog`), replay it on our engine, and compare checksums tick-for-tick.
Matching for thousands of ticks of varied play is the strongest possible
statement that the simulation is correct.

---

## 6. The command layer (how the UI is allowed to change the world)

**What it is.** The apply side of the cmdlog story: `applyCommand(world, packet)`
in `sim/include/op2/sim/command.h` decodes a `CommandPacket` and performs the one
World mutation it describes. The UI never writes game state itself — clicking
IDLE on a building *builds a packet* (a real OP2 `ctMoIdle` opcode, carrying the
target's handle) and hands it to the sim, exactly as a network peer's packet or
a replayed recording would arrive.

**Why it matters.** Lockstep multiplayer and replays only work if commands are
the *complete* set of inputs — if the UI could also poke a flag directly, that
poke would never reach the other machines (or the recording) and the sims would
drift. Routing every mutation through one door makes "record it", "send it to
peers", and "apply it" the same code path. A rejected packet (stale handle, bad
payload, unknown opcode) changes *nothing*, and the rejection logic lives
sim-side so every peer rejects identically.

**The one exception.** Condition-Disabled (no power / no workers / EMP) is not a
player command in OP2 — the sim computes it. Until the economy sim exists, the
debug UI fakes the condition through `debugSetDisabled`, an explicit backdoor
kept *inside* the command layer so the "only this module mutates the World" rule
still holds, and is trivially deletable later.

---

## How these fit together (the loop, in spirit)

```
for each tick (driven by the headless loop / Replay):
    apply commands due this tick     // onCommand: from input, network, or a cmdlog
    clock.advance()                  // tick++
    ... advance units, combat, economy (all integer, all via the one RNG) ...
    if (clock.isMarkBoundary())      // ticks 3,7,11,...
        run the trigger pass         // mission objectives, win/lose checks
    periodically: sample the state checksum   // compare vs peers / a recording
```

Everything above the sim — input, rendering, audio, networking — feeds commands
*in* and reads state *out*, but never reaches inside to nudge a value or peek at
the wall clock. Keep that boundary clean and determinism (and therefore
multiplayer and replays) comes almost for free.

---

## Glossary

| Term | Meaning |
|---|---|
| **Tick** | The smallest unit of sim time; one step of the simulation. |
| **Mark** | 4 ticks. The game-facing time unit; `Time() = tick >> 2`. |
| **Lockstep** | Multiplayer model where all machines run the same sim from the same commands, instead of syncing snapshots. |
| **LFSR** | Linear Feedback Shift Register — the shift-and-XOR random bit generator. |
| **Lemire map** | Bias-free way to turn a 32-bit random value into `[0, n)`. |
| **Determinism** | Same inputs → byte-for-byte same outputs, every run, every machine. |
| **State checksum** | Order-sensitive hash of all sim state; used to detect desync and verify replays. |
| **Desync** | When two machines' simulations drift apart — caught by mismatched checksums. |
| **Cmdlog** | A recording of player commands (tick + opcode + payload); enough to replay a whole game. |
| **Command layer** | The single door through which anything outside the sim mutates the `World` (`applyCommand`); the UI builds packets, never writes state. |
| **Replay harness** | The headless tick loop that drives a sim from a cmdlog and samples checksums. |
| **Sim firewall** | The rule (and CMake boundary) that keeps Qt/float/clock out of `op2sim`. |
