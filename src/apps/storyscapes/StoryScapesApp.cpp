#include "StoryScapesApp.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppPaths.h"
#include "apps/inkkit/HalFileReader.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace storyscapes {

namespace {

constexpr char APP_ID[] = "storyscapes";
constexpr char LEGACY_DIR[] = "/storyscapes";
constexpr char STATE_DIR[] = "/.crosspoint/apps/storyscapes";
constexpr size_t MAX_PACKS = 40;
constexpr size_t MAX_TEXT_BYTES = 6000;
constexpr size_t MAX_CAPTION_BYTES = 200;
constexpr int MARGIN = 20;

#ifndef OMIT_FONTS
constexpr int BODY_FONT_ID = NOTOSERIF_14_FONT_ID;
#else
constexpr int BODY_FONT_ID = UI_12_FONT_ID;
#endif

std::string progressPath(const PackEntry& e) { return std::string(STATE_DIR) + "/" + e.label + ".txt"; }

void addFolder(const char* dir, std::vector<PackEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_PACKS)) {
    if (out.size() >= MAX_PACKS) return;
    std::string name = e.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() < 5 || name[0] == '.' || name.compare(name.size() - 4, 4, ".ssp") != 0) continue;
    out.push_back({name.substr(0, name.size() - 4), std::string(dir) + "/" + name});
  }
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LibraryActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- library

void LibraryActivity::onEnter() {
  UiListActivity::onEnter();
  entries.clear();
  if (inkhome::sdMounted()) {
    inkhome::apps::ensureDirs(APP_ID);
    char dir[48];
    inkhome::apps::contentDir(APP_ID, dir, sizeof(dir));
    addFolder(dir, entries);
    addFolder(LEGACY_DIR, entries);
  }
  rows.assign(entries.size(), fui::ListItem{});
  for (size_t i = 0; i < entries.size(); ++i) {
    rows[i].label = entries[i].label.c_str();
    rows[i].actionValue = static_cast<int16_t>(i);
  }
}

void LibraryActivity::onExit() {
  rows = {};
  entries = {};
  UiListActivity::onExit();
}

int LibraryActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* LibraryActivity::headerTitle() const { return tr(STR_STORY_TITLE); }

void LibraryActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto reader = makeUniqueNoThrow<PackReaderActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!reader) return;
  app.clearTapFlash();
  activityManager.pushActivity(std::move(reader));
}

void LibraryActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  if (rows.empty()) {
    emptyRow.label = tr(STR_STORY_EMPTY);
    emptyRow.enabled = false;
    props.items = &emptyRow;
    props.count = 1;
    props.inputMask = fui::InputNone;
    screen.list(props);
    return;
  }
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- reader

PackReaderActivity::PackReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, PackEntry entry)
    : Activity("StoryScapesPack", renderer, mappedInput), entry(std::move(entry)) {}

PackReaderActivity::~PackReaderActivity() = default;

void PackReaderActivity::onEnter() {
  Activity::onEnter();
  if (inkhome::sdMounted() && Storage.openFileForRead("SSP", entry.path.c_str(), file)) {
    inkkit::HalFileReader reader(file);
    loaded = parsePack(reader, info) && !info.pages.empty();
  }
  if (loaded && Storage.exists(progressPath(entry).c_str())) {
    page = std::min<uint32_t>(Progress::parse(Storage.readFile(progressPath(entry).c_str()).c_str()).page,
                              static_cast<uint32_t>(info.pages.size() - 1));
  }
  fullRefresh = true;
  requestUpdate();
}

void PackReaderActivity::onExit() {
  saveProgress();
  file.close();
  info = PackInfo{};
  Activity::onExit();
}

void PackReaderActivity::saveProgress() const {
  if (!loaded || !inkhome::sdMounted()) return;
  Storage.mkdir(STATE_DIR, true);
  Progress p;
  p.page = page;
  Storage.writeFile(progressPath(entry).c_str(), String(p.serialise().c_str()));
}

void PackReaderActivity::goTo(const uint32_t next) {
  if (!loaded || next >= info.pages.size() || next == page) return;
  page = next;
  // Pictures look best after a clean refresh; text can flip quickly.
  fullRefresh = info.pages[page].type == PageType::Image1bpp;
  requestUpdate();
}

void PackReaderActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(Button::Right) || mappedInput.wasPressed(Button::Down) ||
      mappedInput.wasPressed(Button::PageForward) || mappedInput.wasReleased(Button::Confirm)) {
    goTo(page + 1);
  } else if ((mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::Up) ||
              mappedInput.wasPressed(Button::PageBack)) &&
             page > 0) {
    goTo(page - 1);
  }
}

bool PackReaderActivity::drawImagePage(const PageEntry& entryPage) {
  inkkit::HalFileReader reader(file);
  ImageHeader header;
  if (!readImageHeader(reader, entryPage, header) || header.width == 0 || header.height == 0) return false;
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  // A landscape picture on a portrait screen turns a quarter clockwise when that
  // fits it better (packs built for an X4 in landscape); otherwise it is
  // centred and, if larger than the panel, cropped evenly.
  const bool rotate = header.width > width && header.height <= width && header.height < header.width;
  const int drawnW = rotate ? header.height : header.width;
  const int drawnH = rotate ? header.width : header.height;
  const int offX = (width - drawnW) / 2;
  const int offY = (height - drawnH) / 2;
  const size_t stride = (header.width + 7) / 8;
  auto row = makeUniqueNoThrow<uint8_t[]>(stride);
  if (!row) return false;
  for (int y = 0; y < header.height; ++y) {
    if (reader.read(row.get(), stride) != stride) break;
    for (int x = 0; x < header.width; ++x) {
      const int sx = offX + (rotate ? header.height - 1 - y : x);
      const int sy = offY + (rotate ? x : y);
      if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
      const bool white = row[static_cast<size_t>(x >> 3)] & (0x80 >> (x & 7));
      if (!white) renderer.drawPixel(sx, sy, true);
    }
  }
  if (header.captionLength > 0) {
    std::string caption(std::min<size_t>(header.captionLength, MAX_CAPTION_BYTES), '\0');
    if (reader.read(caption.data(), caption.size()) == caption.size()) {
      const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
      const auto lines = renderer.wrappedText(UI_10_FONT_ID, caption.c_str(), width - MARGIN * 2, 3);
      const int bandH = static_cast<int>(lines.size()) * lineH + 12;
      renderer.fillRect(0, height - bandH, width, bandH, true);
      int y = height - bandH + 6;
      for (const auto& l : lines) {
        renderer.drawCenteredText(UI_10_FONT_ID, y, l.c_str(), false);
        y += lineH;
      }
    }
  }
  return true;
}

void PackReaderActivity::drawTextPage(const PageEntry& entryPage) {
  inkkit::HalFileReader reader(file);
  if (!reader.seek(entryPage.offset)) return;
  std::string text(std::min<size_t>(entryPage.length, MAX_TEXT_BYTES), '\0');
  if (reader.read(text.data(), text.size()) != text.size()) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int textWidth = width - MARGIN * 2;
  const int lineH = renderer.getLineHeight(BODY_FONT_ID);
  int y = metrics.topPadding + metrics.verticalSpacing * 2;

  // Pre-wrapped 62-column lines join back into paragraphs (a blank line ends one).
  size_t pos = 0;
  bool first = true;
  std::string paragraph;
  auto flush = [&] {
    if (paragraph.empty()) return;
    for (const auto& l : renderer.wrappedText(BODY_FONT_ID, paragraph.c_str(), textWidth, 40)) {
      if (y + lineH > bottom) break;
      renderer.drawText(BODY_FONT_ID, MARGIN, y, l.c_str(), true);
      y += lineH;
    }
    y += lineH / 2;
    paragraph.clear();
  };
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    if (first && entryPage.chapterStart) {
      for (const auto& l : renderer.wrappedText(UI_12_FONT_ID, line.c_str(), textWidth, 3, EpdFontFamily::BOLD)) {
        renderer.drawText(UI_12_FONT_ID, MARGIN, y, l.c_str(), true, EpdFontFamily::BOLD);
        y += renderer.getLineHeight(UI_12_FONT_ID);
      }
      y += lineH / 2;
    } else if (line.find_first_not_of(" \t\r") == std::string::npos) {
      flush();
    } else {
      if (!paragraph.empty()) paragraph.push_back(' ');
      paragraph += line;
    }
    first = false;
    if (end == text.size()) break;
  }
  flush();
}

void PackReaderActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  bool image = false;
  if (!loaded) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, entry.label.c_str());
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_STORY_LOAD_FAILED), true,
                              EpdFontFamily::BOLD);
  } else {
    const PageEntry& current = info.pages[page];
    image = current.type == PageType::Image1bpp;
    if (image) {
      if (!drawImagePage(current)) {
        renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_STORY_LOAD_FAILED), true);
      }
    } else {
      drawTextPage(current);
    }
  }
  if (!image) {
    char pos[32] = {};
    if (loaded)
      snprintf(pos, sizeof(pos), "%u / %u", static_cast<unsigned>(page + 1), static_cast<unsigned>(info.pages.size()));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), pos, loaded && page > 0 ? tr(STR_RECIPES_PREV) : "",
                                              loaded && page + 1 < info.pages.size() ? tr(STR_RECIPES_NEXT) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(fullRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  fullRefresh = false;
}

}  // namespace storyscapes
