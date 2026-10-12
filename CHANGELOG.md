# Changelog

Inkhome versions are `<upstream-version>.<n>-inkhome`. Upstream changes are
in CrossPoint's own release notes.

## 1.6.5.7-inkhome (pre-release)

### Added
- InkQuest app (native port): interactive fiction from /apps/inkquest/ and
  the legacy /inkquest/stories/, with The Lighthouse Keeper built in and an
  automatic save after every choice.
- Text darkness (Settings, Reader): Normal, Crisp or Darker anti-aliased text,
  after CPR-vCodex; uses vCodex's textDarkness setting values.

## 1.6.5.6-inkhome (pre-release)

### Added
- Inbox app: books and pictures sent to /inbox (with File Transfer over
  Wi-Fi, or copied on a computer) open in one tap.

## 1.6.5.5-inkhome (pre-release)

### Added
- StoryScapes app (native port): illustrated history packs from
  /apps/storyscapes/, with progress saved per pack.
- Notes: bookmarks exported as Markdown to /notes/inkhome/, refreshed for the
  current book on every sleep.
- DailyDrop app (native port): .drop digests from /apps/dailydrop/, and a
  one-off "Fetch today's drop" over saved Wi-Fi.

## 1.6.5.4-inkhome (pre-release)

### Added
- Chess puzzles app (native port of ChessInk): chess and xiangqi, with
  built-in samplers, legal-move highlighting, rating and streak.
- Flashcards app (native port of Inkcards) and a Flashcard sleep screen.
- Habits app (native port of Habitink) and Habits on sleep screen.
- Break reminder after 30, 45 or 60 minutes of continuous reading.
- Quiet hours (X3): three times fewer full-screen flashes from 22:00 to 07:00.

## 1.6.5.3-inkhome (pre-release)

### Added
- Time left in the chapter on the reader's status bar, from your own pace.
- Cover + stats sleep screen: the book's cover with progress, time left and streak.
- Year in books: your reading year on one card, saved as a picture or a sleep screen.
- Achievements: 62 badges compatible with CPR-vCodex's achievements.json, and a
  Daily reading goal setting.
- Battery health (X3): capacity, cycles, health and temperature from the
  BQ27220 gauge, with a daily capacity chart.
- Motion sensor screen (X3), and experimental shake to bookmark and face down
  to sleep in Settings, Controls (off by default).
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
