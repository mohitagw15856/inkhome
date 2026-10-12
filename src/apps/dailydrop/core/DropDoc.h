// From DailyDrop (github.com/mohitagw15856/dailydrop, MIT), unchanged apart from include paths.
// Parser for the .drop digest format (see docs/FORMAT.md).
//
// Pure, hardware-free logic: the parser consumes the file line by line via a
// caller-supplied line feeder and emits events through a small sink interface,
// so the device never buffers a whole digest and the host tests never touch
// hardware. Lines arrive pre-wrapped by the companion builder; the device does
// no wrapping of its own.
#pragma once

#include <cstdint>
#include <string>

namespace dailydrop {

// One parsed line of a digest document.
struct DropEvent {
  enum class Kind : uint8_t {
    Meta,     // key/value from the header (date, title, goal)
    Section,  // a new section starts; text is the section name
    Heading,  // an emphasised line within a section (e.g. an article title)
    Text,     // a body line, pre-wrapped
    Rule,     // a horizontal separator request
  };
  Kind kind;
  std::string key;   // Meta only
  std::string text;  // all kinds except Rule
};

class DropSink {
 public:
  virtual ~DropSink() = default;
  virtual void onEvent(const DropEvent& e) = 0;
};

// Feed `line` (without trailing newline) for each line of the file, in order,
// then call finish(). Returns false from feedLine when the document is
// irrecoverably malformed (bad magic); the caller should stop.
class DropParser {
 public:
  explicit DropParser(DropSink& sink) : sink_(sink) {}

  bool feedLine(const std::string& line);
  bool sawMagic() const { return sawMagic_; }
  int sectionCount() const { return sections_; }

 private:
  DropSink& sink_;
  bool sawMagic_ = false;
  bool first_ = true;
  int sections_ = 0;
};

// True if `name` looks like a digest filename: YYYY-MM-DD.drop.
bool isDropFilename(const std::string& name);

// Extracts the YYYY-MM-DD date from a digest filename or path. Returns an
// empty string if the name is not a digest filename.
std::string dropDate(const std::string& path);

}  // namespace dailydrop
