# Writing an Inkhome app

An app is a self-contained module in `src/apps/<id>/`. The only line outside
that folder is its descriptor in `src/apps/AppRegistry.cpp`.

## Shape

```
src/apps/<id>/
  <Name>App.h / .cpp        descriptor factory and root Activity
  core/                     pure logic, no Arduino headers, host-tested
  ...                       further screens (one Activity each)
test/inkhome/<Name>Test.cpp host tests for core/
```

Keep the inkkit split: `core/` is plain C++ that builds on the host, the
activities draw with `GfxRenderer` and read buttons through
`MappedInputManager`.

## Registering

```cpp
// src/apps/AppRegistry.cpp
#include "apps/<id>/<Name>App.h"
constexpr std::array<AppDescriptor, N> APPS{{
    <name>::descriptor(),
}};
```

A descriptor holds the folder id, two i18n keys (name, one-line summary), a
factory that returns the root activity (or nullptr on OOM) and an optional
availability check (for example `[] { return gpio.deviceIsX3(); }`).

## Lifecycle

- The launcher **pushes** the app's root activity. `finish()` from the root
  returns to the launcher; Back from the launcher returns Home.
- Allocate in `onEnter`, free in `onExit`. Use `makeUniqueNoThrow`, never
  bare `new`.
- Return `true` from `preventAutoSleep()` only while the app genuinely needs
  the screen on (a running timer), never for the whole session.

## Storage

`apps/AppPaths.h` gives each app two folders, created by `ensureDirs(id)`:

| Folder | Holds |
|---|---|
| `/apps/<id>/` | content the owner copies over (recipes, decks, packs) |
| `/.crosspoint/apps/<id>/` | state the firmware writes (progress, scores) |

Read with `Storage.openFileForRead` and cap file sizes. Write state
atomically (`.tmp` then rename) and only when it changed.

## Layout

- Use `renderer.getScreenWidth()` and `getScreenHeight()`; never 800, 480,
  792 or 528.
- Use theme metrics for chrome (`GUI.drawHeader`, `GUI.drawButtonHints`).
- Design for the X3 (792 x 528) first, then check X4 (800 x 480) in portrait
  and both landscape orientations.

## Strings

Add `STR_*` keys to the end of `lib/I18n/translations/english.yaml` in
British English, and use `tr(STR_...)`. The YAML file does not allow
comments.
