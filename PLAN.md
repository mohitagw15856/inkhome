# Inkhome: plan

Inkhome is an X3-first fork of CrossPoint Reader that also hosts the inkkit
e-ink apps as native on-device Apps.

This file is the Phase 0 deliverable: architecture map, feature tables,
scope, roadmap, risks and open questions.

## 0. Baseline

| Item | Finding |
|---|---|
| Upstream | `crosspoint-reader/crosspoint-reader`, latest stable tag **1.6.5** (2026-09-27). The brief named v1.5.0; 1.6.0 and 1.6.5 have both shipped since. 1.6.5rc is a release candidate and was ignored. |
| vCodex | `franssjz/cpr-vcodex`, `master` at `1.6.5.1-cpr-vcodex`. Merge-base with upstream is exactly tag 1.6.5, so ports are like-for-like. |
| inkkit | Local workspace `xteink/Inkkit` plus the ten app repos beside it. |
| Toolchain | PlatformIO, pioarduino `platform-espressif32` 55.03.311, ESP32-C3, 16 MB flash. `env:default` is a single binary for X3 and X4 (device chosen at runtime). |
| Flashable output | `.pio/build/<env>/firmware.bin` (app image, written at 0x10000 or via OTA/SD update). |
| App partition | `partitions.csv`: `app0` and `app1` are 0x640000 (6,553,600 bytes) each. Size report in section 7. |

## 1. Architecture map (upstream 1.6.5)

**Activities.** `src/activities/Activity.h:16`. Hooks `onEnter`, `onExit`,
`loop`, `render(RenderLock&&)`, `requestUpdate`. Behaviour flags such as
`preventAutoSleep` and `isReaderActivity`. Sub-screens use
`startActivityForResult` and `finish()`. `ActivityManager`
(`src/activities/ActivityManager.h`) owns a stack, defers transitions to the
next loop, and renders on a dedicated FreeRTOS task guarded by `RenderLock`.
List screens derive from `UiListActivity` (FreeInkUI list with button and
touch routing).

**Home.** `src/activities/home/HomeActivity.{h,cpp}`. The menu is hard-coded
in four places (enum `HomeMenuItem`, index mapping helpers, item count,
dispatch switch plus label/icon vectors). There is no launcher or "Apps"
concept upstream. The touch-only cover-grid home (`CoverGridHomeUi`) has its
own fixed five-tab row.

**Settings.** `src/CrossPointSettings.h` (fields and enums, persisted by
index, append only) and `src/SettingsList.h` (declarative `SettingInfo`
list, grouped by category tab). JSON at `/.crosspoint/settings.json` via
`PersistableStore` (ArduinoJson 7). A new setting is a field, one
`SettingInfo` line and an i18n key; JSON and the web settings API follow
automatically.

**Rendering.** `lib/GfxRenderer/GfxRenderer.h`. Runtime
`getScreenWidth/Height` (orientation aware), text (`drawText`,
`getTextWidth`, `getLineHeight`, `wrappedText`, `truncatedText`), shapes,
icons, bitmaps, grayscale modes. `HalDisplay` refresh modes FULL, HALF,
FAST. Theme helpers through the `GUI` macro (`drawHeader`,
`drawButtonHints`, `drawPopup`, metrics).

**Input.** `src/MappedInputManager.h`. Logical buttons (Back, Confirm,
Left, Right, Up, Down, Power, PageBack, PageForward), `wasPressed`,
`wasReleased`, `isPressed`, `getHeldTime`, `mapLabels` for hint labels.
`ButtonNavigator` handles repeat navigation.

**Storage.** `lib/hal/HalStorage.h` (`Storage` macro): exists, mkdir,
`readFile`, `writeFile`, `openFileForRead/Write`, `listFiles`, `HalFile`.
`/.crosspoint/` holds settings, state, recents, Wi-Fi, OPDS, KOReader,
bookmarks, library index and per-book caches.

**i18n.** `lib/I18n/translations/*.yaml` (34 languages, English is the
reference and fallback). `scripts/gen_i18n.py` runs as a pre-build script
and generates `StrId`. Use `tr(STR_X)`.

**Device detection.** `lib/hal/HalGPIO.cpp:78`
`detectDeviceTypeWithFingerprint()`: NVS override, then NVS cache, then an
I2C fingerprint (`freeink::detectXteinkVerdict`), falling back to X4.
`HalGPIO::begin()` selects the board and the UC8279 panel for X3. Public
API: `gpio.deviceIsX3()` / `deviceIsX4()`. X3 differences: 792x528 panel,
BQ27220 gauge for charge detection, half-refresh quirks, I2C IMU.

**IMU.** Already upstream. `lib/hal/HalTiltSensor.{h,cpp}` wraps the SDK
`Imu` driver (QMI8658 or LSM6DS3TR-C). Setting `tiltPageTurn`
(Off, Normal, Inverted), shown only when the IMU is detected, polled from
`main.cpp:608`, consumed in `ReaderUtils.h`. Upstream default is Off.

**Sleep screen.** `src/activities/boot_sleep/SleepActivity.cpp`. Modes
DARK, LIGHT, CUSTOM, COVER, COVER_CUSTOM, BLANK, QUICK_RESUME,
TRANSPARENT_CUSTOM. CUSTOM (your own wallpaper) reads `/sleep.bmp`, else a
random BMP from `/.sleep` then `/sleep`.

**Tests and CI.** Host GoogleTest via CMake in `test/` (about 35 suites,
`pio run -t unit-tests`). Workflows: `ci.yml` (format, cppcheck, build
matrix, unit tests), `release.yml` (`*-gh_release` envs to release
assets), `release_candidate.yml`, `release-fonts.yml`.

## 2. vCodex feature inventory

Licence: vCodex is MIT with the same CrossPoint copyright line as upstream,
so code can be copied with the notice preserved. Credits to carry: franssjz
(vCodex), zgredex and erickosanchezj (Lyra Carousel), uxjulia / CrossInk
(highlights), the crosspet fork (text darkness idea).

**Licence block:** vCodex ships **Bookerly** glyph data
(`lib/EpdFont/builtinFonts/bookerly_*.h`). Bookerly is an Amazon
proprietary font with no licence in the repo. Inkhome will not copy it.
Optional SD fonts from vCodex releases (ChareInk, Lexend) need their
licences confirmed before we redistribute them.

| Feature | vCodex files | Upstream 1.6.5 has | X4 layout assumptions | RAM / flash | Port |
|---|---|---|---|---|---|
| Reading stats | `ReadingStatsStore`, `ReadingSessionLog`, `ReadingStats*Activity`, `JsonSettingsIO` | Nothing | Fixed card heights (76, 34 px) | About 4.2k LOC. Whole history in RAM as vectors of strings; grows with library | L |
| Heatmap | `ReadingHeatmapActivity` | Nothing | Fixed header/legend heights, grid width from screen | Small, reads stats | M |
| Reading day | `ReadingDayDetailActivity` | Nothing | Theme metrics only | Tiny | S |
| Reading profile | `ReadingProfileActivity` | Nothing | Radar section 252 px, tall portrait assumed | Small | M |
| Achievements | `AchievementsStore`, `AchievementsActivity` | Nothing | Theme metrics | 62-entry array plus unbounded path vectors | M |
| Sync Day | `SyncDayActivity`, `ManualDateActivity`, `TrustedTime`, `TimeUtils` | Partial: NTP and a timezone picker exist, no trusted-day concept | Rows only | Small | M |
| Per-book stats correction | `BookReadingAdjustmentActivity` and friends | Nothing | Fixed stepper widths | Small | M |
| Flashcards | `FlashcardsStore`, nine `Flashcard*Activity` | Nothing (Inkcards covers this in our ecosystem) | **Forces landscape**; 48 px touch band | Decks parsed whole (512 KB cap) | M to L |
| Bookmarks app / highlights | `BookmarksAppActivity`, `BookmarkStore` (binary v5) | Per-book bookmarks with a different format | List UI | 256 per book | L, format conflict |
| Sleep tools | `SleepAppActivity`, `SleepPreviewActivity`, `SleepScreenCache`, `PngSleepRenderer` | Partial (CUSTOM wallpaper, overlay) | Cache is a full framebuffer per image | PNG path about 40 KB heap | M |
| Stats sleep screens | `SleepActivity.cpp` (double upstream size) | Nothing | **Worst offender**: absolute y up to 530 and `pageHeight-578`, negative on a 480 or 528 tall panel in landscape | About 900 LOC | M, must be redesigned |
| Home/Apps shortcuts | `ShortcutRegistry`, `AppsActivity`, three settings activities, +1.5k LOC in Home | Fixed Home menu | Rows | 57 settings bytes | L |
| Lyra Carousel | `LyraCarouselTheme` | Lyra, Lyra 3 covers | Hard-coded 600/660 px cover heights | Up to 3 cover BMPs | M, needs X3 redesign |
| Text darkness | Glyph draw path, one enum | Nothing (night mode only) | None | Tiny, hot path | S to M |
| Bionic reading | `TextBlock`, `ParsedText`, section cache | "Focus reading" exists, the same idea | None | None | Skip, upstream covers it |
| Refresh mode | `readerRefreshMode`, `ScreenCleanActivity` | `refreshFrequency` only | None | Tiny | S |
| SD fonts | Extra manifest source, CJK fallback | Yes | None | Small | S (source picker only) |
| SD firmware update | Unchanged from upstream | **Yes, identical** | None | None | Nothing to do |
| If Found screen | `IfFoundActivity`, `IfFoundFile`, web page | Nothing | Centred text | Small | S |
| On-device ReadMe | Removed by vCodex in April 2026 | Nothing | n/a | n/a | S if wanted |

**Storage formats to keep for migration** (detail in `docs/inkhome/STORAGE.md`
once ported): `/.crosspoint/reading_stats.json` (`formatVersion` 6,
`readingDays`, `legacyReadingDays`, `sessionLog`, `books`),
`/.crosspoint/achievements.json` (`formatVersion` 2, positional `states`),
exports under `/exports/stats_exported` and `/exports/stats_backup_YYYY-MM-DD`.

**Sleep-mode index clash.** vCodex renumbered upstream's sleep modes (BLANK
and COVER_CUSTOM swapped, later modes shifted). Inkhome keeps upstream's
indices and **appends** new modes, so `settings.json` stays compatible with
upstream. A vCodex user's `sleepScreen` value will need a one-off mapping on
import; noted under risks.

## 3. inkkit app inventory

All ten apps share one shape: a pure, host-tested `core/` and a `device/`
layer that draws with a copy-pasted 5x7 bitmap font into the raw
framebuffer. Porting means keeping `core/` verbatim and rewriting the device
layer against `GfxRenderer`, CrossPoint fonts and `MappedInputManager`.
That also closes the ecosystem's biggest gap (6 of 6 apps asked for a real
font engine) for free.

| App | What it does | Needs | Size | Port |
|---|---|---|---|---|
| InkRecipes | Recipe list, ingredient checklist with servings scaler, cooking mode with timers | Nothing extra | About 870 LOC | **S, first** |
| InkVerse | Poem or quote standby screen | A sleep-screen hook | About 800 LOC | S |
| StoryScapes | Illustrated micro-history packs | Bitmap pages at fixed size | About 810 LOC | S |
| ChessInk | Chess and xiangqi puzzles, rating, streaks | Board drawing | About 2,250 LOC | M |
| InkQuest | Interactive fiction VM, saves | BMP covers | About 2,000 LOC | M |
| Inkcards | SM-2 flashcards | Already on GfxRenderer | About 2,000 LOC | M |
| Habitink | Habit and streak tracker | Clock adapter | About 2,650 LOC | M |
| PocketWiki | Offline encyclopedia archives | Inflate, keyboard, RAM fixes (loads every title) | Medium | M to L |
| DailyDrop | Daily digest fetched over Wi-Fi | **Wi-Fi**, reuse CrossPoint credentials | About 1,100 LOC | M |
| InkWriter | Writing with a BLE keyboard | **BLE**: CrossPoint sets `lib_ignore = BLE`; NimBLE costs 40 to 60 KB RAM | About 2,700 LOC | L, blocked |

Inkcards overlaps vCodex Flashcards. Recommendation: port Inkcards (your
format, your SM-2) rather than vCodex Flashcards.

## 4. Status (2026-10-11)

You asked for all phases to be built in one run, for the sleep wallpaper
to stay, and later for: switching between stock and Inkhome without
downloading anything, the project on GitHub with screenshots, no SD card
required, and buttons that never leave the reader looking frozen.

Done and merged to `main`, each on its own branch with a build and size
report ([SIZES](docs/inkhome/SIZES.md)) and on-device checks
([TESTING](docs/inkhome/TESTING.md)):

| Branch | What |
|---|---|
| `main` (foundation) | README, SCOPE, CHANGELOG, versioning, release workflow, OTA points at Inkhome |
| `feat/apps-framework` | Apps registry, launcher, per-app folders |
| `feat/controls-tilt-default` | Tilt page turn on by default when the IMU is present |
| `feat/apps-iffound` | If Found (vCodex) |
| `feat/apps-firmware-switch` | Switch firmware app, stock-safe updaters, no-SD menu |
| `chore/simulator-screenshots` | Simulator config, screenshots, button smoke test |
| `feat/apps-inkrecipes` | Recipes (InkRecipes), web flasher that keeps stock |
| `docs/readme-screens` | README screenshots, checklists, size report |
| `feat/stats-core` | Reading stats (roadmap 2.1 and the heatmap, 2.3) |
| `feat/sleep-stats-overlay` | Custom + stats sleep screen (roadmap 2.2) |
| `feat/apps-inkverse` | Verse app and Verse sleep screen (roadmap 2.4) |

Sleep wallpaper: upstream's Custom mode is untouched and new sleep modes
(stats overlay, roadmap 2.2) will be appended beside it.

Not done yet, in order: achievements (2.5), text darkness (2.6), then the
remaining inkkit apps (2.7 onwards). Reading stats is a
lean reimplementation of vCodex's format rather than a copy of its 4,200
lines; progress percentages, Sync Day and per-book correction are not in it
yet.

## 4a. Phase 1 scope (as planned)

The smallest set that makes Inkhome clearly "the X3 home for my e-ink
ecosystem":

1. **Foundation.** README, CHANGELOG, SCOPE, credits, version scheme
   `1.6.5.N-inkhome`, GitHub Actions release build.
2. **Apps framework** (`src/apps/`). A registry of self-contained app
   modules (id, name, icon, factory, availability check), an Apps launcher
   on Home, and a per-app SD folder convention.
3. **Tilt page turn on by default** when the IMU is detected (one default
   change; the setting still disables it; X4 never sees it).
4. **Sleep wallpaper kept.** Upstream's CUSTOM wallpaper mode stays and
   new sleep modes are appended, never replacing it.
5. **If Found screen** (vCodex, S).
6. **InkRecipes** as the first native app.

## 5. Roadmap (one feature per branch)

| Order | Branch | Feature | Notes |
|---|---|---|---|
| 2.1 | `feat/stats-core` | Reading stats store, session tracking, stats screen | vCodex JSON v6 compatible |
| 2.2 | `feat/sleep-stats-overlay` | "Wallpaper + stats" sleep mode | Keeps your wallpaper, stats as a corner card, laid out from screen size |
| 2.3 | `feat/stats-heatmap` | Reading heatmap | X3 grid sized from panel |
| 2.4 | `feat/apps-inkverse` | InkVerse | Including a sleep-screen source |
| 2.5 | `feat/stats-achievements` | Achievements | achievements.json v2 |
| 2.6 | `feat/display-text-darkness` | Text darkness | Glyph hot path, benchmark first |
| 2.7 | `feat/apps-storyscapes`, `feat/apps-chessink`, `feat/apps-inkquest`, `feat/apps-inkcards`, `feat/apps-habitink` | Remaining offline apps | One per branch |
| 2.8 | `feat/stats-syncday`, `feat/stats-correction`, `feat/stats-profile` | Remaining stats tools | |
| 2.9 | `feat/apps-pocketwiki`, `feat/apps-dailydrop` | RAM-heavy and Wi-Fi apps | DailyDrop uses CrossPoint's Wi-Fi store |
| Later | | Lyra Carousel redesign, InkWriter (BLE) | Needs your decision |

## 6. Risks

- **Partition table vs your stock dual-slot setup.** CrossPoint uses two
  0x640000 app slots with app1 at 0x650000. Your X3's stock table has
  0x770000 slots with app1 at 0x780000. Flashing a full CrossPoint image
  rewrites the table, so `xteink-flash.sh`'s "keep stock in the other slot"
  flow does not carry over. Flashing only the app image into stock's app0
  would work size-wise but CrossPoint OTA would then target a slot layout
  it does not expect. Recommendation: treat Inkhome as a full replacement
  and keep your 16 MB stock backup for recovery.
- **Flash budget.** Every app adds code plus i18n strings for up to 34
  languages. Watch the size table; `slim` and a reduced
  `custom_i18n_builtin_langs` are the escape hatches.
- **Heap.** The C3 has about 380 KB and no PSRAM. Apps must allocate in
  `onEnter`, free in `onExit`, cap file sizes and stream from SD. Each app
  states its peak heap in its header.
- **Upstream merges.** Home and Settings are the only upstream files Apps
  touches, with small, isolated hunks. Stats will need reader hooks;
  they will be kept to single-line calls into `src/inkhome/`.
- **Scope drift from upstream.** Upstream SCOPE.md excludes interactive
  apps; that is exactly why this is a fork. Nothing here is meant to be
  sent upstream except generic fixes.
- **Untested on hardware.** Nothing here has run on a device. Each change
  ships with an on-device checklist.

## 7. Size report

See `docs/inkhome/SIZES.md`, updated on every feature branch.

## 8. Open questions for you

1. **Name.** Answered: Inkhome.
2. **GitHub.** Answered: published at `mohitagw15856/inkhome`.
3. **App data location.** Phase 1 uses `/apps/<id>/` for user content
   (recipes you copy over) and `/.crosspoint/apps/<id>/` for app state.
   InkRecipes also reads the legacy `/inkrecipes/` folder so existing cards
   keep working. Happy with that?
4. **Flashcards.** Port Inkcards instead of vCodex Flashcards?
5. **InkWriter.** Enabling BLE costs 40 to 60 KB RAM. Worth exploring, or
   park it?
6. **vCodex sleep-mode import.** Map vCodex `sleepScreen` values on stats
   import, or leave sleep settings alone?
