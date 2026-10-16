#pragma once

#include <string>
#include <vector>

#include "SleepOverlays.h"

class GfxRenderer;

// Sleep wallpapers in Xteink's own .xth (2-bit grey) and .xtg (1-bit)
// formats, so pictures sent with the stock firmware's app ("/Pushed Images"
// on the SD card) work as Inkhome sleep screens without converting them.
namespace inkhome::wallpaper {

// Picks a random .xth/.xtg from /Pushed Images, /sleep and /.sleep. When a BMP
// was also found, returns false half the time so both kinds take turns.
bool pickXth(std::string& outPath, bool bmpAvailable);

// Draws and displays the picture, centred (cropped if larger than the panel).
// Greyscale uses the same passes as CrossPoint's XTCH reader; with overlays
// on it renders black and white with dithered greys and draws them on top.
// Peak heap: the whole image (about 100 KB for a full-screen .xth), freed on
// return. False (nothing displayed) on a bad file or low memory.
bool drawXth(const GfxRenderer& renderer, const std::string& path, const sleep::Overlays& overlays);

// True for .xth/.xtg file names.
bool isXth(const std::string& path);

// Every picture the sleep screen can use (.xth/.xtg/.bmp in /Pushed Images,
// /sleep and /.sleep, plus /sleep.bmp), sorted by folder then name.
std::vector<std::string> listAll();

// The wallpaper pinned in the Wallpaper app, if it is still on the card.
bool pinnedPath(std::string& out);

// Pins `path` so every Custom sleep screen shows it; empty goes back to
// rotating. False if the choice could not be saved.
bool setPinned(const std::string& path);

}  // namespace inkhome::wallpaper
