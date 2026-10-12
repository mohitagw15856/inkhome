#pragma once

// PocketWiki: an offline reference library (.pwa archives built on a computer
// with PocketWiki's companion tool). Native port of PocketWiki
// (github.com/mohitagw15856/PocketWiki, MIT): the archive reader, render
// stream and puff inflater are PocketWiki's; the screens and the proportional
// font layout are Inkhome's. Archives come from /apps/pocketwiki/ and the
// legacy /pocketwiki/. Everything streams from the card: the title index is
// binary searched in place and one 4 KB block is inflated at a time. Heap:
// one archive's block buffers and one article's layout.

#include <HalStorage.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "apps/inkkit/HalFileReader.h"
#include "core/PwaReader.h"
#include "core/WikiLayout.h"

namespace pocketwiki {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"pocketwiki", StrId::STR_WIKI_TITLE, StrId::STR_WIKI_SUMMARY, &create,
                                                  nullptr};

struct ArchiveEntry {
  std::string label;
  std::string path;
};

class ArchiveListActivity final : public UiListActivity {
 public:
  explicit ArchiveListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("PocketWiki", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<ArchiveEntry> entries;
  std::vector<freeink::ui::ListItem> rows;
  freeink::ui::ListItem emptyRow;
};

// Title list for one archive: row 0 searches, the rest are titles (all of
// them, or those matching the last search).
class ArticleListActivity final : public UiListActivity {
 public:
  ArticleListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, ArchiveEntry entry)
      : UiListActivity("PocketWikiList", renderer, mappedInput), entry(std::move(entry)) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void runQuery();

  ArchiveEntry entry;
  HalFile file;
  std::unique_ptr<inkkit::HalFileReader> source;
  std::unique_ptr<PwaReader> archive;
  std::string query;
  std::string title;
  std::vector<SearchHit> hits;
  std::vector<freeink::ui::ListItem> rows;
};

class ArticleActivity final : public Activity {
 public:
  ArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path, uint32_t articleId);
  ~ArticleActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct LinkSpot {
    size_t line;
    size_t segment;
  };
  void load(uint32_t id);
  void collectLinks();

  std::string path;
  uint32_t articleId;
  HalFile file;
  std::unique_ptr<inkkit::HalFileReader> source;
  std::unique_ptr<PwaReader> archive;
  std::unique_ptr<WikiLayout> layout;
  std::string title;
  std::vector<size_t> pageStarts;
  std::vector<LinkSpot> pageLinks;  // links on the current page
  std::vector<uint32_t> backStack;  // articles reached by following links
  size_t page = 0;
  int selectedLink = -1;  // -1 when not choosing a link
  bool loaded = false;
  bool fullRefresh = true;
};

}  // namespace pocketwiki
