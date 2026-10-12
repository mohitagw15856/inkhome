#pragma once

// Card sleep mode: the next card from the most recently studied Flashcards
// deck (front and answer), a different one each sleep. Kept apart from
// InkcardsApp.h so the sleep screen does not include UI list headers.
class GfxRenderer;

namespace inkcards {

// Remembers the deck the owner last opened, for the Card sleep mode.
void rememberLastDeck(const char* source, const char* label);
// Draws the card; falls back to the built-in world capitals deck.
void drawSleepCard(const GfxRenderer& renderer);

}  // namespace inkcards
