#pragma once

// The Mandarin sleep screen: one HSK1 word per sleep (characters, pinyin and
// meaning), moving to the next word each time. The words come from the
// Mandarin90 app's HSK1 list; progress is kept in
// /.crosspoint/apps/mandarin/next.txt.

class GfxRenderer;

namespace inkhome::mandarin {

void drawSleepWord(const GfxRenderer& renderer);

}  // namespace inkhome::mandarin
