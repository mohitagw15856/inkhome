// From DailyDrop (github.com/mohitagw15856/dailydrop, MIT), include paths and one loop tidied for cppcheck.
#include "DropDoc.h"

#include <algorithm>
#include <cstddef>

namespace dailydrop {

namespace {

bool isDigits(const std::string& s, size_t from, size_t count) {
  if (s.size() < from + count) return false;
  return std::all_of(s.begin() + static_cast<std::ptrdiff_t>(from),
                     s.begin() + static_cast<std::ptrdiff_t>(from + count),
                     [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

bool DropParser::feedLine(const std::string& line) {
  if (first_) {
    first_ = false;
    // Magic line pins the format version so future formats fail loudly.
    if (line != "DROP 1") return false;
    sawMagic_ = true;
    return true;
  }
  if (!sawMagic_) return false;
  if (line.empty()) return true;

  const char tag = line[0];
  const bool hasSpace = line.size() >= 2 && line[1] == ' ';
  const std::string rest = hasSpace ? line.substr(2) : std::string();

  DropEvent e;
  switch (tag) {
    case 'M': {
      // "M key value"
      e.kind = DropEvent::Kind::Meta;
      const size_t sp = rest.find(' ');
      if (sp == std::string::npos) {
        e.key = rest;
      } else {
        e.key = rest.substr(0, sp);
        e.text = rest.substr(sp + 1);
      }
      break;
    }
    case 'S':
      e.kind = DropEvent::Kind::Section;
      e.text = rest;
      ++sections_;
      break;
    case 'H':
      e.kind = DropEvent::Kind::Heading;
      e.text = rest;
      break;
    case 'T':
      e.kind = DropEvent::Kind::Text;
      e.text = rest;
      break;
    case 'R':
      e.kind = DropEvent::Kind::Rule;
      break;
    default:
      // Unknown tags are skipped so old firmware tolerates newer builders.
      return true;
  }
  sink_.onEvent(e);
  return true;
}

bool isDropFilename(const std::string& name) {
  // YYYY-MM-DD.drop = 15 characters.
  if (name.size() != 15) return false;
  if (name.compare(10, 5, ".drop") != 0) return false;
  if (name[4] != '-' || name[7] != '-') return false;
  return isDigits(name, 0, 4) && isDigits(name, 5, 2) && isDigits(name, 8, 2);
}

std::string dropDate(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  const std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
  if (!isDropFilename(name)) return std::string();
  return name.substr(0, 10);
}

}  // namespace dailydrop
