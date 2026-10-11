// From InkVerse (github.com/mohitagw15856/inkverse, MIT), unchanged.
#include "VerseDoc.h"

namespace inkverse {

namespace {

void feedLine(Collection& out, const std::string& line, bool& inVerse, size_t maxVerses,
              size_t maxLines) {
  if (line == "%%") {
    if (out.verses.size() < maxVerses) {
      out.verses.emplace_back();
      inVerse = true;
    } else {
      inVerse = false;
    }
    return;
  }
  const char tag = line.empty() ? '\0' : line[0];
  const bool hasSpace = line.size() >= 2 && line[1] == ' ';
  const std::string rest = hasSpace ? line.substr(2) : std::string();

  if (!inVerse) {
    if (tag == 'M') {
      const size_t sp = rest.find(' ');
      const std::string key = rest.substr(0, sp);
      const std::string value = (sp == std::string::npos) ? "" : rest.substr(sp + 1);
      if (key == "title") out.title = value;
      if (key == "attribution") out.attribution = value;
    }
    return;
  }

  Verse& v = out.verses.back();
  switch (tag) {
    case 'S': v.title = rest; break;
    case 'A': v.author = rest; break;
    case 'B': v.attribution = rest; break;
    case 'T':
      if (v.lines.size() < maxLines) v.lines.push_back(rest);
      break;
    default: break;  // unknown tags skipped for forward compatibility
  }
}

}  // namespace

bool parseVerse(const std::string& text, Collection& out, size_t maxVerses,
                size_t maxLines) {
  out = Collection{};
  bool first = true;
  bool inVerse = false;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (first) {
      if (line != "VERSE 1") return false;
      first = false;
      continue;
    }
    if (line.empty()) continue;
    feedLine(out, line, inVerse, maxVerses, maxLines);
    if (end == text.size()) break;
  }
  // Drop a trailing empty verse from a terminal %%.
  while (!out.verses.empty() && out.verses.back().lines.empty() &&
         out.verses.back().title.empty()) {
    out.verses.pop_back();
  }
  return !out.verses.empty();
}

size_t pickVerse(Rotation policy, size_t verseCount, uint32_t dayKey, uint32_t counter,
                 uint32_t seed) {
  if (verseCount == 0) return 0;
  switch (policy) {
    case Rotation::Daily:
      return dayKey % verseCount;
    case Rotation::EverySleep:
    case Rotation::Sequential:
      return counter % verseCount;
    case Rotation::Random: {
      // xorshift keeps this deterministic for tests while spreading seeds.
      uint32_t x = seed ? seed : 0x9E3779B9u;
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      return x % verseCount;
    }
  }
  return 0;
}

}  // namespace inkverse
