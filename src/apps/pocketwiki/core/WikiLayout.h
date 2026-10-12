#pragma once

// Lays a decoded PocketWiki article out into positioned, styled segments for
// proportional fonts. A RenderSink: feed it through PwaReader::readArticle.
// Text width comes from a caller-supplied measure function, so the device
// measures with GfxRenderer and the host tests with a fixed-width stand-in.
// Inkhome's counterpart to PocketWiki's ArticleLayout (which is tied to its
// 5x7 bitmap font).

#include <cstdint>
#include <string>
#include <vector>

#include "RenderStream.h"

namespace pocketwiki {

constexpr uint32_t kNoLink = 0xFFFFFFFF;

enum class Font : uint8_t { Body, Heading };

struct LayoutMetrics {
  int width = 0;           // content width in pixels
  int bodyLineHeight = 0;  // line advance for Font::Body
  int headingLineHeight = 0;
  int indent = 0;  // per list depth
  // Width of `text` in `font` with `style` (kStyleBold / kStyleItalic bits).
  int (*measure)(const void* ctx, const char* text, Font font, uint8_t style) = nullptr;
  const void* ctx = nullptr;
};

struct Segment {
  std::string text;
  int16_t x = 0;
  int16_t width = 0;
  uint8_t style = 0;
  Font font = Font::Body;
  uint32_t link = kNoLink;
};

struct Line {
  std::vector<Segment> segments;
  int16_t height = 0;
  int16_t gapBefore = 0;
  bool rule = false;
};

class WikiLayout final : public RenderSink {
 public:
  static constexpr size_t MAX_LINES = 1500;

  explicit WikiLayout(const LayoutMetrics& metrics) : m(metrics) {}

  const std::vector<Line>& lines() const { return lines_; }
  bool truncated() const { return truncated_; }

  // First line of each page when lines are packed into `pageHeight` pixels.
  std::vector<size_t> paginate(int pageHeight) const;

  void onBlockStart(const BlockInfo& b) override;
  void onText(const char* utf8, size_t len, uint8_t style) override { addRun(utf8, len, style, kNoLink); }
  void onLink(uint32_t target, const char* label, size_t len) override { addRun(label, len, 0, target); }
  void onEnd() override { closeLine(); }

 private:
  void addRun(const char* text, size_t len, uint8_t style, uint32_t link);
  void addWord(const std::string& word, uint8_t style, uint32_t link);
  void newLine();
  void closeLine();
  int measure(const char* text, uint8_t style) const;

  LayoutMetrics m;
  std::vector<Line> lines_;
  Line current;
  bool open = false;  // `current` holds a started line
  int x = 0;
  int lineStartX = 0;  // wrapped lines of a list item hang under its text
  bool pendingSpace = false;
  Font font = Font::Body;
  uint8_t blockStyle = 0;
  int16_t nextGap = 0;
  int orderedCount[4] = {};
  bool truncated_ = false;
};

}  // namespace pocketwiki
