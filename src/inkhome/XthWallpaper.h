#pragma once

#include <string>

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
bool drawXth(GfxRenderer& renderer, const std::string& path, const sleep::Overlays& overlays);

}  // namespace inkhome::wallpaper
