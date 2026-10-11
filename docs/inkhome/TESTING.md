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

## Reporting
If anything hangs, connect USB and capture the serial log
(`pio device monitor -b 115200`) before restarting; the start of the log
shows which screen was entered last.
