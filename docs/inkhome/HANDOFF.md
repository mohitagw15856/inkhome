# Inkhome handoff: what to do when the owner says "continue"

Read this first, then carry on with "Next" below without asking, one feature
per branch, in the order listed. The owner (Mohit) has asked for all of the
features to be built; stop only when they say so.

## What Inkhome is

An X3-first fork of CrossPoint Reader 1.6.5 for the Xteink X3 (and X4): native
apps, reading stats, sleep screens, and a web flasher that installs beside the
stock firmware. Read `README.md`, `SCOPE.md` and `PLAN.md` for the rules
(British English, no em dashes, no hard-coded screen sizes, RAM budget, keep
upstream files to small marked hunks).

## Where things stand (2026-10-12)

Published pre-releases: 1.6.5.1, 1.6.5.2, 1.6.5.3. `main` is ahead of 1.6.5.3.

Done on `main` (each was its own `feat/...` branch):

- Apps framework, If Found, Switch firmware, no-SD menu, slot guard, web flasher
- Recipes (InkRecipes), Verse (InkVerse), Flashcards (Inkcards), Habits (Habitink)
- Reading stats (vCodex format), Year in books, Achievements (vCodex format)
- Focus timer, break reminder, quiet hours, time left in chapter
- Sleep screens: Custom + stats (8), Verse (9), Cover + stats (10),
  Flashcard (11), stock .xth pictures, owner strip, habits strip
- Battery health, Motion sensor screen, experimental shake to bookmark and
  face down to sleep (off by default)

Not verified on hardware. The owner flashes and tests; never claim anything
works on the device. Checklist: `docs/inkhome/TESTING.md` (add a section per
feature).

## Next, in order

1. Flashcard sleep mode (merged, built): add a simulator shot
   (`sim-sd-...` settings with `"sleepScreen":11`), a TESTING.md section and a
   README line, then release **1.6.5.4** (see Releasing).
2. **ChessInk** (`../chessink` in the owner's workspace; on GitHub
   `mohitagw15856/chessink`): puzzles. Copy `src/core/` (chess, xiangqi,
   puzzle) unchanged, rewrite the board view with GfxRenderer primitives,
   puzzles from `/apps/chessink/` and legacy `/chessink/puzzles`.
3. **StoryScapes** (`mohitagw15856/storyscapes`): `.ssp` illustrated packs;
   stream image rows to the renderer, centre on the panel.
4. **Highlights to notes** (#10): when a bookmark is added in the reader,
   append the page's snippet to `/notes/<book title>.md`. Hook through the
   bookmark path without editing the reader if possible (see how shake to
   bookmark injects `HomeButtonAction::Bookmark`).
5. **DailyDrop** (`mohitagw15856/dailydrop`, needs Wi-Fi): fetch once on the
   first wake after 06:00 using CrossPoint's saved Wi-Fi credentials, store the
   `.drop` on the card, show it in an app and as a sleep mode. Never keep
   Wi-Fi on in the background.
6. **Send to reader** (#19): the reader already has File Transfer (web
   upload). Add an "Inbox" view in Library or Apps listing files uploaded in
   the last day; do not add a new network service.
7. **Auto-rotate** (#3): decided against for the reader (re-layout cost and
   accidental rotations). Document that in PLAN.md rather than building it,
   unless the owner asks again.

After each: build, host tests, simulator shots, docs (README, CHANGELOG,
TESTING, SIZES), merge with `--no-ff`, push. Release after every two or
three features.

## How to work in this repo

- **Branches:** `feat/<area>-<thing>`, merged into `main` with `--no-ff`.
- **Commits:** authored as `Mohit <mohit15856@gmail.com>`. The repo's
  CLAUDE.md forbids assistant co-author trailers; credit original human
  authors (e.g. `Co-Authored-By: franssjz <franssjz7@gmail.com>` for vCodex
  ports).
- **New app:** `src/apps/<id>/` with a pure `core/` (host tested in
  `test/inkhome/`), a descriptor registered in `src/apps/AppRegistry.cpp`, and
  English strings appended to `lib/I18n/translations/english.yaml` (that file
  allows no comments). Guide: `docs/inkhome/APPS.md`.
- **Sleep modes:** append to `SLEEP_SCREEN_MODE` in `CrossPointSettings.h`,
  never renumber; add the label in `SettingsList.h` and a case in
  `SleepActivity.cpp`. Overlays go through `src/inkhome/SleepOverlays.*`.
- **Background features:** hang off `inkhome::tick()` / `beforeSleep()` in
  `src/inkhome/InkhomeLoop.cpp`, not new lines in `main.cpp`.
- Headers that the sleep screen includes must not pull in the UI list headers
  (PNGdec defines a `local` macro that breaks them); give sleep hooks their own
  small header, as `apps/inkverse/SleepVerse.h` does.

## Checks before every commit (CI fails otherwise)

```sh
pio run -e default                      # firmware build (first build is slow)
cmake -S test -B build/test && cmake --build build/test --target InkhomeTests && ./build/test/inkhome/InkhomeTests
node --test test/inkhome/flasher.test.mjs
git add -N .                            # so the formatter sees new files
pip install 'clang-format==21.*'        # CI uses clang-format 21; 22 formats differently
./bin/clang-format-fix                  # with clang-format 21 first on PATH
scripts/inkhome/cppcheck-like-ci.sh     # approximates CI's cppcheck
scripts/inkhome/screenshots.sh          # simulator shots and button smoke test (needs SDL2)
```

Notes:

- If the PlatformIO build fails with `SCons.Tool.FortranCommon`, run
  `pip install --python ~/.platformio/penv/bin/python pioarduino==6.1.19`
  (or `uv pip install ...`).
- If the simulator build reports `MissingPackageManifestError`, delete
  `$TMPDIR/inkhome-sim/inkhome/.pio/libdeps/simulator*` and rerun.
- CI's cppcheck still occasionally flags a parameter that could be
  `const GfxRenderer&`; make renderer parameters `const` when only `const`
  methods are used.
- The simulator needs SDL2 (`brew install sdl2`, or `apt install libsdl2-dev`).
  It may not be available in a web sandbox; if not, say so and skip the shots.

## Releasing

1. Bump `version = 1.6.5.N-inkhome` in `platformio.ini` (the `[crosspoint]`
   section) and add a CHANGELOG section.
2. Push `main`, wait for "CI (build)" on that commit to pass.
3. `gh release create 1.6.5.N-inkhome --prerelease --target <sha> --notes-file ...`
   Keep releases as pre-releases until the owner confirms hardware tests.
   The "Inkhome release" workflow builds the firmware, attaches it and
   redeploys the flasher at https://mohitagw15856.github.io/inkhome/.

## Safety rules the owner cares about

- The flasher must never write the slot that holds stock firmware; the
  firmware's own updaters refuse while stock is in the other slot. Keep both.
- No feature may block start-up or leave a screen without a way out (Back
  always works; no SD card is handled).
- Explain risks plainly and give a way back (Switch firmware, the flasher's
  Start stock firmware button, the owner's 16 MB backup).
