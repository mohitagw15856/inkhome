#include "InkcardsApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "apps/AppPaths.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "core/BuiltInDecks.h"
#include "core/DeckReader.h"
#include "core/ReviewState.h"
#include "core/Session.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkcards {

namespace {

constexpr char APP_ID[] = "inkcards";
constexpr char LEGACY_DIR[] = "/inkcards/decks";
constexpr char BUILTIN_DEMO[] = "builtin:demo";
constexpr char BUILTIN_CAPITALS[] = "builtin:capitals";
constexpr char STATE_DIR[] = "/.crosspoint/apps/inkcards";
// Without a clock, each new session advances a stored pseudo-day.
constexpr char PSEUDO_DAY_PATH[] = "/.crosspoint/apps/inkcards/day.txt";
constexpr size_t MAX_DECKS = 50;

#ifndef OMIT_FONTS
constexpr int CARD_FONT_ID = NOTOSANS_18_FONT_ID;
#else
constexpr int CARD_FONT_ID = UI_12_FONT_ID;
#endif

// ByteReader over an open SD file (DeckReader seeks per card).
class FileReader final : public ByteReader {
 public:
  explicit FileReader(HalFile& f) : file(f), length(static_cast<uint32_t>(f.fileSize())) {}
  bool seek(const uint32_t pos) override {
    if (pos > length || !file.seek(pos)) return false;
    cursor = pos;
    return true;
  }
  uint32_t position() const override { return cursor; }
  uint32_t size() const override { return length; }
  size_t read(void* dst, const size_t n) override {
    const int got = file.read(dst, n);
    if (got <= 0) return 0;
    cursor += static_cast<uint32_t>(got);
    return static_cast<size_t>(got);
  }

 private:
  HalFile& file;
  uint32_t length;
  uint32_t cursor = 0;
};

class StringWriter final : public ByteWriter {
 public:
  size_t write(const void* src, const size_t n) override {
    out.append(static_cast<const char*>(src), n);
    return n;
  }
  std::string out;
};

std::string statePath(const DeckEntry& e) { return std::string(STATE_DIR) + "/" + e.stateKey + ".rev"; }

uint32_t studyDay() {
  const uint32_t today = inkhome::stats::todayOrdinal();
  if (today != 0) return today;
  // No clock: one "day" per study session.
  uint32_t day = 1;
  if (inkhome::sdMounted() && Storage.exists(PSEUDO_DAY_PATH)) {
    day = static_cast<uint32_t>(strtoul(Storage.readFile(PSEUDO_DAY_PATH).c_str(), nullptr, 10)) + 1;
  }
  if (inkhome::sdMounted()) {
    Storage.mkdir(STATE_DIR, true);
    Storage.writeFile(PSEUDO_DAY_PATH, String(day));
  }
  return day;
}

void addFolder(const char* dir, std::vector<DeckEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_DECKS)) {
    if (out.size() >= MAX_DECKS) return;
    std::string name = e.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() < 6 || name[0] == '.' || name.compare(name.size() - 5, 5, ".deck") != 0) continue;
    const std::string base = name.substr(0, name.size() - 5);
    out.push_back({base, std::string(dir) + "/" + name, base});
  }
}

}  // namespace

// Deck, review state and session for one study run.
struct StudyActivity::State {
  HalFile file;
  std::unique_ptr<FileReader> fileReader;
  std::unique_ptr<MemoryReader> memoryReader;
  std::unique_ptr<DeckReader> deck;
  ReviewStore store;
  std::unique_ptr<Session> session;
  DeckCard card;
  uint32_t day = 0;
  bool dirty = false;
};

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DeckListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- deck list

void DeckListActivity::onEnter() {
  UiListActivity::onEnter();
  entries.clear();
  entries.reserve(8);
  entries.push_back({tr(STR_CARDS_BUILTIN_DEMO), BUILTIN_DEMO, "builtin-demo"});
  entries.push_back({tr(STR_CARDS_BUILTIN_CAPITALS), BUILTIN_CAPITALS, "builtin-capitals"});
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

void DeckListActivity::onExit() {
  rows = {};
  entries = {};
  UiListActivity::onExit();
}

int DeckListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* DeckListActivity::headerTitle() const { return tr(STR_CARDS_TITLE); }

void DeckListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto study = makeUniqueNoThrow<StudyActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!study) return;
  app.clearTapFlash();
  activityManager.pushActivity(std::move(study));
}

void DeckListActivity::buildScreen(UiScreen& screen) {
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

// ---------------------------------------------------------------- study

StudyActivity::StudyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, DeckEntry entry)
    : Activity("InkcardsStudy", renderer, mappedInput), entry(std::move(entry)) {}

StudyActivity::~StudyActivity() = default;

void StudyActivity::onEnter() {
  Activity::onEnter();
  state = makeUniqueNoThrow<State>();
  failed = !state;
  if (!failed) {
    if (entry.source == BUILTIN_DEMO) {
      state->memoryReader = makeUniqueNoThrow<MemoryReader>(BUILT_IN_DEMO, BUILT_IN_DEMO_SIZE);
    } else if (entry.source == BUILTIN_CAPITALS) {
      state->memoryReader = makeUniqueNoThrow<MemoryReader>(BUILT_IN_CAPITALS, BUILT_IN_CAPITALS_SIZE);
    } else if (inkhome::sdMounted() && Storage.openFileForRead("ICD", entry.source.c_str(), state->file)) {
      state->fileReader = makeUniqueNoThrow<FileReader>(state->file);
    }
    ByteReader* reader = state->memoryReader ? static_cast<ByteReader*>(state->memoryReader.get())
                                             : static_cast<ByteReader*>(state->fileReader.get());
    if (reader) state->deck = makeUniqueNoThrow<DeckReader>(*reader);
    failed = !state->deck || !state->deck->open();
  }
  if (!failed) {
    // Review state, if any (the file is small: a few bytes per card seen).
    HalFile revFile;
    if (inkhome::sdMounted() && Storage.openFileForRead("ICD", statePath(entry).c_str(), revFile)) {
      // Binary: read it whole rather than through String, which stops at a zero byte.
      std::string raw(revFile.fileSize(), '\0');
      if (revFile.read(raw.data(), raw.size()) == static_cast<int>(raw.size())) {
        MemoryReader r(reinterpret_cast<const uint8_t*>(raw.data()), static_cast<uint32_t>(raw.size()));
        if (!state->store.load(r)) state->store.reset();
      }
    }
    state->day = studyDay();
    state->session = makeUniqueNoThrow<Session>(*state->deck, state->store, SessionConfig{});
    failed = !state->session || !state->session->build(state->day);
  }
  if (!failed) loadCurrent();
  requestUpdate();
}

void StudyActivity::onExit() {
  save();
  state.reset();
  Activity::onExit();
}

void StudyActivity::save() {
  if (!state || !state->dirty || !inkhome::sdMounted()) return;
  StringWriter w;
  if (!state->store.save(w)) return;
  Storage.mkdir(STATE_DIR, true);
  const std::string path = statePath(entry);
  const std::string tmp = path + ".tmp";
  HalFile out;
  if (!Storage.openFileForWrite("ICD", tmp.c_str(), out)) return;
  const bool ok = out.write(w.out.data(), w.out.size()) == w.out.size();
  out.close();
  if (!ok) return;
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
  Storage.rename(tmp.c_str(), path.c_str());
  state->dirty = false;
}

void StudyActivity::loadCurrent() {
  revealed = false;
  if (state->session->hasCurrent()) state->deck->readCard(state->session->currentIndex(), state->card);
}

void StudyActivity::grade(const uint8_t g) {
  state->session->grade(static_cast<Grade>(g), state->day);
  state->dirty = true;
  loadCurrent();
  if (!state->session->hasCurrent()) save();
  requestUpdate();
}

void StudyActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (failed || !state->session->hasCurrent()) return;
  if (!revealed) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      revealed = true;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasReleased(Button::Left)) {
    grade(GRADE_AGAIN);
  } else if (mappedInput.wasReleased(Button::Down) || mappedInput.wasReleased(Button::PageBack)) {
    grade(GRADE_HARD);
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    grade(GRADE_GOOD);
  } else if (mappedInput.wasReleased(Button::Right)) {
    grade(GRADE_EASY);
  }
}

void StudyActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int side = metrics.contentSidePadding;
  const int textWidth = width - side * 2;

  char subtitle[48] = {};
  if (!failed) {
    const SessionStats s = state->session->stats();
    snprintf(subtitle, sizeof(subtitle), tr(STR_CARDS_LEFT_FMT), static_cast<unsigned>(s.remaining),
             static_cast<unsigned>(s.streakDays));
  }
  const char* title =
      !failed && !state->deck->header().name.empty() ? state->deck->header().name.c_str() : entry.label.c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title, subtitle);

  const char* confirm = "";
  const char* left = "";
  const char* right = "";
  if (failed) {
    renderer.drawCenteredText(UI_12_FONT_ID, height / 2, tr(STR_CARDS_LOAD_FAILED), true, EpdFontFamily::BOLD);
  } else if (!state->session->hasCurrent()) {
    const SessionStats s = state->session->stats();
    char line[64];
    snprintf(line, sizeof(line), tr(STR_CARDS_DONE_FMT), static_cast<unsigned>(s.reviewedToday));
    renderer.drawCenteredText(UI_12_FONT_ID, height / 2 - renderer.getLineHeight(UI_12_FONT_ID), tr(STR_CARDS_DONE),
                              true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, height / 2 + 6, line, true);
  } else {
    // Front centred in large type; after Confirm, the answer and any pinyin/example below a rule.
    const auto& card = state->card;
    int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 3;
    const int bigH = renderer.getLineHeight(CARD_FONT_ID);
    for (const auto& line : renderer.wrappedText(CARD_FONT_ID, card.front.c_str(), textWidth, 4, EpdFontFamily::BOLD)) {
      renderer.drawCenteredText(CARD_FONT_ID, y, line.c_str(), true, EpdFontFamily::BOLD);
      y += bigH;
    }
    if (revealed) {
      y += bigH / 2;
      renderer.drawLine(side, y, width - side, y);
      y += bigH / 2;
      if (!card.pinyin.empty()) {
        renderer.drawCenteredText(UI_12_FONT_ID, y, card.pinyin.c_str(), true, EpdFontFamily::ITALIC);
        y += renderer.getLineHeight(UI_12_FONT_ID) + 4;
      }
      for (const auto& line : renderer.wrappedText(UI_12_FONT_ID, card.back.c_str(), textWidth, 6)) {
        renderer.drawCenteredText(UI_12_FONT_ID, y, line.c_str(), true);
        y += renderer.getLineHeight(UI_12_FONT_ID);
      }
      if (!card.example.empty()) {
        y += 8;
        for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, card.example.c_str(), textWidth, 4)) {
          renderer.drawCenteredText(UI_10_FONT_ID, y, line.c_str(), true);
          y += renderer.getLineHeight(UI_10_FONT_ID);
        }
      }
      confirm = tr(STR_CARDS_GOOD);
      left = tr(STR_CARDS_AGAIN);
      right = tr(STR_CARDS_EASY);
      GUI.drawSideButtonHints(renderer, "", tr(STR_CARDS_HARD));
    } else {
      confirm = tr(STR_CARDS_SHOW);
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, left, right);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkcards
