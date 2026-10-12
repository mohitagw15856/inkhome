#pragma once

// Markdown for a book's bookmarks, and a safe file name for it. Pure, host-tested.

#include <string>
#include <vector>

namespace inkhome::notes {

struct Mark {
  float fraction = 0;  // 0..1 through the book
  std::string passage;
  std::string label;
};

// "Moby-Dick: or, The Whale" -> "Moby-Dick - or, The Whale" (FAT-safe, max 80 bytes).
std::string fileNameFor(const std::string& title);

// "# Title\n*Author*\n\n- **34%** \"passage\" (label)\n..." with a generated-file note.
std::string markdownFor(const std::string& title, const std::string& author, const std::vector<Mark>& marks);

}  // namespace inkhome::notes
