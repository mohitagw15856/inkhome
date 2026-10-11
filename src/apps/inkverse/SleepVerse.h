#pragma once

// Kept apart from InkVerseApp.h so the sleep screen does not pull in the UI
// list headers (PNGdec, included there, defines a `local` macro they collide with).
class GfxRenderer;

namespace inkverse {

// Draws the next verse of the chosen collection for the Verse sleep mode and
// advances the rotation. Falls back to the built-in poetry collection.
void drawSleepVerse(const GfxRenderer& renderer);

}  // namespace inkverse
