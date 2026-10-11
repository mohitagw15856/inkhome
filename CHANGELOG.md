# Changelog

Inkhome versions are `<upstream-version>.<n>-inkhome`. Upstream changes are
in CrossPoint's own release notes.

## 1.6.5.1-inkhome (unreleased)

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
- Simulator config, screenshot script and button smoke test.
- Inkhome unit tests (`test/inkhome/`) and flasher tests in CI.

### Changed
- Tilt page turn is on by default when the X3's IMU is detected.
- Over-the-air updates check Inkhome's releases and compare four version segments.
- Wi-Fi and SD-card updates refuse to overwrite stock firmware in the other slot.
- Upstream's multi-device release workflow only runs in the upstream repository.
