// Parser and rotation policy for .verse collections (InkVerse docs/FORMAT.md).
// From InkVerse (github.com/mohitagw15856/inkverse, MIT), unchanged.
// Pure, hardware-free logic covered by the host tests.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inkverse {

struct Verse {
  std::string title;       // S line (poem/source title)
  std::string author;      // A line
  std::string attribution; // B line (optional attribution/translation credit)
  std::vector<std::string> lines;  // T lines, pre-wrapped by the companion
};

struct Collection {
  std::string title;
  std::string attribution;
  std::vector<Verse> verses;
};

// Parses a whole .verse document. Returns false on bad magic. Verses beyond
// `maxVerses` and lines beyond `maxLines` per verse are dropped, not fatal.
bool parseVerse(const std::string& text, Collection& out, size_t maxVerses = 400,
                size_t maxLines = 64);

// Rotation policies, mirroring the settings screen order.
enum class Rotation : uint8_t { Daily = 0, EverySleep = 1, Random = 2, Sequential = 3 };

// Chooses the verse index to show.
//   dayKey:   days since epoch (or any value stable for a day)
//   counter:  persisted wake/advance counter (EverySleep/Sequential)
//   seed:     entropy for Random (e.g. millis at wake)
size_t pickVerse(Rotation policy, size_t verseCount, uint32_t dayKey, uint32_t counter,
                 uint32_t seed);

}  // namespace inkverse
