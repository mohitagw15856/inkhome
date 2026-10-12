#include "WikiLayout.h"

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace pocketwiki {

namespace {

int16_t lineHeightFor(const LayoutMetrics& m, const Font f) {
  return static_cast<int16_t>(f == Font::Heading ? m.headingLineHeight : m.bodyLineHeight);
}

}  // namespace

int WikiLayout::measure(const char* text, const uint8_t style) const {
  return m.measure ? m.measure(m.ctx, text, font, static_cast<uint8_t>(style | blockStyle)) : 0;
}

void WikiLayout::closeLine() {
  if (open && !current.segments.empty()) {
    if (lines_.size() < MAX_LINES) {
      lines_.push_back(std::move(current));
    } else {
      truncated_ = true;
    }
  }
  current = Line{};
  open = false;
  pendingSpace = false;
}

void WikiLayout::newLine() {
  closeLine();
  current.height = lineHeightFor(m, font);
  current.gapBefore = nextGap;
  nextGap = 0;
  x = lineStartX;
  open = true;
}

void WikiLayout::onBlockStart(const BlockInfo& b) {
  closeLine();
  font = Font::Body;
  blockStyle = 0;
  lineStartX = 0;
  const int16_t paraGap = static_cast<int16_t>(m.bodyLineHeight / 3);
  if (b.kind != BlockKind::ListItem) {
    std::fill(std::begin(orderedCount), std::end(orderedCount), 0);
  }
  switch (b.kind) {
    case BlockKind::Heading:
      font = b.level <= 2 ? Font::Heading : Font::Body;
      blockStyle = kStyleBold;
      nextGap = lines_.empty() ? 0 : static_cast<int16_t>(m.bodyLineHeight / 2);
      break;
    case BlockKind::Rule: {
      Line rule;
      rule.rule = true;
      rule.height = static_cast<int16_t>(m.bodyLineHeight / 2);
      rule.gapBefore = paraGap;
      if (lines_.size() < MAX_LINES) lines_.push_back(std::move(rule));
      nextGap = 0;
      return;
    }
    case BlockKind::ListItem: {
      const int depth = b.depth < 4 ? b.depth : 3;
      for (int d = depth + 1; d < 4; ++d) orderedCount[d] = 0;
      char marker[12];
      if (b.ordered) {
        snprintf(marker, sizeof(marker), "%d.", ++orderedCount[depth]);
      } else {
        snprintf(marker, sizeof(marker), "%s", "\xE2\x80\xA2");  // bullet
      }
      nextGap = static_cast<int16_t>(m.bodyLineHeight / 6);
      lineStartX = m.indent * (depth + 1);
      newLine();
      Segment s;
      s.text = marker;
      s.width = static_cast<int16_t>(measure(marker, 0));
      s.x = static_cast<int16_t>(lineStartX - s.width - m.indent / 3);
      if (s.x < 0) s.x = 0;
      current.segments.push_back(std::move(s));
      return;
    }
    case BlockKind::Paragraph:
    default:
      nextGap = lines_.empty() ? 0 : paraGap;
      break;
  }
}

void WikiLayout::addRun(const char* text, const size_t len, const uint8_t style, const uint32_t link) {
  std::string word;
  for (size_t i = 0; i < len; ++i) {
    const char c = text[i];
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
      if (!word.empty()) {
        addWord(word, style, link);
        word.clear();
      }
      pendingSpace = true;
    } else {
      word.push_back(c);
    }
  }
  if (!word.empty()) addWord(word, style, link);
}

void WikiLayout::addWord(const std::string& word, const uint8_t style, const uint32_t link) {
  if (!open) newLine();
  const int w = measure(word.c_str(), style);
  const bool lineHasText = x > lineStartX;
  const int space = pendingSpace && lineHasText ? measure(" ", style) : 0;
  if (lineHasText && x + space + w > m.width) {
    newLine();
  } else {
    x += space;
  }
  pendingSpace = false;

  const uint8_t fullStyle = static_cast<uint8_t>(style | blockStyle);
  Segment* last = current.segments.empty() ? nullptr : &current.segments.back();
  if (last && last->style == fullStyle && last->font == font && last->link == link && last->x + last->width <= x) {
    // Extend the previous segment (re-measured whole, so kerning stays right).
    const int gap = x - (last->x + last->width);
    if (gap > 0) last->text.push_back(' ');
    last->text += word;
    last->width = static_cast<int16_t>(measure(last->text.c_str(), style));
    x = last->x + last->width;
    return;
  }
  Segment s;
  s.text = word;
  s.x = static_cast<int16_t>(x);
  s.width = static_cast<int16_t>(w);
  s.style = fullStyle;
  s.font = font;
  s.link = link;
  current.segments.push_back(std::move(s));
  x += w;
}

std::vector<size_t> WikiLayout::paginate(const int pageHeight) const {
  std::vector<size_t> starts;
  starts.push_back(0);
  int used = 0;
  for (size_t i = 0; i < lines_.size(); ++i) {
    const bool firstOnPage = used == 0;
    const int need = (firstOnPage ? 0 : lines_[i].gapBefore) + lines_[i].height;
    if (!firstOnPage && used + need > pageHeight) {
      starts.push_back(i);
      used = lines_[i].height;
    } else {
      used += need;
    }
  }
  return starts;
}

}  // namespace pocketwiki
