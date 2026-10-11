#include "IfFoundApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <iterator>

#include "MappedInputManager.h"
#include "apps/AppPaths.h"
#include "components/UITheme.h"
#include "core/IfFoundText.h"
#include "fontIds.h"

namespace inkhome::iffound {

namespace {

constexpr char ROOT_PATH[] = "/if_found.txt";
constexpr char APP_PATH[] = "/apps/iffound/if_found.txt";
constexpr int MAX_BODY_LINES = 256;
constexpr int RULE_GAP = 8;

std::string findNotePath() {
  if (Storage.exists(ROOT_PATH)) return ROOT_PATH;
  for (const auto& entry : Storage.listFiles("/", 128)) {
    std::string name = entry.c_str();
    if (!isCandidateName(name)) continue;
    if (name.empty() || name.front() != '/') name.insert(0, "/");
    return name;
  }
  if (Storage.exists(APP_PATH)) return APP_PATH;
  return {};
}

std::string readNote(const std::string& path) {
  if (path.empty()) return {};
  HalFile file;
  if (!Storage.openFileForRead("IFF", path.c_str(), file)) return {};
  std::string content;
  content.reserve(std::min(MAX_BYTES, file.fileSize()));
  uint8_t buffer[128];
  while (file.available() > 0 && content.size() < MAX_BYTES) {
    const int got = file.read(buffer, std::min(MAX_BYTES - content.size(), sizeof(buffer)));
    if (got <= 0) break;
    content.append(reinterpret_cast<const char*>(buffer), static_cast<size_t>(got));
  }
  return normalise(std::move(content));
}

bool isBlank(const std::string& line) { return line.find_first_not_of(" \t") == std::string::npos; }

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<IfFoundActivity>(renderer, mappedInput);
}

void IfFoundActivity::onEnter() {
  Activity::onEnter();
  loadContent();
  requestUpdate();
}

void IfFoundActivity::onExit() {
  introLines = {};
  bodyLines = {};
  Activity::onExit();
}

void IfFoundActivity::loadContent() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2 - metrics.scrollBarWidth -
                        metrics.scrollBarRightOffset;

  introLines = renderer.wrappedText(UI_10_FONT_ID, tr(STR_IF_FOUND_MESSAGE), textWidth, 8);

  std::string note = readNote(findNotePath());
  if (isBlank(note)) note = tr(STR_IF_FOUND_FILE_HINT);

  bodyLines.clear();
  bodyLines.reserve(32);
  size_t start = 0;
  while (start <= note.size() && static_cast<int>(bodyLines.size()) < MAX_BODY_LINES) {
    size_t end = note.find('\n', start);
    if (end == std::string::npos) end = note.size();
    const std::string raw = note.substr(start, end - start);
    start = end + 1;
    if (isBlank(raw)) {
      bodyLines.emplace_back();
      continue;
    }
    auto wrapped = renderer.wrappedText(UI_10_FONT_ID, raw.c_str(), textWidth, 64, EpdFontFamily::BOLD);
    if (wrapped.empty()) {
      bodyLines.push_back(raw);
    } else {
      bodyLines.insert(bodyLines.end(), std::make_move_iterator(wrapped.begin()),
                       std::make_move_iterator(wrapped.end()));
    }
  }
  while (!bodyLines.empty() && bodyLines.back().empty()) bodyLines.pop_back();
  scrollOffset = std::clamp(scrollOffset, 0, maxScroll());
}

int IfFoundActivity::bodyTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  return metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing +
         static_cast<int>(introLines.size()) * lineHeight + RULE_GAP * 2;
}

int IfFoundActivity::visibleBodyLines() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int viewport = renderer.getScreenHeight() - bodyTop() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  return std::max(1, viewport / renderer.getLineHeight(UI_10_FONT_ID));
}

int IfFoundActivity::maxScroll() const { return std::max(0, static_cast<int>(bodyLines.size()) - visibleBodyLines()); }

void IfFoundActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int page = visibleBodyLines();
    const int next =
        std::clamp(scrollOffset + (swipe == MappedInputManager::SwipeDir::Up ? page : -page), 0, maxScroll());
    if (next != scrollOffset) {
      scrollOffset = next;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right, MappedInputManager::Button::Down}, [this] {
    if (scrollOffset < maxScroll()) {
      ++scrollOffset;
      requestUpdate();
    }
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left, MappedInputManager::Button::Up}, [this] {
    if (scrollOffset > 0) {
      --scrollOffset;
      requestUpdate();
    }
  });
}

void IfFoundActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int side = metrics.contentSidePadding;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_IF_FOUND_RETURN_ME));

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  for (const auto& line : introLines) {
    renderer.drawText(UI_10_FONT_ID, side, y, line.c_str(), true);
    y += lineHeight;
  }
  y += RULE_GAP;
  renderer.drawLine(side, y, pageWidth - side, y);

  const int top = bodyTop();
  const int visible = visibleBodyLines();
  const int end = std::min(static_cast<int>(bodyLines.size()), scrollOffset + visible);
  y = top;
  for (int i = scrollOffset; i < end; ++i) {
    if (!bodyLines[i].empty())
      renderer.drawText(UI_10_FONT_ID, side, y, bodyLines[i].c_str(), true, EpdFontFamily::BOLD);
    y += lineHeight;
  }

  const int scrollMax = maxScroll();
  if (scrollMax > 0) {
    const int viewport = pageHeight - top - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const int trackX = pageWidth - metrics.scrollBarRightOffset;
    const int thumb = std::max(18, viewport * visible / static_cast<int>(bodyLines.size()));
    const int thumbY = top + (viewport - thumb) * scrollOffset / scrollMax;
    renderer.drawLine(trackX, top, trackX, top + viewport, true);
    renderer.fillRect(trackX - metrics.scrollBarWidth + 1, thumbY, metrics.scrollBarWidth, thumb, true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkhome::iffound
