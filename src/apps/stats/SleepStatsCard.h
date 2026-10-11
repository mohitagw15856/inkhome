#pragma once

class GfxRenderer;

namespace inkhome::stats {

// Draws a compact stats card (today, streak, current book) near the bottom of
// the sleep screen, sized from the renderer and fonts so it fits any panel
// and orientation. Black on white; drawn last over the wallpaper.
void drawSleepCard(const GfxRenderer& renderer);

}  // namespace inkhome::stats
