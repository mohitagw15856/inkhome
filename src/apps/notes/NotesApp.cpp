#include "NotesApp.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <iterator>

#include "BookmarkEntry.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "core/NotesFormat.h"
#include "inkhome/SdState.h"
#include "util/BookmarkFile.h"

namespace fui = freeink::ui;

namespace inkhome::notes {

namespace {

constexpr char NOTES_DIR[] = "/notes/inkhome";
constexpr size_t MAX_BOOKS = 60;

struct TitleAuthor {
  std::string title;
  std::string author;
};

TitleAuthor describe(const std::string& path) {
  const auto& recent = RECENT_BOOKS.getBooks();
  const auto r = std::find_if(recent.begin(), recent.end(), [&path](const auto& b) { return b.path == path; });
  if (r != recent.end()) return {r->title, r->author};
  if (stats::ensureLoaded() == stats::LoadState::Ready) {
    const auto& tracked = stats::data().books;
    const auto b = std::find_if(tracked.begin(), tracked.end(),
                                [&path](const auto& s) { return s.path == path && !s.title.empty(); });
    if (b != tracked.end()) return {b->title, b->author};
  }
  std::string name = path.substr(path.find_last_of('/') + 1);
  const auto dot = name.find_last_of('.');
  if (dot != std::string::npos) name.resize(dot);
  return {name, ""};
}

// Books worth checking: recent books and every book with reading stats.
std::vector<std::string> candidateBooks() {
  std::vector<std::string> paths;
  paths.reserve(MAX_BOOKS);
  auto add = [&paths](const std::string& p) {
    if (!p.empty() && paths.size() < MAX_BOOKS && std::find(paths.begin(), paths.end(), p) == paths.end()) {
      paths.push_back(p);
    }
  };
  for (const auto& r : RECENT_BOOKS.getBooks()) add(r.path);
  if (stats::ensureLoaded() == stats::LoadState::Ready) {
    for (const auto& b : stats::data().books) add(b.path);
  }
  return paths;
}

}  // namespace

int exportBook(const std::string& bookPath) {
  if (!sdMounted()) return 0;
  std::vector<BookmarkEntry> entries;
  if (!BookmarkFile::load(bookPath, entries) || entries.empty()) return 0;
  std::vector<Mark> marks;
  marks.reserve(entries.size());
  std::transform(entries.begin(), entries.end(), std::back_inserter(marks),
                 [](const BookmarkEntry& e) { return Mark{e.percentage, e.summary, e.name}; });
  std::sort(marks.begin(), marks.end(), [](const Mark& a, const Mark& b) { return a.fraction < b.fraction; });
  const TitleAuthor ta = describe(bookPath);
  Storage.mkdir(NOTES_DIR, true);
  const std::string path = std::string(NOTES_DIR) + "/" + fileNameFor(ta.title);
  if (!Storage.writeFile(path.c_str(), String(markdownFor(ta.title, ta.author, marks).c_str()))) {
    LOG_ERR("NOTES", "Could not write %s", path.c_str());
    return 0;
  }
  return static_cast<int>(marks.size());
}

void exportCurrentBook() {
  if (!APP_STATE.openEpubPath.empty()) exportBook(APP_STATE.openEpubPath);
}

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<NotesActivity>(renderer, mappedInput);
}

void NotesActivity::onEnter() {
  UiListActivity::onEnter();
  books.clear();
  if (sdMounted()) {
    for (const auto& path : candidateBooks()) {
      std::vector<BookmarkEntry> entries;
      if (!BookmarkFile::load(path, entries) || entries.empty()) continue;
      char summary[48];
      snprintf(summary, sizeof(summary), tr(STR_NOTES_COUNT_FMT), static_cast<unsigned>(entries.size()));
      books.push_back({path, describe(path).title, summary});
    }
  }
  rows.assign(books.size(), fui::ListItem{});
  for (size_t i = 0; i < books.size(); ++i) {
    rows[i].label = books[i].title.c_str();
    rows[i].subtitle = books[i].summary.c_str();
    rows[i].actionValue = static_cast<int16_t>(i);
  }
}

void NotesActivity::onExit() {
  rows = {};
  books = {};
  UiListActivity::onExit();
}

int NotesActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* NotesActivity::headerTitle() const { return tr(STR_NOTES_TITLE); }

void NotesActivity::activateIndex(int) {
  // Confirm writes every book's notes; the result shows in each row.
  int total = 0;
  for (size_t i = 0; i < books.size(); ++i) {
    const int n = exportBook(books[i].path);
    total += n;
    char summary[64];
    snprintf(summary, sizeof(summary), tr(STR_NOTES_EXPORTED_FMT), static_cast<unsigned>(n));
    books[i].summary = summary;
    rows[i].subtitle = books[i].summary.c_str();
  }
  LOG_INF("NOTES", "Exported %d bookmarks to %s", total, NOTES_DIR);
  requestUpdate();
}

void NotesActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), books.empty() ? "" : tr(STR_NOTES_EXPORT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void NotesActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  if (rows.empty()) {
    emptyRow.label = tr(STR_NOTES_EMPTY);
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

}  // namespace inkhome::notes
