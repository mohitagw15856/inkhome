#pragma once

// Highlights: passages chosen in the reader (menu, Highlight a passage),
// appended per book to /notes/inkhome/highlights/<title>.md beside the
// bookmark notes.

#include <string>

namespace inkhome::notes {

// False when the card is missing, the file is too large or the write fails.
bool saveHighlight(const std::string& title, const std::string& passage, int percent);

}  // namespace inkhome::notes
