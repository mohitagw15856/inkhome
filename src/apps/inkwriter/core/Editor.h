// From InkWriter (github.com/mohitagw15856/inkwriter, MIT), unchanged apart from include paths.
// Portable editor core: a line-based text buffer with cursor editing, word
// counting, and paragraph queries for focus mode. No Arduino, no display
// concepts beyond a column width for wrapping at render time.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inkwriter {

class Editor {
 public:
  Editor() { lines_.push_back(""); }

  // Editing at the cursor.
  void insertChar(char c);
  void insertNewline();
  void backspace();

  // Cursor movement (clamped).
  void moveLeft();
  void moveRight();
  void moveUp();
  void moveDown();
  void moveHome();
  void moveEnd();

  // State.
  const std::vector<std::string>& lines() const { return lines_; }
  size_t cursorLine() const { return cursorLine_; }
  size_t cursorCol() const { return cursorCol_; }
  bool dirty() const { return dirty_; }
  void clearDirty() { dirty_ = false; }

  // Whole-document text with trailing newline; used for saving.
  std::string text() const;
  // Replace the whole document (loading a draft).
  void setText(const std::string& text);

  // Total word count (whitespace-separated runs).
  int wordCount() const;

  // Paragraph containing the cursor, for focus mode: [first, last] line
  // indices of the contiguous non-blank block around the cursor line.
  void currentParagraph(size_t& first, size_t& last) const;

 private:
  std::vector<std::string> lines_;
  size_t cursorLine_ = 0;
  size_t cursorCol_ = 0;
  bool dirty_ = false;
};

// Words in a single string (exposed for the session-goal math and tests).
int countWords(const std::string& s);

}  // namespace inkwriter
