#include "InkVerseApp.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SleepVerse.h"
#include "apps/AppPaths.h"
#include "components/UITheme.h"
#include "core/BuiltInVerses.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkverse {

namespace {

constexpr char APP_ID[] = "inkverse";
constexpr char LEGACY_DIR[] = "/inkverse";
constexpr char SLEEP_STATE[] = "/.crosspoint/apps/inkverse/sleep.txt";
constexpr char BUILTIN_POETRY[] = "builtin:poetry";
constexpr char BUILTIN_STOIC[] = "builtin:stoic";
constexpr size_t MAX_FILE_BYTES = 32 * 1024;
constexpr size_t MAX_COLLECTIONS = 50;

// Largest first; the first size at which every line fits is used, so poems
// keep their line breaks. The last size wraps if it still has to.
#ifndef OMIT_FONTS
constexpr int VERSE_FONT_IDS[] = {NOTOSERIF_16_FONT_ID, NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID};
#else
constexpr int VERSE_FONT_IDS[] = {UI_12_FONT_ID};
#endif

using Layout = VerseLayout;

bool endsWith(const std::string& s, const char* suffix) {
  const size_t n = strlen(suffix);
  return s.size() > n && s.compare(s.size() - n, n, suffix) == 0;
}

bool loadCollection(const std::string& source, Collection& out) {
  if (source == BUILTIN_POETRY) return parseVerse(BUILT_IN_POETRY, out);
  if (source == BUILTIN_STOIC) return parseVerse(BUILT_IN_STOIC, out);
  if (!inkhome::sdMounted()) return false;
  HalFile file;
  if (!Storage.openFileForRead("VRS", source.c_str(), file)) return false;
  const size_t size = file.fileSize();
  if (size == 0 || size > MAX_FILE_BYTES) {
    LOG_ERR("VRS", "%s is %u bytes (limit %u)", source.c_str(), static_cast<unsigned>(size),
            static_cast<unsigned>(MAX_FILE_BYTES));
    return false;
  }
  std::string text(size, '\0');
  if (file.read(text.data(), size) != static_cast<int>(size)) return false;
  return parseVerse(text, out);
}

void addFolder(const char* dir, std::vector<CollectionEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_COLLECTIONS)) {
    if (out.size() >= MAX_COLLECTIONS) return;
    std::string name = e.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.empty() || name[0] == '.' || !endsWith(name, ".verse")) continue;
    std::string label = name.substr(0, name.size() - 6);
    std::replace_if(label.begin(), label.end(), [](char c) { return c == '-' || c == '_'; }, ' ');
    if (!label.empty() && label[0] >= 'a' && label[0] <= 'z') label[0] = static_cast<char>(label[0] - 'a' + 'A');
    out.push_back({label, std::string(dir) + "/" + name});
  }
}

// sleep.txt: "source=<...>" and "counter=<n>" lines.
void readSleepState(std::string& source, uint32_t& counter) {
  source = BUILTIN_POETRY;
  counter = 0;
  if (!inkhome::sdMounted() || !Storage.exists(SLEEP_STATE)) return;
  const String text = Storage.readFile(SLEEP_STATE);
  const std::string s = text.c_str();
  size_t pos = 0;
  while (pos < s.size()) {
    size_t end = s.find('\n', pos);
    if (end == std::string::npos) end = s.size();
    const std::string line = s.substr(pos, end - pos);
    pos = end + 1;
    if (line.rfind("source=", 0) == 0) source = line.substr(7);
    if (line.rfind("counter=", 0) == 0) counter = static_cast<uint32_t>(strtoul(line.c_str() + 8, nullptr, 10));
  }
}

void writeSleepState(const std::string& source, const uint32_t counter) {
  if (!inkhome::sdMounted()) return;
  inkhome::apps::ensureDirs(APP_ID);
  char text[320];
  snprintf(text, sizeof(text), "source=%s\ncounter=%u\n", source.c_str(), static_cast<unsigned>(counter));
  Storage.writeFile(SLEEP_STATE, String(text));
}

// Centred verse: title, body lines wrapped to the panel, author. Used by both
// the app and the sleep screen. Returns the number of body lines drawn.
int drawVerse(const GfxRenderer& renderer, const Verse& verse, const Layout& layout, const int top, const int bottom,
              const int firstLine) {
  const auto& body = layout.body;
  const int fontId = layout.fontId;
  const int pageWidth = renderer.getScreenWidth();
  const int lineHeight = renderer.getLineHeight(fontId);
  const int smallHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int headHeight = verse.title.empty() ? 0 : renderer.getLineHeight(UI_12_FONT_ID) + lineHeight / 2;
  const int footHeight = (verse.author.empty() ? 0 : smallHeight) + (verse.attribution.empty() ? 0 : smallHeight) +
                         (verse.author.empty() && verse.attribution.empty() ? 0 : lineHeight / 2);
  const int room = std::max(1, (bottom - top - headHeight - footHeight) / lineHeight);
  const int count = std::min(room, std::max(0, static_cast<int>(body.size()) - firstLine));
  const int blockHeight = headHeight + count * lineHeight + footHeight;
  int y = top + std::max(0, (bottom - top - blockHeight) / 2);

  if (!verse.title.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, verse.title.c_str(), true, EpdFontFamily::BOLD);
    y += headHeight;
  }
  // Body lines share one left edge, centred as a block, so poetry keeps its shape.
  int widest = 0;
  for (int i = firstLine; i < firstLine + count; ++i) {
    widest = std::max(widest, renderer.getTextWidth(fontId, body[static_cast<size_t>(i)].c_str()));
  }
  const int left = std::max(0, (pageWidth - widest) / 2);
  for (int i = firstLine; i < firstLine + count; ++i) {
    renderer.drawText(fontId, left, y, body[static_cast<size_t>(i)].c_str(), true);
    y += lineHeight;
  }
  y += lineHeight / 2;
  if (!verse.author.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, verse.author.c_str(), true, EpdFontFamily::ITALIC);
    y += smallHeight;
  }
  if (!verse.attribution.empty()) renderer.drawCenteredText(SMALL_FONT_ID, y, verse.attribution.c_str(), true);
  return count;
}

Layout layoutBody(const GfxRenderer& renderer, const Verse& verse, const int width) {
  Layout layout;
  for (const int fontId : VERSE_FONT_IDS) {
    layout.fontId = fontId;
    bool fits = true;
    for (const auto& line : verse.lines) {
      if (renderer.getTextWidth(fontId, line.c_str()) > width) {
        fits = false;
        break;
      }
    }
    if (fits) break;
  }
  layout.body.reserve(verse.lines.size());
  for (const auto& line : verse.lines) {
    if (line.empty()) {
      layout.body.emplace_back();
      continue;
    }
    auto parts = renderer.wrappedText(layout.fontId, line.c_str(), width, 4);
    if (parts.empty()) layout.body.push_back(line);
    for (auto& p : parts) layout.body.push_back(std::move(p));
  }
  return layout;
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<CollectionListActivity>(renderer, mappedInput);
}

void drawSleepVerse(const GfxRenderer& renderer) {
  std::string source;
  uint32_t counter = 0;
  readSleepState(source, counter);
  Collection collection;
  if (!loadCollection(source, collection)) {
    source = BUILTIN_POETRY;
    loadCollection(source, collection);
  }
  if (collection.verses.empty()) return;
  const size_t pick = pickVerse(Rotation::EverySleep, collection.verses.size(), 0, counter, 0);
  writeSleepState(source, counter + 1);

  const int margin = 32;
  const Layout layout = layoutBody(renderer, collection.verses[pick], renderer.getScreenWidth() - margin * 2);
  renderer.clearScreen();
  drawVerse(renderer, collection.verses[pick], layout, margin, renderer.getScreenHeight() - margin, 0);
}

// ---------------------------------------------------------------- collection list

void CollectionListActivity::onEnter() {
  UiListActivity::onEnter();
  entries.clear();
  entries.reserve(8);
  entries.push_back({tr(STR_VERSE_BUILTIN_POETRY), BUILTIN_POETRY});
  entries.push_back({tr(STR_VERSE_BUILTIN_STOIC), BUILTIN_STOIC});
  if (inkhome::sdMounted()) {
    inkhome::apps::ensureDirs(APP_ID);
    char dir[48];
    inkhome::apps::contentDir(APP_ID, dir, sizeof(dir));
    addFolder(dir, entries);
    addFolder(LEGACY_DIR, entries);
  }
  rowItems.clear();
  rowItems.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    fui::ListItem item;
    item.label = entries[i].label.c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
}

void CollectionListActivity::onExit() {
  rowItems = {};
  entries = {};
  UiListActivity::onExit();
}

int CollectionListActivity::listCount() const { return static_cast<int>(rowItems.size()); }

const char* CollectionListActivity::headerTitle() const { return tr(STR_VERSE_TITLE); }

void CollectionListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto activity = makeUniqueNoThrow<VerseActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!activity) return;
  app.clearTapFlash();
  activityManager.pushActivity(std::move(activity));
}

void CollectionListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- verse

void VerseActivity::onEnter() {
  Activity::onEnter();
  loaded = loadCollection(entry.source, collection);
  if (loaded) {
    // Open on today's verse when the clock is set, else the first.
    struct tm t = {};
    const bool clockSet = halClock.localTime(t) && t.tm_year + 1900 >= 2024;
    index = pickVerse(Rotation::Daily, collection.verses.size(), clockSet ? static_cast<uint32_t>(t.tm_yday) : 0, 0, 0);
    layoutVerse();
  }
  requestUpdate();
}

void VerseActivity::onExit() {
  collection = Collection{};
  layout = {};
  Activity::onExit();
}

void VerseActivity::layoutVerse() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  layout = layoutBody(renderer, collection.verses[index], renderer.getScreenWidth() - metrics.contentSidePadding * 2);
  scroll = 0;
  savedForSleep = false;
}

void VerseActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!loaded) return;
  const size_t count = collection.verses.size();
  if (mappedInput.wasPressed(Button::Right) || mappedInput.wasPressed(Button::PageForward)) {
    index = (index + 1) % count;
    layoutVerse();
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::PageBack)) {
    index = (index + count - 1) % count;
    layoutVerse();
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Down) && scroll + visibleLines < static_cast<int>(layout.body.size())) {
    ++scroll;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Up) && scroll > 0) {
    --scroll;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    writeSleepState(entry.source, static_cast<uint32_t>(index));
    SETTINGS.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::VERSE;
    SETTINGS.saveToFile();
    savedForSleep = true;
    requestUpdate();
  }
}

void VerseActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;

  char subtitle[24] = {};
  if (loaded) {
    snprintf(subtitle, sizeof(subtitle), "%u / %u", static_cast<unsigned>(index + 1),
             static_cast<unsigned>(collection.verses.size()));
  }
  const char* title = loaded && !collection.title.empty() ? collection.title.c_str() : entry.label.c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title, subtitle);

  if (!loaded) {
    renderer.drawCenteredText(UI_12_FONT_ID, (top + bottom) / 2, tr(STR_VERSE_LOAD_FAILED), true, EpdFontFamily::BOLD);
  } else {
    visibleLines = std::max(1, drawVerse(renderer, collection.verses[index], layout, top, bottom, scroll));
  }
  if (savedForSleep) GUI.drawPopup(renderer, tr(STR_VERSE_SLEEP_SET));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), loaded ? tr(STR_VERSE_USE_FOR_SLEEP) : "",
                                            loaded ? tr(STR_RECIPES_PREV) : "", loaded ? tr(STR_RECIPES_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkverse
