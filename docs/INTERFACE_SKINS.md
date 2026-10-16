# OP2 interface (HUD) bitmap skins

Outpost 2 builds its HUD frame from **bitmap pieces** rather than drawing it
procedurally. The remake supports loading those real interface graphics — from
both the **demo** and **retail** versions — and drawing them over our layout, so
the HUD can look like the original instead of our hand-drawn placeholder frame.

## How it works

- **Source:** an interface "skin" is just a directory of `Bitmap_*.bmp` files.
  Set it with `[UI] SkinDir` in `outpost2.ini`. It points at **the user's own
  files** (e.g. the repo's `ui/demo`, or extracted retail UI bitmaps) — nothing is
  baked into the binary, so no copyrighted HUD art ships in shared builds (same
  rule as `op2_art`). Empty `SkinDir` → the hand-drawn frame is used alone.
- **On/off:** `[UI] UseSkin` (default `1`) is the master toggle. `0` ignores the
  HUD bitmaps and uses the original hand-drawn frame (the look from before the
  interface bitmaps were imported), without having to clear `SkinDir`. In QML the
  pieces are gated on `root.hudSkin = config.useSkin && config.uiSkinDir !== ""`.
- **Transparency / masking:** each piece is an 8-bit indexed bitmap whose
  background is a single key colour — **palette index 0 = RGB(20,20,20) / #141414**.
  `UiImageProvider` (`render/uiimageprovider.{h,cpp}`) loads the bitmap, converts to
  ARGB32, and sets every key-coloured pixel to fully transparent. The grey beveled
  frame and any coloured content (e.g. the green status-line edge) stay opaque; the
  masked interior lets the game view / sidebar show through, so the HUD piece frames
  the screen without blocking it.
- **QML:** `Image { source: "image://ui/<name>" }` where `<name>` is the file stem
  (no `.bmp`). Registered on the engine in `main.cpp` as the `ui` provider.
- **Shared assets / customisable buttons:** the `ui` provider also takes a
  skin-INDEPENDENT **common dir** (`[UI] AssetsDir`, default the repo `ui/`),
  searched *after* the skin dir, and it accepts any extension (`.png`/`.bmp`). This
  is where replaceable button graphics live. The long OP2 button (art group 50,
  175×25, frame 0 normal / 1 pressed) was exported to `ui/group50-0.png` / `-1.png`
  and is loaded as `image://ui/group50-<f>.png` (still colour-keyed) — used by the
  Game-menu buttons and the selection panel's IDLE/ACTIVATE button. Drop a
  replacement PNG in `ui/` to customise a button (no rebuild; a skin can override by
  name). (`group51-*` = the 80×16 short button, exported but not yet wired.)
- **Object category icons** (`ui/tech/Tech_*.png`, 45×45) are NOT colour-keyed —
  they have their own dark backgrounds and are drawn at native size via a `file://`
  URL (so they're customisable too). See `docs/SELECTION_PANEL.md`.

## Pieces mapped so far

| File         | Size   | Behaviour      | Position            | Notes                                   |
|--------------|--------|----------------|---------------------|-----------------------------------------|
| `Bitmap_170` | 468×48 | fixed-corner   | bottom-right        | bottom border under the message/status area + sidebar bottom-right; green line = status-bar content edge |
| `Bitmap_169` | —      | fixed-corner   | bottom-left         | bottom-left corner; wired in QML, renders once the file is added to the skin dir |

(More pieces — top bar, left/right borders, minimap surround, command-bar
background, etc. — to be added as they're identified. `Bitmap_169` is in `Main.qml`
but the file isn't in the skin folders yet, so it currently draws nothing.)

## Layout model (decided): stretchable borders

OP2 itself resizes its HUD to any window size by **stretching some pieces** — this
is how both retail and the demo work. We follow the same model, which is
resolution-independent:

- **Corner pieces** — fixed size, anchored to each window corner.
- **Edge / border pieces** — stretched along their axis (the bottom border between
  the corners stretches horizontally; side borders stretch vertically) to fill the
  current window width/height.
- **Fixed blocks** — the sidebar body, minimap surround, and command-bar strip are
  drawn at native size, anchored to the right.
- **Game view** — fills the remaining middle.

So each piece is tagged with how it behaves (fixed-corner / stretch-x / stretch-y /
fixed-anchored), not pinned to one reference resolution. Demo vs retail are the same
sizes, so a single layout drives both — the user just points `SkinDir` at the skin
they want. The bottom-right corner (`Bitmap_170`) is the first piece in place.
