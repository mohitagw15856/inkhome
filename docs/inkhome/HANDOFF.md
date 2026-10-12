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

## Where things stand (2026-10-12, evening)

Published pre-releases up to 1.6.5.7 (text darkness, InkQuest). All 20
features the owner asked for are built (see PLAN.md section 4
for the three that were reshaped or declined).

Apps on `main`: Inbox, Reading stats, Year in books, Achievements, Notes,
Focus timer, Habits (Habitink), Flashcards (Inkcards), Chess puzzles
(ChessInk), Recipes (InkRecipes), Verse (InkVerse), StoryScapes, DailyDrop,
InkQuest, If Found, Battery health, Motion sensor, Switch firmware.

Sleep modes appended after upstream's: Custom + stats (8), Verse (9),
Cover + stats (10), Flashcard (11); overlays: owner strip, habits strip;
Custom also shows stock .xth pictures.

Not verified on hardware. The owner flashes and tests; never claim anything
works on the device. Checklist: `docs/inkhome/TESTING.md`.

## Next, in order

1. PocketWiki port, then ask the owner before InkWriter (needs BLE, 40 to
   60 KB RAM). Flash use is 89.7% of the slot, so check sizes first.

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
- No cmake on the Mac: `uvx cmake ...` works in its place.
- Inkkit's byte streams are shared in `src/apps/inkkit/` (`ByteStream.h`,
  `HalFileReader.h`); include those rather than copying them into a port.
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
