# Divergences from Outpost 2

This remake aims to reproduce Outpost 2 faithfully, but in a few deliberate places we
**diverge** — usually to fix an inconsistency or a behaviour that doesn't make sense. Every
divergence is intentional and recorded here: the original behaviour, ours, and why. (Bugs /
not-yet-implemented gaps are NOT divergences — those live in STATUS/CHANGES.)

---

## D1 — GuardPost stops animating when Idle or Disabled

**OP2 behaviour (inconsistent):**
- A GuardPost that is **Idle** (the player switched it off) **keeps** animating — its turret
  still idle-wanders, even though the post is offline.
- A GuardPost **Disabled by EMP** **stops** animating (correct).
- A GuardPost **Disabled by lack of power** **keeps** animating.

So OP2 animates an off/idle GuardPost and a power-disabled one, but freezes an EMP'd one — an
inconsistent mix.

**Our behaviour:** a GuardPost **stops** animating whenever it is **Idle** or **Disabled** for
**any** reason (player-idle, EMP, no power, no workers, damage).

**Why:** an offline / idle / disabled building isn't operating, so it shouldn't show activity.
We make it uniform with how main structures already behave — *not operating ⇒ no animation* —
rather than reproduce OP2's special-case mix.

**Where:** `TilesetView::advanceUnitTurrets` gates the GuardPost turret wander on
`active == (!idle && !disabled)` — the same rule that gates ambient animation on every other
structure (`advanceAnimation`).

---

## D2 — Skinnable HUD that supports demo *and* retail interface graphics

**OP2 behaviour:** each build ships **one** fixed HUD, built from bitmap pieces baked for the
original's fixed screen layout — the demo and retail interfaces are different art and you only
ever get the one that came with your copy.

**Our behaviour:** the HUD is a **skinnable layer**. By default we draw our own resolution-flexible
frame; on top of it we can draw the **real OP2 interface bitmaps** from a user-selected skin
directory (`[UI] SkinDir`), and that skin can be **either the demo or the retail** interface art.
Each piece's `#141414` background is colour-keyed to transparent so the masked areas never block
the game view.

**Why:** lets the remake look like the original on a modern flexible layout, support both the demo
and retail interface graphics interchangeably, and keep copyrighted HUD art **out of the binary**
(the skin points at the user's own files). See `docs/INTERFACE_SKINS.md`.

**Where:** `render/uiimageprovider.{h,cpp}` (load + colour-key, `image://ui/<name>`); registered in
`main.cpp`; drawn in `Main.qml` (first piece: `Bitmap_170`, the bottom-right corner). Skin dir from
`Config::uiSkinDir` (`[UI] SkinDir`).
