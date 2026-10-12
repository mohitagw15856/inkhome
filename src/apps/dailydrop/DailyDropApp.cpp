#include "DailyDropApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "apps/AppPaths.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "apps/stats/core/ReadingStatsData.h"
#include "components/UITheme.h"
#include "core/DropDoc.h"
#include "fontIds.h"
#include "inkhome/SdState.h"
#include "network/HttpDownloader.h"

namespace fui = freeink::ui;

namespace dailydrop {

namespace {

constexpr char APP_ID[] = "dailydrop";
constexpr char APP_DIR[] = "/apps/dailydrop";
constexpr char LEGACY_DIR[] = "/dailydrop";
constexpr char CONFIG_PATH[] = "/apps/dailydrop/config.txt";
constexpr char LEGACY_CONFIG_PATH[] = "/dailydrop/config.txt";
constexpr size_t MAX_DROPS = 30;
constexpr size_t MAX_FILE_BYTES = 24 * 1024;
constexpr size_t MAX_SECTIONS = 12;
constexpr size_t MAX_SECTION_LINES = 200;

// "url=..." (or "url ..."), from the new or the legacy config.
std::string configuredUrl() {
  for (const char* path : {CONFIG_PATH, LEGACY_CONFIG_PATH}) {
    if (!Storage.exists(path)) continue;
    const std::string text = Storage.readFile(path).c_str();
    size_t pos = 0;
    while (pos < text.size()) {
      size_t end = text.find('\n', pos);
      if (end == std::string::npos) end = text.size();
      std::string line = text.substr(pos, end - pos);
      pos = end + 1;
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.rfind("url", 0) == 0 && line.size() > 4 && (line[3] == '=' || line[3] == ' ')) return line.substr(4);
    }
  }
  return {};
}

// Today's file name from the clock, or "latest.drop" without one.
std::string todayFileName() {
  const uint32_t day = inkhome::stats::todayOrdinal();
  if (day == 0) return "latest.drop";
  int y = 0;
  unsigned m = 0;
  unsigned d = 0;
  inkhome::stats::civilFromDays(day, y, m, d);
  char name[24];
  snprintf(name, sizeof(name), "%04d-%02u-%02u.drop", y, m, d);
  return name;
}

class Collector final : public DropSink {
 public:
  explicit Collector(DropViewActivity::Section* out, size_t& count, std::string& title)
      : sections(out), sectionCount(count), docTitle(title) {}
  void onEvent(const DropEvent& e) override {
    using Kind = DropEvent::Kind;
    if (e.kind == Kind::Meta) {
      if (e.key == "title") docTitle = e.text;
      return;
    }
    if (e.kind == Kind::Section) {
      if (sectionCount < MAX_SECTIONS) sections[sectionCount++].name = e.text;
      return;
    }
    if (sectionCount == 0) sections[sectionCount++].name = "";
    auto& lines = sections[sectionCount - 1].lines;
    if (lines.size() >= MAX_SECTION_LINES) return;
    using LineKind = DropViewActivity::Line::Kind;
    lines.push_back({e.kind == Kind::Heading ? LineKind::Heading
                     : e.kind == Kind::Rule  ? LineKind::Rule
                                             : LineKind::Text,
                     e.text});
  }

 private:
  DropViewActivity::Section* sections;
  size_t& sectionCount;
  std::string& docTitle;
};

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DropListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- list

void DropListActivity::onEnter() {
  UiListActivity::onEnter();
  paths.clear();
  labels.clear();
  url.clear();
  if (inkhome::sdMounted()) {
    inkhome::apps::ensureDirs(APP_ID);
    url = configuredUrl();
    for (const char* dir : {APP_DIR, LEGACY_DIR}) {
      if (!Storage.exists(dir)) continue;
      for (const auto& e : Storage.listFiles(dir, MAX_DROPS)) {
        std::string name = e.c_str();
        const auto slash = name.find_last_of('/');
        if (slash != std::string::npos) name = name.substr(slash + 1);
        if ((isDropFilename(name) || name == "latest.drop") && paths.size() < MAX_DROPS) {
          paths.push_back(std::string(dir) + "/" + name);
        }
      }
    }
    // Dated names sort newest first by text.
    std::sort(paths.begin(), paths.end(), [](const std::string& a, const std::string& b) {
      return a.substr(a.find_last_of('/') + 1) > b.substr(b.find_last_of('/') + 1);
    });
  }
  labels.reserve(paths.size() + 1);
  labels.push_back(url.empty() ? tr(STR_DROP_NO_URL) : tr(STR_DROP_FETCH));
  for (const auto& p : paths) {
    std::string name = p.substr(p.find_last_of('/') + 1);
    labels.push_back(name.substr(0, name.size() - 5));
  }
  rows.assign(labels.size(), fui::ListItem{});
  for (size_t i = 0; i < labels.size(); ++i) {
    rows[i].label = labels[i].c_str();
    rows[i].actionValue = static_cast<int16_t>(i);
  }
  rows[0].enabled = !url.empty();
}

void DropListActivity::onExit() {
  rows = {};
  labels = {};
  paths = {};
  UiListActivity::onExit();
}

int DropListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* DropListActivity::headerTitle() const { return tr(STR_DROP_TITLE); }

void DropListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  if (index == 0) {
    if (url.empty()) return;
    if (auto fetch = makeUniqueNoThrow<FetchActivity>(renderer, mappedInput, url)) {
      activityManager.pushActivity(std::move(fetch));
    }
    return;
  }
  if (auto view = makeUniqueNoThrow<DropViewActivity>(renderer, mappedInput, paths[static_cast<size_t>(index - 1)])) {
    activityManager.pushActivity(std::move(view));
  }
}

void DropListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- view

void DropViewActivity::onEnter() {
  Activity::onEnter();
  HalFile file;
  if (inkhome::sdMounted() && Storage.openFileForRead("DRP", path.c_str(), file) && file.fileSize() <= MAX_FILE_BYTES) {
    std::string text(file.fileSize(), '\0');
    if (file.read(text.data(), text.size()) == static_cast<int>(text.size())) {
      sections.resize(MAX_SECTIONS);
      size_t count = 0;
      Collector sink(sections.data(), count, title);
      DropParser parser(sink);
      size_t pos = 0;
      bool ok = true;
      while (ok && pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        ok = parser.feedLine(line);
      }
      sections.resize(count);
      loaded = ok && parser.sawMagic() && !sections.empty();
    }
  }
  if (title.empty()) title = path.substr(path.find_last_of('/') + 1);
  requestUpdate();
}

void DropViewActivity::onExit() {
  sections = {};
  Activity::onExit();
}

void DropViewActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!loaded) return;
  if ((mappedInput.wasPressed(Button::Right) || mappedInput.wasPressed(Button::PageForward)) &&
      section + 1 < sections.size()) {
    ++section;
    scroll = 0;
    requestUpdate();
  } else if ((mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::PageBack)) && section > 0) {
    --section;
    scroll = 0;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Down) &&
             scroll + visibleLines < static_cast<int>(sections[section].lines.size())) {
    scroll += std::max(1, visibleLines - 2);
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Up) && scroll > 0) {
    scroll = std::max(0, scroll - std::max(1, visibleLines - 2));
    requestUpdate();
  }
}

void DropViewActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  char subtitle[64] = {};
  if (loaded) {
    snprintf(subtitle, sizeof(subtitle), "%s  %u/%u", sections[section].name.c_str(),
             static_cast<unsigned>(section + 1), static_cast<unsigned>(sections.size()));
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title.c_str(), subtitle);
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 2;
  visibleLines = std::max(1, (bottom - top) / lineH);

  if (!loaded) {
    renderer.drawCenteredText(UI_12_FONT_ID, (top + bottom) / 2, tr(STR_DROP_LOAD_FAILED), true, EpdFontFamily::BOLD);
  } else {
    int y = top;
    const auto& lines = sections[section].lines;
    for (int i = scroll; i < static_cast<int>(lines.size()) && y + lineH <= bottom; ++i) {
      const auto& line = lines[static_cast<size_t>(i)];
      if (line.kind == Line::Kind::Rule) {
        renderer.drawLine(side, y + lineH / 2, width - side, y + lineH / 2);
      } else {
        const bool heading = line.kind == Line::Kind::Heading;
        const std::string fitted = renderer.truncatedText(UI_10_FONT_ID, line.text.c_str(), width - side * 2,
                                                          heading ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
        renderer.drawText(UI_10_FONT_ID, side, y, fitted.c_str(), true,
                          heading ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      }
      y += lineH;
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", loaded && section > 0 ? tr(STR_RECIPES_PREV) : "",
                                            loaded && section + 1 < sections.size() ? tr(STR_RECIPES_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

// ---------------------------------------------------------------- fetch

void FetchActivity::onEnter() {
  Activity::onEnter();
  // The same flow as CrossPoint's update screen: pick or join a saved network first.
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           state = result.isCancelled ? State::Failed : State::Downloading;
                           started = false;
                         });
}

void FetchActivity::onExit() {
  Activity::onExit();
  // Free the network stack: Wi-Fi off and a quiet restart, as CrossPoint's Wi-Fi screens do.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void FetchActivity::download() {
  Storage.mkdir(APP_DIR, true);
  const std::string dest = std::string(APP_DIR) + "/" + todayFileName();
  const std::string tmp = dest + ".tmp";
  const auto err = HttpDownloader::downloadToFile(url, tmp);
  bool ok = err == HttpDownloader::OK;
  if (ok) {
    // Only keep a file that really is a digest.
    HalFile check;
    char head[7] = {};
    ok =
        Storage.openFileForRead("DRP", tmp.c_str(), check) && check.read(head, 6) == 6 && std::string(head) == "DROP 1";
  }
  if (ok) {
    if (Storage.exists(dest.c_str())) Storage.remove(dest.c_str());
    ok = Storage.rename(tmp.c_str(), dest.c_str());
  } else if (Storage.exists(tmp.c_str())) {
    Storage.remove(tmp.c_str());
  }
  LOG_INF("DROP", "Fetch %s: %s", url.c_str(), ok ? "ok" : "failed");
  state = ok ? State::Done : State::Failed;
}

void FetchActivity::loop() {
  if (state == State::Downloading && !started) {
    started = true;
    requestUpdateAndWait();  // show "Downloading" before the blocking transfer
    download();
    requestUpdate();
    return;
  }
  if (state != State::Downloading && mappedInput.wasReleased(MappedInputManager::Button::Back)) finish();
  if (state == State::Done && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) finish();
}

void FetchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_DROP_TITLE));
  const int mid = renderer.getScreenHeight() / 2;
  const StrId message = state == State::Connecting    ? StrId::STR_DROP_CONNECTING
                        : state == State::Downloading ? StrId::STR_DROP_DOWNLOADING
                        : state == State::Done        ? StrId::STR_DROP_DONE
                                                      : StrId::STR_DROP_FAILED;
  renderer.drawCenteredText(UI_12_FONT_ID, mid, I18N.get(message), true, EpdFontFamily::BOLD);
  if (state == State::Done || state == State::Failed) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), state == State::Done ? tr(STR_DROP_OK) : "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}

}  // namespace dailydrop
