#pragma once

// Fork identity. Renaming the fork means editing this file, the [crosspoint]
// version suffix in platformio.ini and the release workflow asset prefix.
namespace inkhome {

constexpr char FORK_NAME[] = "Inkhome";
// Product names are not translated.
constexpr const char* FORK_NAME_FOR_UI = FORK_NAME;
// OTA checks this repository's latest release, never upstream's, so a fork
// build is not offered stock CrossPoint as an "update".
constexpr char RELEASE_API_URL[] = "https://api.github.com/repos/mohitagw15856/inkhome/releases/latest";
// Release assets are named <prefix><tag><device-suffix>.bin.
constexpr char ASSET_PREFIX[] = "inkhome-";

}  // namespace inkhome
