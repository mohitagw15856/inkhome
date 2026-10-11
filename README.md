# Inkhome

Inkhome is an X3-first fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
for the Xteink X3 (and, unchanged, the X4). It keeps CrossPoint's reader and
adds a proper home for a small e-ink ecosystem: native on-device **Apps**,
reading statistics ported from [CPR-vCodex](https://github.com/franssjz/cpr-vcodex),
and the X3's motion sensor as a first-class feature.

> Status: early. Built and unit tested on a computer; **not yet verified on a
> device**. Every change ships with an on-device checklist in
> [docs/inkhome/TESTING.md](docs/inkhome/TESTING.md).

## Why a fork

Upstream CrossPoint deliberately keeps interactive apps out of scope
([docs/UPSTREAM_SCOPE.md](docs/UPSTREAM_SCOPE.md)). Inkhome is where they
live instead. Everything else tracks upstream closely, and the fork keeps its
changes small and isolated so upstream releases merge cleanly.

## What it adds

- **Apps.** A launcher on Home and a small framework (`src/apps/`) where each
  app is a self-contained module with its own SD folder.
  - **InkRecipes**: recipe checklist with a servings scaler and a cooking mode
    with big type and step timers.
  - **If Found**: shows your contact details so a lost reader can find its
    way home.
- **Tilt page turn on by default** on the X3 when its QMI8658 sensor is
  detected. You can turn it off in Settings, Controls. The X4 has no sensor
  and is unaffected.
- **Your sleep wallpaper stays.** The Custom sleep screen (your own images
  from `/sleep` or `/.sleep`) is kept, and new sleep modes are added
  alongside it rather than replacing it.
- **Updates come from Inkhome.** The over-the-air and SD updaters look for
  Inkhome releases, so a fork build is never "updated" back to stock
  CrossPoint by accident.

The full plan and roadmap are in [PLAN.md](PLAN.md). Scope rules are in
[SCOPE.md](SCOPE.md).

## X3 first, X4 safe

- One binary for both devices; the device is detected at start-up using
  upstream's runtime detection (`gpio.deviceIsX3()`).
- No screen sizes are hard-coded. Every screen lays out from the renderer's
  width and height, is designed for the X3's 792 x 528 panel first and is
  checked against the X4's 800 x 480.
- The ESP32-C3 has no PSRAM. Apps allocate on entry, free on exit, cap the
  size of files they read and keep state on the SD card under
  `/.crosspoint/`.

## Install

**Web flasher (first install).** Open the Inkhome flasher page in Chrome or
Edge, connect the reader over USB and press Connect and install. The page is
published from [docs/flash](docs/flash) by the release workflow.

This writes a full image including the partition table, which replaces the
stock firmware. Take a full backup first if you may want stock back.

**Updating.** Copy `inkhome-<version>-x3-x4.bin` from a release to the SD
card and choose Settings, SD Card Firmware Update, or use Check for Updates
over Wi-Fi.

**Building it yourself.**

```sh
git clone --recursive https://github.com/mohitagw15856/inkhome.git
cd inkhome
pio run -e default           # development build, serial logging on
pio run -e gh_release        # release build
# app image: .pio/build/<env>/firmware.bin
# full image for a USB flash at offset 0: .pio/build/<env>/firmware.factory.bin
```

Unit tests: `pio run -t unit-tests`, or configure `test/` with CMake and run
`ctest`. The fork's own tests live in `test/inkhome/`.

## Versioning

`<upstream-version>.<n>-inkhome`, for example `1.6.5.1-inkhome` is the first
Inkhome release on top of CrossPoint 1.6.5. Release tags match the version
exactly.

## Your data

Inkhome keeps CrossPoint's `/.crosspoint/` layout. Apps keep user content
in `/apps/<app>/` and app state in `/.crosspoint/apps/<app>/`. Reading
statistics, when ported, keep CPR-vCodex's JSON formats so you can bring
your history across.

## Credits

- **CrossPoint Reader**, the CrossPoint Reader organisation and contributors
  (MIT). Inkhome is their firmware with additions; the upstream README is
  kept at [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).
- **CPR-vCodex** by franssjz (MIT). Features ported from vCodex are credited
  in the source files and commit messages that bring them in. vCodex in turn
  credits zgredex and erickosanchezj (Lyra Carousel), uxjulia and CrossInk
  (highlights) and the crosspet fork (text darkness).
- **FreeInk SDK** (MIT), the hardware layer CrossPoint is built on.
- **inkkit apps** by Mohit (MIT): InkRecipes and the rest of the ecosystem.

Inkhome does not include vCodex's built-in Bookerly font, which is not
openly licensed.

## Licence

MIT, see [LICENSE](LICENSE). The CrossPoint copyright notice is preserved;
fork additions are copyright Mohit and contributors under the same licence.
