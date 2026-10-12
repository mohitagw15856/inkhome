#pragma once

// StoryScapes: illustrated micro-history packs (.ssp) with text and 1-bit
// picture pages. Native port of StoryScapes (github.com/mohitagw15856/storyscapes,
// MIT). Packs come from /apps/storyscapes/ and the legacy /storyscapes/;
// progress per pack lives in /.crosspoint/apps/storyscapes/<pack>.txt.
// Pictures stream from the card one row at a time (no image buffer); text is
// re-wrapped for the panel. Heap: the page table (up to 512 entries) and one
// text page (6 KB cap).

#include <HalStorage.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "core/Ssp.h"

namespace storyscapes {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"storyscapes", StrId::STR_STORY_TITLE, StrId::STR_STORY_SUMMARY,
                                                  &create, nullptr};

struct PackEntry {
  std::string label;
  std::string path;
};

class LibraryActivity final : public UiListActivity {
 public:
  explicit LibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("StoryScapes", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<PackEntry> entries;
  std::vector<freeink::ui::ListItem> rows;
  freeink::ui::ListItem emptyRow;
};

class PackReaderActivity final : public Activity {
 public:
  PackReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, PackEntry entry);
  ~PackReaderActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goTo(uint32_t page);
  void saveProgress() const;
  bool drawImagePage(const PageEntry& page);
  void drawTextPage(const PageEntry& page);

  PackEntry entry;
  HalFile file;
  PackInfo info;
  bool loaded = false;
  uint32_t page = 0;
  bool fullRefresh = true;
};

}  // namespace storyscapes
