#include "WrapRows.h"

namespace inkwriter {

namespace {

// Byte length of the UTF-8 sequence starting with `lead`.
size_t charLen(const unsigned char lead) {
  if (lead < 0x80) return 1;
  if ((lead >> 5) == 0x06) return 2;
  if ((lead >> 4) == 0x0E) return 3;
  if ((lead >> 3) == 0x1E) return 4;
  return 1;
}

}  // namespace

std::vector<Row> wrapRows(const std::vector<std::string>& lines, const size_t firstLine, const size_t lastLine,
                          const int width, const MeasureFn measure, const void* ctx) {
  std::vector<Row> rows;
  for (size_t l = firstLine; l <= lastLine && l < lines.size(); ++l) {
    const std::string& text = lines[l];
    size_t start = 0;
    if (text.empty()) {
      rows.push_back({l, 0, 0});
      continue;
    }
    while (start < text.size()) {
      // Grow the row one character at a time, remembering the last space.
      size_t end = start;
      size_t lastBreak = 0;
      while (end < text.size()) {
        const size_t next = end + charLen(static_cast<unsigned char>(text[end]));
        if (measure(ctx, text.data() + start, next - start) > width && end > start) break;
        if (text[end] == ' ') lastBreak = next;
        end = next;
      }
      if (end < text.size() && lastBreak > start) end = lastBreak;
      rows.push_back({l, start, end});
      start = end;
    }
  }
  return rows;
}

size_t cursorRow(const std::vector<Row>& rows, const size_t line, const size_t col) {
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].line != line) continue;
    const bool lastOfLine = i + 1 == rows.size() || rows[i + 1].line != line;
    if (col < rows[i].end || lastOfLine) return i;
  }
  return rows.size();
}

}  // namespace inkwriter
