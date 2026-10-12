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
    perl -e 'alarm 90; exec @ARGV' ".pio/build/$env/program" >"$log" 2>&1 || {
      echo "FAIL $name (exit $?), log: $log"; tail -20 "$log"; exit 1; }
  if grep -qiE 'abort|panic|assert|segmentation' "$log"; then
    echo "FAIL $name: crash signature in $log"; exit 1
  fi
  to_png "$WORK/$name.bmp" "$OUT/$name.png"
  echo "ok   $name"
}

# Home list order without recent books: Browse Files, Library, Apps, File Transfer, Settings.
APPS='900:DOWN;1200:DOWN;1500:ENTER'
# Launcher order, as in src/apps/AppRegistry.cpp.
APP_ORDER=(stats year achievements notes focus habits inkcards chessink inkrecipes inkverse storyscapes iffound battery motion fwswitch)

# app <id> [extra steps...]: input script that opens an app from Home, then runs
# "ms-offset:KEY" steps relative to the moment the app opened. Prints the script
# and the time it ends on, as "<script> <end>".
app() {
  local id="$1"; shift
  local t=2100 steps="$APPS" i
  for i in "${!APP_ORDER[@]}"; do
    [ "${APP_ORDER[$i]}" = "$id" ] && break
    steps="$steps;$t:DOWN"; t=$((t + 300))
  done
  steps="$steps;$t:ENTER"
  local base=$((t + 700)) step
  for step in "$@"; do
    steps="$steps;$((base + ${step%%:*})):${step#*:}"
    t=$((base + ${step%%:*}))
  done
  echo "$steps $t"
}
# shoot_app <env> <name> <id> [steps...]: open the app, run the steps, capture.
shoot_app() {
  local env="$1" name="$2"; shift 2
  local out; out="$(app "$@")"
  shoot "$env" "$name" "${out% *};$(( ${out##* } + 1800 )):QUIT"
}

shoot simulator_x3 x3-home        '2600:QUIT'
shoot simulator_x3 x3-apps        "$APPS;3200:QUIT"
shoot_app simulator_x3 x3-stats    stats
shoot_app simulator_x3 x3-year     year
shoot_app simulator_x3 x3-achievements achievements
shoot_app simulator_x3 x3-focus    focus
shoot_app simulator_x3 x3-habits   habits
shoot_app simulator_x3 x3-card     inkcards 0:DOWN 400:ENTER
shoot_app simulator_x3 x3-card-back inkcards 0:DOWN 400:ENTER 1500:ENTER
shoot_app simulator_x3 x3-chess    chessink 0:ENTER 900:DOWN 1200:DOWN 1500:DOWN 1800:ENTER
shoot_app simulator_x3 x3-xiangqi  chessink 0:DOWN 400:ENTER
shoot_app simulator_x3 x3-recipes  inkrecipes
shoot_app simulator_x3 x3-recipe   inkrecipes 0:ENTER
shoot_app simulator_x3 x3-cooking  inkrecipes 0:ENTER 700:UP 1100:ENTER 1800:ENTER
shoot_app simulator_x3 x3-verse    inkverse 0:ENTER
shoot_app simulator_x3 x3-story    storyscapes 0:ENTER
shoot_app simulator_x3 x3-story-2  storyscapes 0:ENTER 1500:RIGHT
shoot_app simulator_x3 x3-story-3  storyscapes 0:ENTER 1500:RIGHT 3000:RIGHT
shoot_app simulator_x3 x3-iffound  iffound
shoot_app simulator_x3 x3-battery battery
shoot_app simulator_x3 x3-motion motion
shoot_app simulator_x3 x3-fwswitch fwswitch
shoot simulator_x3 x3-no-sd       '2600:QUIT' INKHOME_SIM_NO_SD=1
shoot simulator    x4-home        '2600:QUIT'
shoot simulator    x4-apps        "$APPS;3200:QUIT"
shoot_app simulator x4-stats       stats
EXTRA_SD=sim-sd-sleep shoot simulator_x3 x3-sleep-stats '1500:SLEEP;4500:QUIT'
EXTRA_SD=sim-sd-verse shoot simulator_x3 x3-sleep-verse '1500:SLEEP;4500:QUIT'
EXTRA_SD=sim-sd-flashcard shoot simulator_x3 x3-sleep-flashcard '1500:SLEEP;4500:QUIT'
EXTRA_SD=sim-sd-xth shoot simulator_x3 x3-sleep-pushed '1500:SLEEP;5000:QUIT'
EXTRA_SD='sim-sd-xth sim-sd-owner' shoot simulator_x3 x3-sleep-owner '1500:SLEEP;5000:QUIT'

# A reading session: open the sample book (Project Gutenberg's The Time Machine), turn eleven
# pages at a steady pace so the time-left estimate appears, then sleep on Cover + stats.
READ='900:ENTER;2000:DOWN;2400:ENTER;3600:ENTER'
t=10000
for _ in $(seq 1 11); do READ="$READ;$t:RIGHT"; t=$((t + 2600)); done
shoot simulator_x3 x3-reader "$READ;$((t + 800)):QUIT"
EXTRA_SD=sim-sd-cover shoot simulator_x3 x3-sleep-cover "$READ;$((t + 500)):SLEEP;$((t + 5600)):QUIT"
EXTRA_SD=sim-sd-habits shoot simulator_x3 x3-sleep-habits "$READ;$((t + 500)):SLEEP;$((t + 5600)):QUIT"

# Into and back out of every app, then Home: a key path that hangs fails here.
RT="$APPS"; t=2100
for _ in "${APP_ORDER[@]}"; do
  RT="$RT;$t:ENTER;$((t + 900)):BACK;$((t + 1500)):DOWN"; t=$((t + 1800))
done
shoot simulator_x3 x3-roundtrip "$RT;$t:BACK;$((t + 1500)):QUIT"
# Home is entered once at start-up; a second entry means the round trip came back.
[ "$(grep -c 'Entering activity: Home' "$WORK/x3-roundtrip.log")" -ge 2 ] || { echo "FAIL roundtrip: never returned Home"; exit 1; }
rm -f "$OUT/x3-roundtrip.png"
echo "Screenshots in $OUT"
