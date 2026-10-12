#include "NotesFormat.h"

#include <cstdio>

namespace inkhome::notes {

std::string fileNameFor(const std::string& title) {
  std::string out;
  out.reserve(title.size());
  for (const char c : title) {
    switch (c) {
      case '/':
      case '\\':
      case ':':
      case '*':
      case '?':
      case '"':
      case '<':
      case '>':
      case '|':
        out += " - ";
        break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20) out.push_back(c);
    }
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '.' || out.back() == '-')) out.pop_back();
  if (out.size() > 80) {
    out.resize(80);
    // Never cut a UTF-8 character in half.
    while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) out.pop_back();
    if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0x80)) out.pop_back();
  }
  if (out.empty()) out = "Untitled";
  return out + ".md";
}

std::string markdownFor(const std::string& title, const std::string& author, const std::vector<Mark>& marks) {
  std::string md = "# " + title + "\n";
  if (!author.empty()) md += "*" + author + "*\n";
  md += "\n<!-- Written by Inkhome from your bookmarks; it is replaced on the next export. -->\n\n";
  char percent[8];
  for (const auto& m : marks) {
    const int p = static_cast<int>(m.fraction * 100.0f + 0.5f);
    snprintf(percent, sizeof(percent), "%d%%", p < 0 ? 0 : p > 100 ? 100 : p);
    md += "- **" + std::string(percent) + "**";
    if (!m.passage.empty()) md += " \"" + m.passage + "\"";
    if (!m.label.empty()) md += " (" + m.label + ")";
    md += "\n";
  }
  return md;
}

}  // namespace inkhome::notes
