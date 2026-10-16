#pragma once

// The Clock sleep screen: the time the device went to sleep in large
// segment digits, the date and the battery. E-ink keeps the picture without
// power, so the time does not move until the next sleep; the caption says
// so. Without a set clock it shows how to set one.

class GfxRenderer;

namespace inkhome::sleep {

void drawClock(const GfxRenderer& renderer);

}  // namespace inkhome::sleep
