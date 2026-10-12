#include "InkWriterApp.h"

#include <Arduino.h>
#include <BleKeyboardHost.h>
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
#include "core/WrapRows.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkwriter {

namespace {

constexpr char APP_ID[] = "inkwriter";
constexpr char DRAFTS_DIR[] = "/apps/inkwriter/drafts";
constexpr char LEGACY_DIR[] = "/inkwriter/drafts";
constexpr char CONFIG_PATH[] = "/apps/inkwriter/config.txt";
constexpr char STREAK_PATH[] = "/.crosspoint/apps/inkwriter/streak.txt";
constexpr size_t MAX_DRAFTS = 60;
// Larger drafts are refused rather than opened in part, so a save can never
// cut a file short.
constexpr size_t MAX_DRAFT_BYTES = 48 * 1024;
constexpr uint32_t AUTOSAVE_MS = 5000;
constexpr uint32_t QUIET_MS = 150;              // repaint once typing pauses
constexpr uint32_t KEEP_AWAKE_MS = 10 * 60000;  // after the last key
constexpr int FAST_REFRESHES_PER_CLEAN = 30;
constexpr size_t LOOKBACK_LINES = 60;  // bounds the wrapping work per repaint
constexpr int MARGIN = 18;
constexpr int ROW_ACTION_NEW = 0;
constexpr int ROW_ACTION_KEYBOARD = 1;
constexpr int FIRST_DRAFT_ROW = 2;

#ifndef OMIT_FONTS
constexpr int TEXT_FONT_ID = NOTOSERIF_14_FONT_ID;
#else
constexpr int TEXT_FONT_ID = UI_12_FONT_ID;
#endif

freeink::BleKeyboardHost& ble() { return freeink::BleKeyboardHost::getInstance(); }

int measureBytes(const void* ctx, const char* text, const size_t len) {
  char buf[256];
  const size_t n = std::min(len, sizeof(buf) - 1);
  memcpy(buf, text, n);
  buf[n] = '\0';
  return static_cast<const GfxRenderer*>(ctx)->getTextWidth(TEXT_FONT_ID, buf);
}

std::string baseName(const std::string& path) {
  const auto slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

void addDrafts(const char* dir, std::vector<std::string>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_DRAFTS)) {
    if (out.size() >= MAX_DRAFTS) return;
    const std::string name = baseName(e.c_str());
    if (name.size() < 4 || name[0] == '.') continue;
    const std::string ext = name.substr(name.size() - 3);
    if (ext != ".md" && name.compare(name.size() - 4, 4, ".txt") != 0) continue;
    out.push_back(std::string(dir) + "/" + name);
  }
}

std::string newDraftPath() {
  char path[64];
  for (int n = 1; n < 1000; ++n) {
    snprintf(path, sizeof(path), "%s/draft-%03d.md", DRAFTS_DIR, n);
    if (!Storage.exists(path)) return path;
  }
  return std::string(DRAFTS_DIR) + "/draft-overflow.md";
}

bool readSmallFile(const char* path, std::string& out, const size_t cap) {
  HalFile file;
  if (!Storage.openFileForRead("IW", path, file)) return false;
  const size_t size = file.fileSize();
  if (size > cap) return false;
  out.assign(size, '\0');
  return size == 0 || file.read(out.data(), size) == static_cast<int>(size);
}

void drawHints(GfxRenderer& renderer, const MappedInputManager& mappedInput, const char* back, const char* confirm) {
  const auto labels = mappedInput.mapLabels(back, confirm, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DraftListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- drafts

void DraftListActivity::onEnter() {
  UiListActivity::onEnter();
  if (!ble().isRunning()) {
    const uint32_t before = ESP.getFreeHeap();
    bleStarted = ble().begin("Inkhome");
    if (!bleStarted) LOG_ERR("IW", "Bluetooth could not start");
    LOG_INF("IW", "Free heap %lu before Bluetooth, %lu after", static_cast<unsigned long>(before),
            static_cast<unsigned long>(ESP.getFreeHeap()));
  }
  refresh();
}

void DraftListActivity::onExit() {
  if (ble().isRunning()) ble().end();
  bleStarted = false;
  rows = {};
  names = {};
  paths = {};
  UiListActivity::onExit();
}

void DraftListActivity::refresh() {
  paths.clear();
  if (inkhome::sdMounted()) {
    inkhome::apps::ensureDirs(APP_ID);
    Storage.mkdir(DRAFTS_DIR, true);
    addDrafts(DRAFTS_DIR, paths);
    addDrafts(LEGACY_DIR, paths);
    std::sort(paths.begin(), paths.end(),
              [](const std::string& a, const std::string& b) { return baseName(a) > baseName(b); });
  }
  names.clear();
  names.reserve(paths.size());
  std::transform(paths.begin(), paths.end(), std::back_inserter(names), baseName);

  wasConnected = ble().isConnected();
  keyboardLabel = tr(STR_WRITER_KEYBOARD);
  keyboardLabel += ": ";
  if (!ble().isRunning()) {
    keyboardLabel += tr(STR_WRITER_BLE_FAILED);
  } else if (wasConnected) {
    keyboardLabel += ble().connectedName();
  } else {
    keyboardLabel += tr(STR_WRITER_NOT_CONNECTED);
  }

  rows.assign(paths.size() + FIRST_DRAFT_ROW, fui::ListItem{});
  rows[0].label = tr(STR_WRITER_NEW);
  rows[0].actionValue = ROW_ACTION_NEW;
  rows[0].enabled = inkhome::sdMounted();
  rows[1].label = keyboardLabel.c_str();
  rows[1].actionValue = ROW_ACTION_KEYBOARD;
  rows[1].enabled = ble().isRunning();
  for (size_t i = 0; i < names.size(); ++i) {
    rows[i + FIRST_DRAFT_ROW].label = names[i].c_str();
    rows[i + FIRST_DRAFT_ROW].actionValue = static_cast<int16_t>(i + FIRST_DRAFT_ROW);
  }
}

void DraftListActivity::loop() {
  ble().poll();
  freeink::KeyEvent ev;
  while (ble().popKey(ev)) {
  }
  if (ble().isConnected() != wasConnected) {
    RenderLock lock(*this);
    refresh();
    requestUpdate();
  }
  UiListActivity::loop();
}

int DraftListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* DraftListActivity::headerTitle() const { return tr(STR_WRITER_TITLE); }

void DraftListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount() || !rows[static_cast<size_t>(index)].enabled) return;
  app.clearTapFlash();
  if (index == ROW_ACTION_KEYBOARD) {
    auto pair = makeUniqueNoThrow<PairActivity>(renderer, mappedInput);
    if (pair) activityManager.pushActivity(std::move(pair));
    return;
  }
  const std::string path =
      index == ROW_ACTION_NEW ? newDraftPath() : paths[static_cast<size_t>(index - FIRST_DRAFT_ROW)];
  auto editor = makeUniqueNoThrow<EditorActivity>(renderer, mappedInput, path);
  if (editor) activityManager.pushActivity(std::move(editor));
}

void DraftListActivity::buildScreen(UiScreen& screen) {
  if (paths.size() + FIRST_DRAFT_ROW != rows.size()) refresh();  // back from the editor with a new draft
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

// ---------------------------------------------------------------- pairing

void PairActivity::onEnter() {
  Activity::onEnter();
  rescan();
}

void PairActivity::onExit() {
  ble().stopScan();
  ble().releaseScanResults();
  Activity::onExit();
}

void PairActivity::rescan() {
  state = State::Scanning;
  selected = 0;
  shownCount = 0;
  hasPasskey = false;
  failure[0] = '\0';
  ble().startScan(6000);
  requestUpdate();
}

void PairActivity::loop() {
  using Button = MappedInputManager::Button;
  ble().poll();
  freeink::KeyEvent ev;
  while (ble().popKey(ev)) {
  }

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  bool dirty = false;
  if (state == State::Scanning && (!ble().isScanning() || ble().deviceCount() != shownCount)) {
    shownCount = ble().deviceCount();
    if (!ble().isScanning()) state = State::Choosing;
    dirty = true;
  }
  if (state == State::Connecting) {
    uint32_t key = 0;
    if (ble().takePairingPasskey(key)) {
      passkey = key;
      hasPasskey = true;
      dirty = true;
    }
    if (ble().isConnected()) {
      state = State::Connected;
      ble().releaseScanResults();
      dirty = true;
    } else if (ble().takeConnectFailure(failure, sizeof(failure))) {
      state = State::Failed;
      dirty = true;
    }
  }

  const bool canChoose = (state == State::Scanning || state == State::Choosing) && shownCount > 0;
  if (canChoose && mappedInput.wasPressed(Button::Down)) {
    selected = (selected + 1) % shownCount;
    dirty = true;
  } else if (canChoose && mappedInput.wasPressed(Button::Up)) {
    selected = (selected + shownCount - 1) % shownCount;
    dirty = true;
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    if (state == State::Connected) {
      finish();
      return;
    }
    if (canChoose && selected < shownCount) {
      ble().stopScan();
      if (ble().connect(ble().device(static_cast<uint8_t>(selected)).addr)) {
        state = State::Connecting;
      } else {
        state = State::Failed;
      }
      dirty = true;
    } else if (state == State::Failed || state == State::Choosing) {
      rescan();
      return;
    }
  }
  if (dirty) requestUpdate();
}

void PairActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_WRITER_PAIR_TITLE));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);

  auto message = [&](const char* text, const bool bold) {
    for (const auto& l : renderer.wrappedText(UI_12_FONT_ID, text, width - MARGIN * 2, 4,
                                              bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR)) {
      renderer.drawText(UI_12_FONT_ID, MARGIN, y, l.c_str(), true, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      y += lineH;
    }
    y += metrics.verticalSpacing;
  };

  const char* confirm = "";
  switch (state) {
    case State::Scanning:
    case State::Choosing: {
      if (state == State::Scanning) message(tr(STR_WRITER_SCANNING), false);
      if (shownCount == 0 && state == State::Choosing) message(tr(STR_WRITER_NONE_FOUND), false);
      for (uint8_t i = 0; i < shownCount; ++i) {
        const auto& d = ble().device(i);
        const bool isSelected = i == selected;
        if (isSelected) renderer.fillRect(MARGIN / 2, y - 2, width - MARGIN, lineH + 4, true);
        renderer.drawText(UI_12_FONT_ID, MARGIN, y, d.hasName ? d.name : d.addr, !isSelected);
        y += lineH + metrics.verticalSpacing;
      }
      confirm = shownCount > 0 ? tr(STR_WRITER_CONNECT) : (state == State::Choosing ? tr(STR_WRITER_RESCAN) : "");
      break;
    }
    case State::Connecting: {
      message(tr(STR_WRITER_CONNECTING), true);
      if (hasPasskey) {
        message(tr(STR_WRITER_PASSKEY), false);
        char code[12];
        snprintf(code, sizeof(code), "%06lu", static_cast<unsigned long>(passkey));
        renderer.drawCenteredText(UI_12_FONT_ID, y, code, true, EpdFontFamily::BOLD);
      }
      break;
    }
    case State::Connected:
      message(tr(STR_WRITER_CONNECTED), true);
      message(ble().connectedName(), false);
      confirm = tr(STR_DROP_OK);
      break;
    case State::Failed:
      message(tr(STR_WRITER_FAILED), true);
      if (failure[0]) message(failure, false);
      confirm = tr(STR_WRITER_RESCAN);
      break;
  }
  drawHints(renderer, mappedInput, tr(STR_BACK), confirm);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// ---------------------------------------------------------------- editor

EditorActivity::EditorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
    : Activity("InkWriterEditor", renderer, mappedInput), path(std::move(path)), draftName(baseName(this->path)) {}

void EditorActivity::loadConfig() {
  std::string text;
  if (readSmallFile(CONFIG_PATH, text, 1024)) {
    size_t pos = 0;
    while (pos < text.size()) {
      size_t end = text.find('\n', pos);
      if (end == std::string::npos) end = text.size();
      const std::string line = text.substr(pos, end - pos);
      if (line.rfind("goal=", 0) == 0) goalWords = std::max(1, atoi(line.c_str() + 5));
      if (line.rfind("sprint=", 0) == 0) sprintMinutes = std::clamp(atoi(line.c_str() + 7), 1, 180);
      pos = end + 1;
    }
  }
  if (readSmallFile(STREAK_PATH, text, 256)) streak = Streak::parse(text);
}

void EditorActivity::onEnter() {
  Activity::onEnter();
  loadConfig();
  std::string text;
  if (Storage.exists(path.c_str())) {
    if (!readSmallFile(path.c_str(), text, MAX_DRAFT_BYTES)) {
      LOG_ERR("IW", "Draft too large or unreadable: %s", path.c_str());
      path.clear();  // never overwrite what was not loaded
      refused = true;
    }
  }
  editor.setText(text);
  editor.clearDirty();
  sessionStartWords = editor.wordCount();
  lastSaveMs = millis();
  lastKeyMs = millis();
  wasConnected = ble().isConnected();
  requestUpdate();
}

void EditorActivity::onExit() {
  save();
  Activity::onExit();
}

bool EditorActivity::preventAutoSleep() { return millis() - lastKeyMs < KEEP_AWAKE_MS; }

void EditorActivity::save() {
  lastSaveMs = millis();
  if (path.empty() || !editor.dirty() || !inkhome::sdMounted()) return;
  Storage.mkdir(DRAFTS_DIR, true);
  const std::string text = editor.text();
  // Write beside the draft, then swap, so a power cut leaves the old copy.
  const std::string tmp = path + ".tmp";
  {
    HalFile out;
    if (!Storage.openFileForWrite("IW", tmp, out)) return;
    if (out.write(text.data(), text.size()) != text.size()) {
      LOG_ERR("IW", "Short write: %s", tmp.c_str());
      return;
    }
  }
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
  if (!Storage.rename(tmp.c_str(), path.c_str())) {
    LOG_ERR("IW", "Could not replace %s", path.c_str());
    return;
  }
  editor.clearDirty();
}

void EditorActivity::noteWriting() {
  if (wroteToday) return;
  wroteToday = true;
  const uint32_t day = inkhome::stats::todayOrdinal();
  if (day == 0 || !inkhome::sdMounted()) return;  // no clock yet
  streak.recordWriting(day);
  Storage.mkdir("/.crosspoint/apps/inkwriter", true);
  Storage.writeFile(STREAK_PATH, String(streak.serialise().c_str()));
}

void EditorActivity::loop() {
  using Button = MappedInputManager::Button;
  ble().poll();

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (refused) return;
  if (mappedInput.wasReleased(Button::Confirm)) {
    if (sprint.running()) {
      sprint.stop();
    } else {
      sprint.start(millis(), sprintMinutes * 60000UL);
    }
    lastSprintMinute = 0;
    requestUpdate();
  }

  bool edited = false;
  bool typed = false;
  {
    RenderLock lock(*this);
    using SK = freeink::SpecialKey;
    freeink::KeyEvent ev;
    while (ble().popKey(ev)) {
      edited = true;
      switch (ev.special) {
        case SK::None:
          if (ev.ch) {
            editor.insertChar(ev.ch);
            typed = true;
          }
          break;
        case SK::Enter:
          editor.insertNewline();
          typed = true;
          break;
        case SK::Backspace:
          editor.backspace();
          typed = true;
          break;
        case SK::Left:
          editor.moveLeft();
          break;
        case SK::Right:
          editor.moveRight();
          break;
        case SK::Up:
          editor.moveUp();
          break;
        case SK::Down:
          editor.moveDown();
          break;
        case SK::Home:
          editor.moveHome();
          break;
        case SK::End:
          editor.moveEnd();
          break;
        case SK::PageUp:
          for (int i = 0; i < 10; ++i) editor.moveUp();
          break;
        case SK::PageDown:
          for (int i = 0; i < 10; ++i) editor.moveDown();
          break;
        case SK::Tab:
          focusMode = !focusMode;
          break;
        case SK::Escape:
          finish();
          return;
        default:
          break;
      }
    }
  }
  if (edited) {
    lastKeyMs = millis();
    pendingRender = true;
  }
  if (typed && !path.empty()) noteWriting();
  if (ble().isConnected() != wasConnected) {
    wasConnected = ble().isConnected();
    requestUpdate();
  }
  if (pendingRender && millis() - lastKeyMs > QUIET_MS) {
    pendingRender = false;
    requestUpdate();
  }
  if (editor.dirty() && millis() - lastSaveMs > AUTOSAVE_MS && millis() - lastKeyMs > QUIET_MS) save();
  if (sprint.running()) {
    const uint32_t minute = (sprint.remainingSeconds(millis()) + 59) / 60;
    if (minute != lastSprintMinute) {
      lastSprintMinute = minute;
      requestUpdate();
    }
  }
}

void EditorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, draftName.c_str());
  if (refused) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_WRITER_TOO_LARGE), true,
                              EpdFontFamily::BOLD);
    drawHints(renderer, mappedInput, tr(STR_BACK), "");
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }

  const int lineH = renderer.getLineHeight(TEXT_FONT_ID);
  const int statusH = renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - statusH - metrics.verticalSpacing;
  const int visible = std::max(1, (bottom - top) / lineH);
  const int textWidth = width - MARGIN * 2;
  const auto& lines = editor.lines();

  size_t first = 0;
  size_t last = lines.size() - 1;
  if (focusMode) editor.currentParagraph(first, last);
  const size_t cursorLine = editor.cursorLine();

  // Wrap a bounded stretch up to the cursor, then show the screenful that ends
  // on the cursor's row (or starts at the top when everything fits).
  const size_t from = cursorLine > first + LOOKBACK_LINES ? cursorLine - LOOKBACK_LINES : first;
  std::vector<Row> rows = wrapRows(lines, from, cursorLine, textWidth, &measureBytes, &renderer);
  const size_t cursorAt = cursorRow(rows, cursorLine, editor.cursorCol());
  const size_t skip = static_cast<int>(cursorAt) >= visible ? cursorAt - static_cast<size_t>(visible) + 1 : 0;
  if (cursorLine < last && static_cast<int>(rows.size() - skip) < visible) {
    const auto more = wrapRows(lines, cursorLine + 1, last, textWidth, &measureBytes, &renderer);
    rows.insert(rows.end(), more.begin(), more.begin() + std::min<size_t>(more.size(), visible));
  }

  int y = top;
  for (size_t i = skip; i < rows.size() && y + lineH <= bottom + lineH / 2; ++i) {
    const Row& r = rows[i];
    const std::string text = lines[r.line].substr(r.start, r.end - r.start);
    if (!text.empty()) renderer.drawText(TEXT_FONT_ID, MARGIN, y, text.c_str(), true);
    if (i == cursorAt) {
      const size_t col = std::min(editor.cursorCol(), r.end) - r.start;
      const int x = MARGIN + (col > 0 ? measureBytes(&renderer, lines[r.line].data() + r.start, col) : 0);
      renderer.fillRect(x, y + 2, 2, lineH - 4, true);
    }
    y += lineH;
  }

  // Status: words this session against the goal, sprint, streak, keyboard.
  const int sy = renderer.getScreenHeight() - metrics.buttonHintsHeight - statusH;
  renderer.fillRect(MARGIN, sy - 4, textWidth, 1, true);
  const int sessionWords = std::max(0, editor.wordCount() - sessionStartWords);
  char status[96];
  if (!ble().isConnected()) {
    snprintf(status, sizeof(status), "%s", tr(STR_WRITER_NO_KEYBOARD));
  } else if (sprint.running()) {
    snprintf(status, sizeof(status), "%d %s   %lu %s", editor.wordCount(), tr(STR_WRITER_WORDS),
             static_cast<unsigned long>(lastSprintMinute), tr(STR_WRITER_MIN_LEFT));
  } else if (streak.length > 0) {
    snprintf(status, sizeof(status), "%d %s   %lu %s", editor.wordCount(), tr(STR_WRITER_WORDS),
             static_cast<unsigned long>(streak.length), tr(STR_WRITER_STREAK));
  } else {
    snprintf(status, sizeof(status), "%d %s", editor.wordCount(), tr(STR_WRITER_WORDS));
  }
  // Eight-segment goal bar at the right of the status line.
  const int segW = 10;
  const int barW = 8 * (segW + 3);
  renderer.drawText(UI_10_FONT_ID, MARGIN, sy,
                    renderer.truncatedText(UI_10_FONT_ID, status, textWidth - barW - MARGIN).c_str(), true);
  const int segH = renderer.getLineHeight(UI_10_FONT_ID) / 2;
  const int eighths = goalEighths(sessionWords, goalWords);
  int sx = width - MARGIN - barW;
  for (int i = 0; i < 8; ++i, sx += segW + 3) {
    if (i < eighths) {
      renderer.fillRect(sx, sy + segH / 2, segW, segH, true);
    } else {
      renderer.drawRect(sx, sy + segH / 2, segW, segH, true);
    }
  }

  drawHints(renderer, mappedInput, path.empty() ? tr(STR_BACK) : tr(STR_WRITER_SAVE),
            sprint.running() ? tr(STR_WRITER_STOP) : tr(STR_WRITER_SPRINT));
  const bool clean = ++fastRefreshes >= FAST_REFRESHES_PER_CLEAN;
  if (clean) fastRefreshes = 0;
  renderer.displayBuffer(clean ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
}

}  // namespace inkwriter
