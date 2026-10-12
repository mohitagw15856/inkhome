#pragma once

// Soft-wraps the editor's lines into screen rows for a proportional font and
// finds the row and x position of the cursor. Width comes from a caller
// measure function (GfxRenderer on the device, fixed width in host tests).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inkwriter {

struct Row {
  size_t line = 0;   // editor line index
  size_t start = 0;  // first byte of the row within the line
  size_t end = 0;    // one past the last byte
};

using MeasureFn = int (*)(const void* ctx, const char* text, size_t len);

// Wraps lines [firstLine, lastLine] at word boundaries (a word longer than
// the width is split). An empty line still makes one row.
std::vector<Row> wrapRows(const std::vector<std::string>& lines, size_t firstLine, size_t lastLine, int width,
                          MeasureFn measure, const void* ctx);

// Index of the row holding the cursor, or rows.size() if none does.
size_t cursorRow(const std::vector<Row>& rows, size_t line, size_t col);

}  // namespace inkwriter
