// From InkWriter (github.com/mohitagw15856/inkwriter, MIT), unchanged apart from include paths and
// std::accumulate in wordCount (for cppcheck).
#include "Editor.h"

#include <numeric>

namespace inkwriter {

void Editor::insertChar(char c) {
  lines_[cursorLine_].insert(cursorCol_, 1, c);
  ++cursorCol_;
  dirty_ = true;
}

void Editor::insertNewline() {
  std::string& line = lines_[cursorLine_];
  const std::string tail = line.substr(cursorCol_);
  line.erase(cursorCol_);
  lines_.insert(lines_.begin() + static_cast<long>(cursorLine_) + 1, tail);
  ++cursorLine_;
  cursorCol_ = 0;
  dirty_ = true;
}

void Editor::backspace() {
  if (cursorCol_ > 0) {
    lines_[cursorLine_].erase(cursorCol_ - 1, 1);
    --cursorCol_;
    dirty_ = true;
  } else if (cursorLine_ > 0) {
    const std::string current = lines_[cursorLine_];
    lines_.erase(lines_.begin() + static_cast<long>(cursorLine_));
    --cursorLine_;
    cursorCol_ = lines_[cursorLine_].size();
    lines_[cursorLine_] += current;
    dirty_ = true;
  }
}

void Editor::moveLeft() {
  if (cursorCol_ > 0) {
    --cursorCol_;
  } else if (cursorLine_ > 0) {
    --cursorLine_;
    cursorCol_ = lines_[cursorLine_].size();
  }
}

void Editor::moveRight() {
  if (cursorCol_ < lines_[cursorLine_].size()) {
    ++cursorCol_;
  } else if (cursorLine_ + 1 < lines_.size()) {
    ++cursorLine_;
    cursorCol_ = 0;
  }
}

void Editor::moveUp() {
  if (cursorLine_ > 0) {
    --cursorLine_;
    if (cursorCol_ > lines_[cursorLine_].size()) cursorCol_ = lines_[cursorLine_].size();
  }
}

void Editor::moveDown() {
  if (cursorLine_ + 1 < lines_.size()) {
    ++cursorLine_;
    if (cursorCol_ > lines_[cursorLine_].size()) cursorCol_ = lines_[cursorLine_].size();
  }
}

void Editor::moveHome() { cursorCol_ = 0; }
void Editor::moveEnd() { cursorCol_ = lines_[cursorLine_].size(); }

std::string Editor::text() const {
  std::string out;
  for (const auto& line : lines_) {
    out += line;
    out += '\n';
  }
  return out;
}

void Editor::setText(const std::string& text) {
  lines_.clear();
  std::string current;
  for (char c : text) {
    if (c == '\n') {
      lines_.push_back(current);
      current.clear();
    } else if (c != '\r') {
      current += c;
    }
  }
  if (!current.empty() || lines_.empty()) lines_.push_back(current);
  cursorLine_ = lines_.size() - 1;
  cursorCol_ = lines_.back().size();
  dirty_ = false;
}

int countWords(const std::string& s) {
  int words = 0;
  bool inWord = false;
  for (char c : s) {
    const bool space = (c == ' ' || c == '\t');
    if (!space && !inWord) {
      ++words;
      inWord = true;
    } else if (space) {
      inWord = false;
    }
  }
  return words;
}

int Editor::wordCount() const {
  return std::accumulate(lines_.begin(), lines_.end(), 0,
                         [](const int total, const std::string& line) { return total + countWords(line); });
}

void Editor::currentParagraph(size_t& first, size_t& last) const {
  first = cursorLine_;
  last = cursorLine_;
  while (first > 0 && !lines_[first - 1].empty()) --first;
  while (last + 1 < lines_.size() && !lines_[last + 1].empty()) ++last;
}

}  // namespace inkwriter
