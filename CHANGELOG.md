# Changelog

Inkhome versions are `<upstream-version>.<n>-inkhome`. Upstream changes are
in CrossPoint's own release notes.

## 1.6.5.3-inkhome (pre-release)

### Added
- Time left in the chapter on the reader's status bar, from your own pace.
- Focus timer app: read for 15 to 60 minutes, then a break screen.
- Owner details on sleep screen: your If Found note across the top of every
  sleep screen.
- Custom sleep screens show Xteink .xth/.xtg pictures in greyscale, including
  the stock app's /Pushed Images folder, alongside BMP wallpapers.
- scripts/inkhome/xth_to_bmp.py converts .xth/.xtg pictures to BMP on a computer.

## 1.6.5.2-inkhome (pre-release)

### Changed
- Start-up and default sleep screens say Inkhome (with "Based on CrossPoint
  Reader" on start-up), so it is clear which firmware is running.
- The flasher verifies the whole image on the chip before switching slots,
  and keeps the computer awake during installs.

## 1.6.5.1-inkhome (pre-release)

Based on CrossPoint Reader 1.6.5. Not yet verified on a device; see
docs/inkhome/TESTING.md.

### Added
- Web flasher that installs Inkhome beside the stock firmware and switches
  between the two from the browser, choosing the slot by identifying stock.
- Apps framework and launcher on Home, with a per-app SD folder layout.
- Recipes app (native port of InkRecipes) with a built-in recipe.
- If Found app (ported from CPR-vCodex).
- Switch firmware app: restart into the stock firmware from the reader.
- No-SD-card menu instead of a static error screen.
- Reading stats app, compatible with CPR-vCodex's reading_stats.json
  (format 6), with a heatmap, streaks, per-book time, export and import.
- Custom + stats sleep screen: your wallpaper with a reading card, beside the
  unchanged Custom mode.
- Verse app (native port of InkVerse) and a Verse sleep screen.
- Simulator config, screenshot script and button smoke test.
- Inkhome unit tests (`test/inkhome/`) and flasher tests in CI.

### Changed
- Tilt page turn is on by default when the X3's IMU is detected.
- Over-the-air updates check Inkhome's releases and compare four version segments.
- Wi-Fi and SD-card updates refuse to overwrite stock firmware in the other slot.
- Upstream's multi-device release workflow only runs in the upstream repository.
