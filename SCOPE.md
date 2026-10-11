# Inkhome scope

Upstream's scope document is kept at [docs/UPSTREAM_SCOPE.md](docs/UPSTREAM_SCOPE.md).
Inkhome inherits it for the reader itself and widens it in one direction:
**on-device apps for the inkkit e-ink ecosystem.**

## In scope

- Everything upstream treats as in scope (reading, typography, library,
  memory and flash work).
- **Apps** that suit an e-ink reader with buttons: recipes, flashcards,
  habits, puzzles, interactive fiction, reference archives, quotes. Each app
  is a self-contained module under `src/apps/<app>/`.
- **Reading statistics** and the tools around them (heatmap, achievements,
  sleep-screen stats), compatible with CPR-vCodex's data formats.
- **X3 hardware**: the motion sensor, the 792 x 528 panel, the fuel gauge.

## Rules every change follows

1. **X3 first, X4 safe.** Device behaviour goes through upstream's runtime
   detection. Never break the X4 build or behaviour.
2. **No hard-coded screen sizes.** Lay out from `renderer.getScreenWidth()`
   and `getScreenHeight()`. Design for 792 x 528, then check 800 x 480 and
   both landscape orientations.
3. **RAM is the budget.** No PSRAM on the C3. Allocate in `onEnter`, free in
   `onExit`, cap file sizes, stream from SD, and state the expected peak heap
   in the module's header comment.
4. **Flash is the other budget.** Report the binary size against the
   6,553,600-byte app partition on every change ([docs/inkhome/SIZES.md](docs/inkhome/SIZES.md)).
5. **Stay mergeable.** New code goes in new files (`src/apps/`,
   `src/inkhome/`, `test/inkhome/`). Edits to upstream files are small hunks
   marked with an `Inkhome:` comment where the reason is not obvious.
6. **Keep the data compatible.** `/.crosspoint/` stays upstream's layout;
   ported vCodex stores keep their JSON formats.
7. **Credit and licences.** Check the licence before copying anything, keep
   copyright notices, credit authors in the file and the commit.
8. **British English** in docs and UI strings, and no em dashes.

## Out of scope

- Anything that needs to stay connected in the background (feeds, browsers,
  sync daemons). Apps may use Wi-Fi briefly and explicitly, through
  CrossPoint's existing Wi-Fi credentials.
- Fonts or assets without an open licence.
- Sweeping refactors of upstream code.
