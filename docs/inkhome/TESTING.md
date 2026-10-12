# On-device test checklist

Nothing in Inkhome has run on real hardware yet. Every screen below has been
driven with scripted button presses in crosspoint-simulator at X3 and X4
size (`scripts/inkhome/screenshots.sh`), which catches hangs and crashes but
not panel, button-wiring or flash behaviour. Please work through this on
the X3, then on an X4 if you have one, and note the result against each line.

Keep the 16 MB stock backup (`xteink/device-backup/full-20260904-2227.bin`)
to hand before step 1.

## 1. Web flasher, install beside stock
- [ ] Open the flasher page in Chrome, Connect. Both slots are listed; on
      your X3 both show "Stock firmware" and app1 shows "starts now".
- [ ] The plan says Inkhome replaces the copy in app0 and keeps app1.
- [ ] Install. Progress completes, then "Inkhome is installed in app0".
- [ ] Unplug, hold Power about 2 s. Inkhome boots to Home (not frozen).

## 2. Buttons and Home
- [ ] Up/Down move on Home; Confirm opens; Back returns. Front buttons
      match their on-screen hints.
- [ ] Home shows Browse Files, Library, Apps, File Transfer, Settings and
      nothing overlaps the button hints, in portrait and both landscapes
      (Settings, Display, Orientation).
- [ ] Hold Power: the reader sleeps; press Power: it wakes.

## 3. Apps
- [ ] Apps lists Recipes, If Found, Switch firmware. Back returns to Home
      with Apps highlighted.
- [ ] Recipes: built-in Shakshuka opens without any card content. Tick an
      ingredient (Confirm), change servings (Left/Right) and quantities
      rescale. Up from the top reaches Start cooking.
- [ ] Cooking: Right/Left change step; Confirm starts a timer; the corner
      updates about every 15 s, then each second in the last 10 s; at zero
      the "Time's up" banner appears with a full refresh. The reader does
      not auto-sleep while the timer runs, and does once you leave.
- [ ] Copy `.rcp` files to `/apps/inkrecipes/` (or the old `/inkrecipes/`):
      they appear in the list.
- [ ] If Found: shows the hint, then your note after you add
      `/if_found.txt`. Up/Down scroll a long note.

## 4. Switch firmware (on the device)
- [ ] Apps, Switch firmware: shows "Running: Inkhome ... (app0)" and
      "Other slot: Stock firmware (app1)".
- [ ] Confirm, then Confirm again: the reader restarts into stock.
- [ ] From stock, back to Inkhome via the flasher's Start Inkhome.

## 5. Updates keep stock
- [ ] Settings, Check for Updates (Wi-Fi) or SD Card Firmware Update:
      either refuses with "Use the web flasher to keep stock." Stock is still
      in app1 afterwards (confirm with the flasher's Connect).
- [ ] Re-run the flasher's Install: it says it updates Inkhome in app0 and
      keeps app1.

## 6. No SD card
- [ ] Remove the card and restart: the No SD card menu appears (not a
      frozen "SD card error" page).
- [ ] Apps opens Recipes (built-in recipe works) and If Found (shows the
      hint). Back returns to the No SD card menu.
- [ ] Insert the card, choose Try again: the reader restarts normally.

## 7. Tilt (X3 only)
- [ ] Settings, Controls shows Tilt Page Turn = Normal on a fresh install.
- [ ] In a book, a flick turns the page. Setting it to Off stops that.
- [ ] On an X4 the setting is absent and nothing changes.

## 8. Sleep wallpaper
- [ ] Settings, Display, Sleep Screen = Custom with BMPs in `/sleep`: your
      wallpaper shows on sleep, in greyscale, as on upstream CrossPoint.
- [ ] Sleep Screen = Custom + stats: the same wallpaper (black and white)
      with a card at the bottom showing today's minutes and streak, plus the
      book when you slept from the reader.

- [ ] Sleep Screen = Custom with your stock app pictures still in
      `/Pushed Images` on the card: they show in greyscale, taking turns with
      any BMPs in `/sleep`. With Custom + stats they show in black and white
      with the card.

## 9. Reading stats
- [ ] Settings, System, Clock: set the time (X3). Read a book for a few
      minutes, turning pages. Apps, Reading stats shows today's minutes and
      a filled square for today in the heatmap.
- [ ] Leave the book idle for over five minutes: the idle time is not counted.
- [ ] Sleep and wake in the reader: `/.crosspoint/reading_stats.json` exists
      and grows; `reading_stats.json.bak` holds the previous copy.
- [ ] Confirm (Export) writes `/exports/stats_exported`. Copy a vCodex
      `reading_stats.json` there, press Left then Confirm (Import): its books
      and days appear.
- [ ] X4: the screen says the reader has no clock and still counts per book.

## 10. Verse
- [ ] Apps, Verse lists the two built-in collections (and any `.verse` files
      in `/apps/inkverse/` or `/inkverse/`). A poem keeps its line breaks.
- [ ] Left/Right change verse; Up/Down scroll a long one.
- [ ] Confirm (Sleep) shows "Shown on the sleep screen from now on"; on
      sleep the next verse appears, a different one each time.
- [ ] Settings, Display, Sleep Screen lists Verse after Custom + stats.

## 11. Batch 1 additions
- [ ] Reader: after about eight page turns, the status bar shows "~N min"
      beside the chapter page count. Settings, Reader, Time left in chapter
      turns it off.
- [ ] Apps, Focus timer, 15 minutes: your book opens. After 15 minutes of
      reading a break screen appears; Again restarts, Done returns to the book.
- [ ] Settings, Display, Owner details on sleep screen = On with
      `/if_found.txt` on the card: every sleep screen shows "If found:" and
      your first two lines across the top.

## 12. Cover + stats and Year in books
- [ ] Sleep Screen = Cover + stats, sleep from a book: the cover shows with a
      card (today, streak, the book, a progress bar and minutes left).
- [ ] Apps, Year in books: the card shows this year. Up/Down change year.
      Save writes `/exports/year-in-books-<year>.bmp`; Sleep writes it into
      `/sleep`, where Custom picks it up.

## 13. Achievements
- [ ] Settings, Reader, Daily reading goal: 15/30/45/60 min (30 by default).
- [ ] Read for a minute and leave the book: Apps, Achievements shows "Start
      your first book" and "Your first reading session" unlocked with today's
      date, and `/.crosspoint/achievements.json` exists.
- [ ] Copy a vCodex `achievements.json` over: its unlocked badges show.

## 14. Battery health and motion (X3)
- [ ] Apps, Battery health: health %, full charge / design mAh, cycles, charge,
      current and temperature look plausible. After a few days of sleeping
      the chart shows a line. (Not listed on an X4.)
- [ ] Apps, Motion sensor: values change as you tilt the reader; total is
      about 1.00 g when still. After a few button presses "Screen faces"
      shows +Z or -Z.
- [ ] Settings, Controls, Shake to bookmark = On: in a book, three firm
      jolts add a bookmark (the bookmark message appears). Note whether a
      page also turns.
- [ ] Settings, Controls, Face down to sleep = On: after pressing a few
      buttons, laying the reader screen down for two seconds sends it to
      sleep. Lying face up on a table must not.

## 15. Break reminder and quiet hours
- [ ] Settings, Reader, Break reminder = 30 min: after 30 minutes of reading
      in one go, the break screen appears; Done returns to the book.
- [ ] Settings, Reader, Quiet hours = On, clock after 22:00: full-screen
      flashes come about three times less often than in the day.

## 16. Flashcards
- [ ] Apps, Flashcards, World capitals: a country shows; Show reveals the
      capital; Left = Again, Confirm = Good, Right = Easy, side Down = Hard.
      The header counts down.
- [ ] Leave and return: graded cards are not shown again today
      (`/.crosspoint/apps/inkcards/builtin-capitals.rev` exists).
- [ ] Copy `hsk1.deck` to `/apps/inkcards/` with a CJK SD font installed:
      hanzi show on the front, pinyin and English on the back.

## 17. Habits
- [ ] Apps, Habits (clock set): with no `/.habitink` it starts with three
      example habits. Confirm ticks the selected one; the count in the
      header and its streak update. `/.habitink/logs/01.log` gains a line.
- [ ] An existing Habitink card shows its own habits and streaks.
- [ ] Settings, Display, Habits on sleep screen = On: today's habits show as
      tick boxes along the bottom of the sleep screen, under any stats card.

## 18. Flashcard sleep screen
- [ ] Study any deck in Flashcards, then Settings, Display, Sleep Screen =
      Flashcard: each sleep shows the next card from that deck (front, then
      pinyin and answer). Without any study it uses world capitals.

## 19. Chess puzzles
- [ ] Apps, Chess puzzles, Chess tactics: Left/Right/Up/Down move the cursor;
      Confirm on your piece shows dots on its legal moves; Confirm on a dot
      plays it. The right move shows "Solved!", a wrong one shows the answer.
      Confirm goes to the next puzzle; rating and streak update in the header.
- [ ] Xiangqi endgames: Chinese piece characters on the intersections, palaces
      and the river drawn; moves work the same way.

## 20. StoryScapes
- [ ] Copy `isle-of-serpents.ssp` (StoryScapes' demo pack) to
      `/apps/storyscapes/`. Apps, StoryScapes lists it; Confirm opens it.
- [ ] Right/Down/Confirm go forward, Left/Up back. Text pages fill the
      width; the map plate shows whole (turned sideways) with its caption.
- [ ] Back and reopen: it returns to the same page.

## 21. Notes
- [ ] Add two bookmarks in a book (reader menu), then sleep: the card has
      `/notes/inkhome/<book title>.md` listing both with percentages and the
      page's opening words.
- [ ] Apps, Notes lists books with bookmarks; Confirm exports them all and
      each row says how many were written. Your own files in `/notes` are
      never touched.

## 22. DailyDrop
- [ ] Copy a `.drop` digest to `/apps/dailydrop/`: Apps, DailyDrop lists it;
      Left/Right move between sections, Up/Down scroll a long one.
- [ ] Put `url=https://...your digest...` in `/apps/dailydrop/config.txt`.
      Fetch today's drop: pick or confirm your saved Wi-Fi, the download
      runs, then "Today's drop is ready". Back restarts quietly to Home with
      Wi-Fi off; the new file is in the list.
- [ ] A bad URL shows "could not be fetched" and leaves no stray file.

## 23. Inbox
- [ ] File Transfer (Wi-Fi): upload an EPUB into the `inbox` folder. Apps,
      Inbox lists it; Confirm opens it in the reader. A BMP opens in the
      picture viewer.

## 24. Text darkness
- [ ] Settings, Reader, Text darkness = Darker: the grey edges of letters in a
      book look heavier; Crisp makes them lighter; Normal matches upstream.
      (Text anti-aliasing must be on.)

## Reporting
If anything hangs, connect USB and capture the serial log
(`pio device monitor -b 115200`) before restarting; the start of the log
shows which screen was entered last.
