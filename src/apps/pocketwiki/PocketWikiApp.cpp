#include "PocketWikiApp.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "apps/AppPaths.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace pocketwiki {

namespace {

constexpr char APP_ID[] = "pocketwiki";
constexpr char LEGACY_DIR[] = "/pocketwiki";
constexpr size_t MAX_ARCHIVES = 40;
constexpr size_t MAX_TITLES = 200;
constexpr size_t MAX_BACK = 16;
constexpr int MARGIN = 18;

#ifndef OMIT_FONTS
constexpr int BODY_FONT_ID = NOTOSERIF_12_FONT_ID;
#else
constexpr int BODY_FONT_ID = UI_10_FONT_ID;
#endif
constexpr int HEADING_FONT_ID = UI_12_FONT_ID;

int fontIdFor(const Font f) { return f == Font::Heading ? HEADING_FONT_ID : BODY_FONT_ID; }

int measureText(const void* ctx, const char* text, const Font font, const uint8_t style) {
  const auto* renderer = static_cast<const GfxRenderer*>(ctx);
  const auto fontStyle = static_cast<EpdFontFamily::Style>(style & 0x03);
  // A lone space has no glyph box, so its advance comes from the font.
  if (text[0] == ' ' && text[1] == '\0') return renderer->getSpaceWidth(fontIdFor(font), fontStyle);
  return renderer->getTextWidth(fontIdFor(font), text, fontStyle);
}

void addFolder(const char* dir, std::vector<ArchiveEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_ARCHIVES)) {
    if (out.size() >= MAX_ARCHIVES) return;
    std::string name = e.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() < 5 || name[0] == '.' || name.compare(name.size() - 4, 4, ".pwa") != 0) continue;
    out.push_back({name.substr(0, name.size() - 4), std::string(dir) + "/" + name});
  }
}

// Opens `path` into the three owned objects; false (and logs) on any failure.
bool openArchive(const std::string& path, HalFile& file, std::unique_ptr<inkkit::HalFileReader>& source,
                 std::unique_ptr<PwaReader>& archive) {
  if (!inkhome::sdMounted() || !Storage.openFileForRead("PWA", path.c_str(), file)) return false;
  source = makeUniqueNoThrow<inkkit::HalFileReader>(file);
  if (source) archive = makeUniqueNoThrow<PwaReader>(*source);
  if (archive && archive->open()) return true;
  LOG_ERR("PWA", "Not a PocketWiki archive: %s", path.c_str());
  archive.reset();
  source.reset();
  file.close();
  return false;
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ArchiveListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- archives

void ArchiveListActivity::onEnter() {
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

void ArchiveListActivity::onExit() {
  rows = {};
  entries = {};
  UiListActivity::onExit();
}

int ArchiveListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* ArchiveListActivity::headerTitle() const { return tr(STR_WIKI_TITLE); }

void ArchiveListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto list = makeUniqueNoThrow<ArticleListActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!list) return;
  app.clearTapFlash();
  activityManager.pushActivity(std::move(list));
}

void ArchiveListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  if (rows.empty()) {
    emptyRow.label = tr(STR_WIKI_EMPTY);
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

// ---------------------------------------------------------------- titles

void ArticleListActivity::onEnter() {
  UiListActivity::onEnter();
  if (openArchive(entry.path, file, source, archive)) {
    title = archive->header().collectionName.empty() ? entry.label : archive->header().collectionName;
  } else {
    title = entry.label;
  }
  runQuery();
}

void ArticleListActivity::onExit() {
  rows = {};
  hits = {};
  archive.reset();
  source.reset();
  file.close();
  UiListActivity::onExit();
}

void ArticleListActivity::runQuery() {
  hits.clear();
  if (archive) hits = archive->search(query, MAX_TITLES);
  rows.assign(hits.size() + 1, fui::ListItem{});
  rows[0].label = tr(STR_WIKI_SEARCH);
  rows[0].actionValue = 0;
  for (size_t i = 0; i < hits.size(); ++i) {
    rows[i + 1].label = hits[i].title.c_str();
    rows[i + 1].actionValue = static_cast<int16_t>(i + 1);
  }
  if (!archive) {
    rows[0].label = tr(STR_WIKI_LOAD_FAILED);
    rows[0].enabled = false;
  }
  nav.top = 0;
  nav.selected = hits.empty() ? 0 : 1;
}

int ArticleListActivity::listCount() const { return static_cast<int>(rows.size()); }

const char* ArticleListActivity::headerTitle() const { return query.empty() ? title.c_str() : query.c_str(); }

void ArticleListActivity::activateIndex(const int index) {
  if (!archive || index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  if (index == 0) {
    auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WIKI_SEARCH), query, 40);
    if (!keyboard) return;
    startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
      if (result.isCancelled) return;
      query = std::get<KeyboardResult>(result.data).text;
      runQuery();
      requestUpdate();
    });
    return;
  }
  auto article = makeUniqueNoThrow<ArticleActivity>(renderer, mappedInput, entry.path,
                                                    hits[static_cast<size_t>(index - 1)].articleId);
  if (!article) return;
  activityManager.pushActivity(std::move(article));
}

void ArticleListActivity::buildScreen(UiScreen& screen) {
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

// ---------------------------------------------------------------- article

ArticleActivity::ArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path,
                                 const uint32_t articleId)
    : Activity("PocketWikiArticle", renderer, mappedInput), path(std::move(path)), articleId(articleId) {}

ArticleActivity::~ArticleActivity() = default;

void ArticleActivity::onEnter() {
  Activity::onEnter();
  if (openArchive(path, file, source, archive)) load(articleId);
  requestUpdate();
}

void ArticleActivity::onExit() {
  layout.reset();
  pageStarts = {};
  pageLinks = {};
  backStack = {};
  archive.reset();
  source.reset();
  file.close();
  Activity::onExit();
}

void ArticleActivity::load(const uint32_t id) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  LayoutMetrics lm;
  lm.width = renderer.getScreenWidth() - MARGIN * 2;
  lm.bodyLineHeight = renderer.getLineHeight(BODY_FONT_ID);
  lm.headingLineHeight = renderer.getLineHeight(HEADING_FONT_ID);
  lm.indent = renderer.getSpaceWidth(BODY_FONT_ID) * 5;
  lm.measure = &measureText;
  lm.ctx = &renderer;

  layout.reset();  // free the previous article before laying out the next
  layout = makeUniqueNoThrow<WikiLayout>(lm);
  DirEntry dir;
  loaded = layout && archive->dirEntry(id, dir) && archive->readArticle(id, *layout);
  if (!loaded) {
    LOG_ERR("PWA", "Article %u could not be read", static_cast<unsigned>(id));
    layout.reset();
  }
  articleId = id;
  title = loaded ? dir.title : std::string();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  pageStarts = loaded && layout ? layout->paginate(bottom - top) : std::vector<size_t>{0};
  page = 0;
  selectedLink = -1;
  collectLinks();
  fullRefresh = true;
}

void ArticleActivity::collectLinks() {
  pageLinks.clear();
  if (!layout) return;
  const auto& lines = layout->lines();
  const size_t end = page + 1 < pageStarts.size() ? pageStarts[page + 1] : lines.size();
  for (size_t l = pageStarts[page]; l < end; ++l) {
    for (size_t s = 0; s < lines[l].segments.size(); ++s) {
      if (lines[l].segments[s].link != kNoLink) pageLinks.push_back({l, s});
    }
  }
}

void ArticleActivity::loop() {
  using Button = MappedInputManager::Button;
  const bool next = mappedInput.wasPressed(Button::Down) || mappedInput.wasPressed(Button::Right);
  const bool prev = mappedInput.wasPressed(Button::Up) || mappedInput.wasPressed(Button::Left);

  if (mappedInput.wasReleased(Button::Back)) {
    if (selectedLink >= 0) {
      selectedLink = -1;
      requestUpdate();
    } else if (!backStack.empty()) {
      RenderLock lock(*this);
      const uint32_t previous = backStack.back();
      backStack.pop_back();
      load(previous);
      requestUpdate();
    } else {
      finish();
    }
    return;
  }
  if (!loaded || !layout) return;

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (selectedLink < 0) {
      if (!pageLinks.empty()) {
        selectedLink = 0;
        requestUpdate();
      }
      return;
    }
    const LinkSpot spot = pageLinks[static_cast<size_t>(selectedLink)];
    const uint32_t target = layout->lines()[spot.line].segments[spot.segment].link;
    if (target >= archive->articleCount()) return;
    RenderLock lock(*this);
    if (backStack.size() >= MAX_BACK) backStack.erase(backStack.begin());
    backStack.push_back(articleId);
    load(target);
    requestUpdate();
    return;
  }

  if (selectedLink >= 0) {
    const int count = static_cast<int>(pageLinks.size());
    if (next || prev) {
      selectedLink = (selectedLink + (next ? 1 : count - 1)) % count;
      requestUpdate();
    }
    return;
  }
  if (next && page + 1 < pageStarts.size()) {
    RenderLock lock(*this);
    page++;
    collectLinks();
    requestUpdate();
  } else if (prev && page > 0) {
    RenderLock lock(*this);
    page--;
    collectLinks();
    requestUpdate();
  }
}

void ArticleActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight},
                 loaded ? title.c_str() : tr(STR_WIKI_TITLE));

  if (!loaded) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_WIKI_LOAD_FAILED), true,
                              EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }

  const auto& lines = layout->lines();
  const size_t end = page + 1 < pageStarts.size() ? pageStarts[page + 1] : lines.size();
  const LinkSpot* chosen = selectedLink >= 0 ? &pageLinks[static_cast<size_t>(selectedLink)] : nullptr;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  for (size_t l = pageStarts[page]; l < end; ++l) {
    const Line& line = lines[l];
    if (l != pageStarts[page]) y += line.gapBefore;
    if (line.rule) {
      renderer.fillRect(MARGIN, y + line.height / 2, width - MARGIN * 2, 1, true);
    }
    for (size_t s = 0; s < line.segments.size(); ++s) {
      const Segment& seg = line.segments[s];
      const int fontId = fontIdFor(seg.font);
      const int x = MARGIN + seg.x;
      const bool highlighted = chosen && chosen->line == l && chosen->segment == s;
      if (highlighted) renderer.fillRect(x - 2, y, seg.width + 4, line.height, true);
      renderer.drawText(fontId, x, y, seg.text.c_str(), !highlighted,
                        static_cast<EpdFontFamily::Style>(seg.style & 0x03));
      if (seg.link != kNoLink && !highlighted) {
        renderer.fillRect(x, y + renderer.getLineHeight(fontId) - 3, seg.width, 1, true);
      }
    }
    y += line.height;
  }

  char pos[16] = {};
  if (pageStarts.size() > 1) {
    snprintf(pos, sizeof(pos), "%u / %u", static_cast<unsigned>(page + 1), static_cast<unsigned>(pageStarts.size()));
  }
  const char* confirm = selectedLink >= 0 ? tr(STR_WIKI_OPEN) : (pageLinks.empty() ? "" : tr(STR_WIKI_LINKS));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, pos, "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(fullRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  fullRefresh = false;
}

}  // namespace pocketwiki
