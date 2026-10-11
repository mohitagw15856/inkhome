#!/usr/bin/env bash
# Renders README screenshots with crosspoint-simulator and drives every
# screen with scripted button presses, so it doubles as a smoke test: a
# screen that hangs or crashes fails the run.
#
#   scripts/inkhome/screenshots.sh            # writes docs/images/inkhome/*.png
#
# Needs PlatformIO and SDL2 (macOS: brew install sdl2). The tree is copied to
# a scratch folder because the simulator writes .pio/ and fs_/ into it.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${INKHOME_SIM_DIR:-${TMPDIR:-/tmp}/inkhome-sim}"
SIM_COMMIT="20e7380"
OUT="$REPO/docs/images/inkhome"
mkdir -p "$WORK" "$OUT"

if [ ! -d "$WORK/crosspoint-simulator/.git" ]; then
  git clone -q https://github.com/crosspoint-reader/crosspoint-simulator.git "$WORK/crosspoint-simulator"
fi
git -C "$WORK/crosspoint-simulator" checkout -q "$SIM_COMMIT"

rsync -a --delete --exclude .pio --exclude build --exclude .cache --exclude managed_components \
  --exclude fs_ --exclude .git "$REPO/" "$WORK/inkhome/"
cp "$REPO/scripts/inkhome/simulator.ini" "$WORK/inkhome/platformio.local.ini"

cd "$WORK/inkhome"
pio run -e simulator -s
pio run -e simulator_x3 -s

to_png() {
  if command -v sips >/dev/null; then sips -s format png "$1" --out "$2" >/dev/null
  else convert "$1" "$2"; fi
}

# shoot <env> <name> <input-script> [extra env]
# Input times are ms from start; the capture is taken 600 ms before QUIT.
shoot() {
  local env="$1" name="$2" script="$3"; shift 3
  local quit_at; quit_at="$(echo "$script" | tr ';' '\n' | tail -1 | cut -d: -f1)"
  local at=$((quit_at - 600))
  rm -rf fs_ && mkdir -p fs_
  if [ -d "$REPO/scripts/inkhome/sim-sd" ]; then cp -R "$REPO/scripts/inkhome/sim-sd/." fs_/; fi
  # Optional per-shot extra card content, space-separated folder names (e.g. a wallpaper and a sleep setting).
  for extra in ${EXTRA_SD:-}; do cp -R "$REPO/scripts/inkhome/$extra/." fs_/; done
  local log="$WORK/$name.log"
  env "$@" SDL_VIDEODRIVER=dummy CROSSPOINT_SIM_INPUT_SCRIPT="$script" \
    CROSSPOINT_SIM_SCREENSHOTS="$at:$WORK/$name.bmp" \
    perl -e 'alarm 30; exec @ARGV' ".pio/build/$env/program" >"$log" 2>&1 || {
      echo "FAIL $name (exit $?), log: $log"; tail -20 "$log"; exit 1; }
  if grep -qiE 'abort|panic|assert|segmentation' "$log"; then
    echo "FAIL $name: crash signature in $log"; exit 1
  fi
  to_png "$WORK/$name.bmp" "$OUT/$name.png"
  echo "ok   $name"
}

# Home list order without recent books: Browse Files, Library, Apps, File Transfer, Settings.
# Apps order: Reading stats, Focus timer, Recipes, Verse, If Found, Switch firmware.
APPS='900:DOWN;1200:DOWN;1500:ENTER'
FOCUS="$APPS;2100:DOWN;2400:ENTER"
RECIPES="$APPS;2100:DOWN;2400:DOWN;2700:ENTER"
VERSE="$APPS;2100:DOWN;2400:DOWN;2700:DOWN;3000:ENTER"
shoot simulator_x3 x3-home        '2600:QUIT'
shoot simulator_x3 x3-apps        "$APPS;3200:QUIT"
shoot simulator_x3 x3-stats       "$APPS;2100:ENTER;3800:QUIT"
shoot simulator_x3 x3-focus       "$FOCUS;4000:QUIT"
shoot simulator_x3 x3-recipes     "$RECIPES;4300:QUIT"
shoot simulator_x3 x3-recipe      "$RECIPES;3400:ENTER;5000:QUIT"
shoot simulator_x3 x3-cooking     "$RECIPES;3400:ENTER;4100:UP;4500:ENTER;5200:ENTER;7000:QUIT"
shoot simulator_x3 x3-verse       "$VERSE;3700:ENTER;5500:QUIT"
shoot simulator_x3 x3-iffound     "$APPS;2100:DOWN;2400:DOWN;2700:DOWN;3000:DOWN;3300:ENTER;4900:QUIT"
shoot simulator_x3 x3-fwswitch    "$APPS;2100:DOWN;2400:DOWN;2700:DOWN;3000:DOWN;3300:DOWN;3600:ENTER;5200:QUIT"
shoot simulator_x3 x3-no-sd       '2600:QUIT' INKHOME_SIM_NO_SD=1
shoot simulator    x4-home        '2600:QUIT'
shoot simulator    x4-apps        "$APPS;3200:QUIT"
shoot simulator    x4-stats       "$APPS;2100:ENTER;3800:QUIT"
EXTRA_SD=sim-sd-sleep shoot simulator_x3 x3-sleep-stats '1500:SLEEP;4500:QUIT'
EXTRA_SD=sim-sd-verse shoot simulator_x3 x3-sleep-verse '1500:SLEEP;4500:QUIT'
EXTRA_SD=sim-sd-xth shoot simulator_x3 x3-sleep-pushed '1500:SLEEP;5000:QUIT'
EXTRA_SD='sim-sd-xth sim-sd-owner' shoot simulator_x3 x3-sleep-owner '1500:SLEEP;5000:QUIT'

# A reading session: open the sample book (Project Gutenberg's The Time Machine), turn eleven
# pages at a steady pace so the time-left estimate appears, then sleep on Cover + stats.
READ='900:ENTER;2000:DOWN;2400:ENTER;3600:ENTER'
t=10000
for _ in $(seq 1 11); do READ="$READ;$t:RIGHT"; t=$((t + 2600)); done
shoot simulator_x3 x3-reader "$READ;$((t + 800)):QUIT"
EXTRA_SD=sim-sd-cover shoot simulator_x3 x3-sleep-cover "$READ;$((t + 500)):SLEEP;$((t + 5600)):QUIT"

# Into and back out of every app, then Home: a key path that hangs fails here.
shoot simulator_x3 x3-roundtrip   "$APPS;2100:ENTER;2800:BACK;3300:DOWN;3600:ENTER;4200:BACK;4700:DOWN;5000:ENTER;5600:BACK;6100:DOWN;6400:ENTER;7000:ENTER;7600:RIGHT;8000:BACK;8500:BACK;9000:DOWN;9300:ENTER;9900:BACK;10400:DOWN;10700:ENTER;11300:BACK;11800:BACK;13500:QUIT"
grep -q 'Entering activity: Home' "$WORK/x3-roundtrip.log" || { echo "FAIL roundtrip: never returned Home"; exit 1; }
rm -f "$OUT/x3-roundtrip.png"
echo "Screenshots in $OUT"
