# UI reference — OP2 selection panel (pixel review)

Reference: the OP2 **selection detail panel** (Agridome), `ui/reference/agridome_panel.png`.
**Measured** (192×215 image; our sidebar inner width ~196 — near-identical).

Font (decompile, `Review/findings/RENDERER.md`): **Arial via GDI**, non-antialiased. The non-AA
rendering is why **every text pixel measures exactly `#00ff00`** (no intermediate shades) — the whole
panel is one green.

## OP2 Agridome panel — measured

| # | Element | Text | Measured | Colour |
|---|---------|------|----------|--------|
| 1 | Title | `Agridome # 1-02` | band 10px tall, 88px wide → **Arial ~13px bold** | `#00ff00` |
| 2 | Portrait box + icon | — | 60×60 box, 45×45 icon | — |
| 3 | Armor class | `Light Armor` | **~11px** | **`#00ff00`** (NOT muted!) |
| 4 | HP value | `1000/1000` | ~11px, **above** the bar | `#00ff00` |
| 5 | HP bar | — | **~5–6px tall**, ~115px wide (full right column) | `#00ff00` on dark |
| 6 | Upgrade note | `Increased Food production` | 129px / 25 chars ≈ 5.2px/char → **Arial ~10–11px** | `#00ff00` |
| 7 | Stat label + value | `Status:` `Active` … | **~11px**, **row pitch ~11–12px** | `#00ff00` (Status state-coloured) |
| 8 | Production rows | `Production: Food` / `Production Level: 54` | ~11px | `#00ff00` |
| 9 | Storage | `In Storage: 2258 Units` | ~11px | `#00ff00` |

## Ours vs OP2 — after this pass

| Element | OP2 (measured) | Ours | Status |
|---------|----------------|------|--------|
| Title | Arial 13 bold, `#00ff00` | 13 bold, `#00ff00` | ✅ |
| Armor class | 11, **`#00ff00`** | 11, `#00ff00` | ✅ (was wrongly muted — **fixed**) |
| HP value | 11, text above bar | 11, text above bar | ✅ (**fixed** — was on a thick bar) |
| HP bar | ~6px tall | 6px | ✅ (**fixed** — was 13px) |
| Stat label/value | 11, pitch ~11–12 | 11, pitch tightened (rowSpacing 1) | ≈ (Qt font leading keeps it ~13–14; close) |
| Upgrade note | ~10–11 | *(none)* | add when tech/upgrade data exists |
| Production / Production Level / In Storage | ~11 | *(none)* | add when economy/production sim lands |

## Gaps (fields OP2 shows that we don't yet)

- **Upgrade note** ("Increased Food production" / "Increased Power output") — a per-building
  research/upgrade description. Needs the tech/research data.
- **Production** + **Production Level** — what the building outputs and its rate. Economy sim.
- **In Storage** — building's stored amount. Economy sim.

## Takeaway

Font family + rendering already match OP2 (Arial via GDI). Sizes are close; the panel is meant to
be **compact** (≤11px for body, ~13px bold title, the upgrade note ~10–11px). Finalise exact sizes
+ row pitch once `ui/reference/agridome_panel.png` is saved for measurement.
