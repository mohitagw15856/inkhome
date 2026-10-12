#pragma once

// InkQuest: choose-your-path interactive fiction (.iqs). Native port of
// InkQuest (github.com/mohitagw15856/InkQuest, MIT). The story engine and save
// format are InkQuest's own; this file adds the list and play screens. Stories
// come from /apps/inkquest/ and the legacy /inkquest/stories/, plus one built
// in (The Lighthouse Keeper) so the app works without a card. Progress is
// saved after every choice to /.crosspoint/apps/inkquest/<story>.sav and
// resumed on reopen. Heap: the passage table and one passage while playing.

#include <HalStorage.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "core/StoryEngine.h"

namespace inkquest {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"inkquest", StrId::STR_QUEST_TITLE, StrId::STR_QUEST_SUMMARY, &create,
                                                  nullptr};

struct StoryEntry {
  std::string label;
  std::string path;  // empty for the built-in story
};

class StoryListActivity final : public UiListActivity {
 public:
  explicit StoryListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("InkQuest", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<StoryEntry> entries;
  std::vector<freeink::ui::ListItem> rows;
};

class PlayActivity final : public Activity {
 public:
  PlayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, StoryEntry entry);
  ~PlayActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void layoutPassage();
  void takeChoice();
  void restart();
  void save() const;
  std::string savePath() const;

  StoryEntry entry;
  HalFile file;
  std::unique_ptr<inkkit::ByteReader> reader;
  StoryEngine engine;
  PassageView passage;
  bool loaded = false;
  bool ended = false;

  // Laid out by the input thread under the render lock.
  std::vector<std::string> lines;
  std::vector<uint16_t> visibleChoices;  // indexes into passage.choices
  int linesPerPage = 1;
  int page = 0;
  int pageCount = 1;
  int selected = 0;
  bool fullRefresh = true;
};

}  // namespace inkquest
