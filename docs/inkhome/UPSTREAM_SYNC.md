# Merging upstream CrossPoint

Remotes: `upstream` (crosspoint-reader/crosspoint-reader, never pushed to),
`vcodex` (franssjz/cpr-vcodex, read only), `origin` (the Inkhome repo).

## Taking a new upstream release

```sh
git fetch upstream --tags
git switch main
git switch -c chore/upstream-<version>
git merge <version>            # a stable tag, never an rc
```

Expected conflicts and how to resolve them:

| File | Resolution |
|---|---|
| `README.md`, `SCOPE.md` | Keep ours. Refresh `docs/UPSTREAM_README.md` and `docs/UPSTREAM_SCOPE.md` from the new tag. |
| `platformio.ini` `[crosspoint] version` | Set `<new-upstream>.1-inkhome`. |
| `src/network/OtaUpdater.cpp` | Keep the `inkhome::` URL, asset prefix and version compare. |
| `src/activities/home/HomeActivity.*` | Re-apply the Apps menu entry (search for `APPS`). |
| `src/CrossPointSettings.h` | Keep Inkhome's appended sleep modes (CUSTOM_STATS = 8, VERSE = 9) after upstream's, and the tilt default. If upstream appends its own sleep mode, renumber ours after it and migrate saved values. |
| `src/SettingsList.h` | Re-add the two sleep-mode labels. |
| `src/activities/boot_sleep/SleepActivity.*` | Re-apply the `statsCard` flag and the CUSTOM_STATS and VERSE cases. |
| `src/activities/util/BmpViewerActivity.cpp` | Keep "Set as sleep image" from replacing Custom + stats. |
| `src/main.cpp` | Keep the no-SD branch, `inkhome::stats::tick` in the loop and `inkhome::stats::flush` before deep sleep. |
| `src/network/FirmwareFlasher.cpp`, `src/activities/settings/{Ota,SdFirmware}UpdateActivity.cpp` | Keep the `otherSlotHoldsOtherFirmware` guards. |
| `.github/workflows/ci.yml` | Keep `main` in the push branches and the flasher test step. |

Then build `default` and `gh_release`, run the unit tests, update
`docs/inkhome/SIZES.md` and `CHANGELOG.md`.

## Where Inkhome touches upstream files

Search for `Inkhome` in `src/`, `lib/`, `.github/` and `test/CMakeLists.txt`.
Every hunk outside `src/apps/`, `src/inkhome/` and `test/inkhome/` carries
that marker or is listed in the table above.
