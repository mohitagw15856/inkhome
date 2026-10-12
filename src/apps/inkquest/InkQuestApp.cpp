#include "InkQuestApp.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <iterator>

#include "MappedInputManager.h"
#include "apps/AppPaths.h"
#include "apps/inkkit/HalFileReader.h"
#include "components/UITheme.h"
#include "core/BuiltInStory.h"
#include "core/SaveGame.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkquest {

namespace {

constexpr char APP_ID[] = "inkquest";
constexpr char LEGACY_DIR[] = "/inkquest/stories";
constexpr char STATE_DIR[] = "/.crosspoint/apps/inkquest";
constexpr size_t MAX_STORIES = 40;
constexpr size_t MAX_SAVE_BYTES = 8192;
constexpr int MARGIN = 20;
constexpr int MAX_CHOICE_LINES = 2;

#ifndef OMIT_FONTS
constexpr int BODY_FONT_ID = NOTOSERIF_14_FONT_ID;
#else
constexpr int BODY_FONT_ID = UI_12_FONT_ID;
#endif

void addFolder(const char* dir, std::vector<StoryEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_STORIES)) {
    if (out.size() >= MAX_STORIES) return;
    std::string name = e.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() < 5 || name[0] == '.' || name.compare(name.size() - 4, 4, ".iqs") != 0) continue;
    out.push_back({name.substr(0, name.size() - 4), std::string(dir) + "/" + name});
  }
}

std::string builtInTitle() {
  inkkit::MemoryReader mem(BUILT_IN_STORY, BUILT_IN_STORY_SIZE);
  StoryEngine probe;
  return probe.open(mem) && !probe.meta().title.empty() ? probe.meta().title : std::string("The Lighthouse Keeper");
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<StoryListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- list

void StoryListActivity::onEnter() {
  UiListActivity::onEnter();
  entries.clear();
  entries.push_back({builtInTitle(), ""});
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

void StoryListActivity::onExit() {
  rows = {};
  entries = {};
  UiListActivity::onExit();
}

int StoryListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* StoryListActivity::headerTitle() const { return tr(STR_QUEST_TITLE); }

void StoryListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto play = makeUniqueNoThrow<PlayActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!play) return;
  app.clearTapFlash();
  activityManager.pushActivity(std::move(play));
}

void StoryListActivity::buildScreen(UiScreen& screen) {
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

// ---------------------------------------------------------------- play

PlayActivity::PlayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, StoryEntry entry)
    : Activity("InkQuestPlay", renderer, mappedInput), entry(std::move(entry)) {}

PlayActivity::~PlayActivity() = default;

std::string PlayActivity::savePath() const {
  std::string name = engine.meta().uid.empty() ? entry.label : engine.meta().uid;
  std::transform(name.begin(), name.end(), name.begin(), [](const char c) {
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
    return safe ? c : '_';
  });
  if (name.size() > 40) name.resize(40);
  return std::string(STATE_DIR) + "/" + name + ".sav";
}

void PlayActivity::onEnter() {
  Activity::onEnter();
  if (entry.path.empty()) {
    reader = makeUniqueNoThrow<inkkit::MemoryReader>(BUILT_IN_STORY, BUILT_IN_STORY_SIZE);
  } else if (inkhome::sdMounted() && Storage.openFileForRead("IQS", entry.path.c_str(), file)) {
    reader = makeUniqueNoThrow<inkkit::HalFileReader>(file);
  }
  loaded = reader && engine.open(*reader) && engine.passageCount() > 0;
  if (!loaded) {
    LOG_ERR("IQS", "Could not open %s", entry.path.empty() ? "built-in story" : entry.path.c_str());
  }

  bool resumed = false;
  if (loaded && inkhome::sdMounted()) {
    HalFile saveFile;
    if (Storage.openFileForRead("IQS", savePath().c_str(), saveFile)) {
      const size_t size = std::min<size_t>(saveFile.fileSize(), MAX_SAVE_BYTES);
      std::vector<uint8_t> bytes(size);
      if (size > 0 && saveFile.read(bytes.data(), size) == static_cast<int>(size)) {
        inkkit::MemoryReader mem(bytes);
        resumed = readSave(engine, mem) && engine.loadPassage(engine.currentPassage(), passage);
      }
    }
  }
  if (loaded && !resumed) restart();
  layoutPassage();
  fullRefresh = true;
  requestUpdate();
}

void PlayActivity::onExit() {
  save();
  passage = PassageView{};
  lines = {};
  visibleChoices = {};
  reader.reset();
  file.close();
  Activity::onExit();
}

void PlayActivity::save() const {
  if (!loaded || !inkhome::sdMounted()) return;
  const std::string path = savePath();
  if (ended) {
    if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
    return;
  }
  inkkit::MemoryWriter writer;
  writeSave(engine, writer, 0, true);
  Storage.mkdir(STATE_DIR, true);
  HalFile out;
  if (!Storage.openFileForWrite("IQS", path, out)) return;
  out.write(writer.bytes().data(), writer.bytes().size());
}

void PlayActivity::restart() {
  engine.reset();
  ended = !engine.enter(engine.currentPassage(), passage);
}

void PlayActivity::layoutPassage() {
  lines.clear();
  visibleChoices.clear();
  page = 0;
  selected = 0;
  if (!loaded) return;

  for (size_t i = 0; i < passage.choices.size(); ++i) {
    if (engine.choiceVisible(passage.choices[i])) visibleChoices.push_back(static_cast<uint16_t>(i));
  }
  if (visibleChoices.empty()) ended = true;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int textWidth = renderer.getScreenWidth() - MARGIN * 2;
  const std::string text = engine.visibleText(passage);
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string paragraph = text.substr(pos, end - pos);
    if (paragraph.find_first_not_of(" \t\r") == std::string::npos) {
      if (!lines.empty() && !lines.back().empty()) lines.emplace_back();
    } else {
      auto wrapped = renderer.wrappedText(BODY_FONT_ID, paragraph.c_str(), textWidth, 200);
      lines.insert(lines.end(), std::make_move_iterator(wrapped.begin()), std::make_move_iterator(wrapped.end()));
    }
    if (end == text.size()) break;
    pos = end + 1;
  }
  while (!lines.empty() && lines.back().empty()) lines.pop_back();

  // Space below the text holds the choices (or the ending line).
  const int choiceLineH = renderer.getLineHeight(UI_12_FONT_ID);
  int choiceH = choiceLineH + metrics.verticalSpacing;
  if (!ended) {
    choiceH = 0;
    for (const uint16_t i : visibleChoices) {
      const auto wrapped = renderer.wrappedText(UI_12_FONT_ID, passage.choices[i].text.c_str(),
                                                renderer.getScreenWidth() - MARGIN * 3, MAX_CHOICE_LINES);
      choiceH += static_cast<int>(std::max<size_t>(1, wrapped.size())) * choiceLineH + metrics.verticalSpacing;
    }
  }
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int textArea = bottom - top - choiceH - metrics.verticalSpacing * 2;
  linesPerPage = std::max(3, textArea / renderer.getLineHeight(BODY_FONT_ID));
  pageCount = std::max(1, (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage);
}

void PlayActivity::takeChoice() {
  if (selected < 0 || selected >= static_cast<int>(visibleChoices.size())) return;
  {
    RenderLock lock(*this);
    const Choice choice = passage.choices[visibleChoices[static_cast<size_t>(selected)]];
    ended = !engine.choose(choice, passage);
    layoutPassage();
    fullRefresh = true;
  }
  save();
  requestUpdate();
}

void PlayActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!loaded) return;

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (ended) {
      {
        RenderLock lock(*this);
        restart();
        layoutPassage();
        fullRefresh = true;
      }
      save();
      requestUpdate();
    } else if (page + 1 < pageCount) {
      page++;
      requestUpdate();
    } else {
      takeChoice();
    }
    return;
  }

  const int count = static_cast<int>(visibleChoices.size());
  const bool next = mappedInput.wasPressed(Button::Down);
  const bool prev = mappedInput.wasPressed(Button::Up);
  const bool right = mappedInput.wasPressed(Button::Right);
  const bool left = mappedInput.wasPressed(Button::Left);
  if (pageCount > 1 && (right || left)) {
    const int target = std::clamp(page + (right ? 1 : -1), 0, pageCount - 1);
    if (target != page) {
      page = target;
      requestUpdate();
    }
  } else if (count > 0 && (next || prev || right || left)) {
    selected = (selected + ((next || right) ? 1 : count - 1)) % count;
    requestUpdate();
  }
}

void PlayActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const char* title = loaded && !engine.meta().title.empty() ? engine.meta().title.c_str() : entry.label.c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title);

  if (!loaded) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_QUEST_LOAD_FAILED), true,
                              EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }

  const int lineH = renderer.getLineHeight(BODY_FONT_ID);
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const size_t first = static_cast<size_t>(page * linesPerPage);
  for (size_t i = first; i < lines.size() && i < first + static_cast<size_t>(linesPerPage); ++i) {
    if (!lines[i].empty()) renderer.drawText(BODY_FONT_ID, MARGIN, y, lines[i].c_str(), true);
    y += lineH;
  }
  y += metrics.verticalSpacing * 2;

  const int choiceLineH = renderer.getLineHeight(UI_12_FONT_ID);
  if (ended) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_QUEST_END), true, EpdFontFamily::BOLD);
  } else {
    for (size_t c = 0; c < visibleChoices.size(); ++c) {
      const auto wrapped = renderer.wrappedText(UI_12_FONT_ID, passage.choices[visibleChoices[c]].text.c_str(),
                                                width - MARGIN * 3, MAX_CHOICE_LINES);
      const int h = static_cast<int>(std::max<size_t>(1, wrapped.size())) * choiceLineH;
      const bool isSelected = static_cast<int>(c) == selected;
      if (isSelected) renderer.fillRect(MARGIN / 2, y - 2, width - MARGIN, h + 4, true);
      int ly = y;
      for (const auto& l : wrapped) {
        renderer.drawText(UI_12_FONT_ID, MARGIN + MARGIN / 2, ly, l.c_str(), !isSelected);
        ly += choiceLineH;
      }
      y += h + metrics.verticalSpacing;
    }
  }

  char pos[16] = {};
  if (pageCount > 1) snprintf(pos, sizeof(pos), "%d / %d", page + 1, pageCount);
  const char* confirm = ended                  ? tr(STR_QUEST_AGAIN)
                        : page + 1 < pageCount ? tr(STR_RECIPES_NEXT)
                                               : tr(STR_QUEST_CHOOSE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, pageCount > 1 ? pos : "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(fullRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  fullRefresh = false;
}

}  // namespace inkquest
