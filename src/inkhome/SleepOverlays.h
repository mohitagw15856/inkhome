#pragma once

class GfxRenderer;

// Overlays drawn on top of a sleep screen before it is displayed. When any is
// on, sleep pictures render in black and white so the overlays stay crisp.
namespace inkhome::sleep {

struct Overlays {
  bool statsCard = false;
  bool ownerStrip = false;
  bool any() const { return statsCard || ownerStrip; }
};

// From the sleep mode and settings.
Overlays current(bool customStatsMode);
void draw(GfxRenderer& renderer, const Overlays& overlays);

}  // namespace inkhome::sleep
