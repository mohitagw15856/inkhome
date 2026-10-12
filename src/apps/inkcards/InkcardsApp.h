#pragma once

// Inkcards: spaced-repetition flashcards (SM-2), native port of Inkcards
// (github.com/mohitagw15856/Inkcards, MIT). Decks are compiled .deck files
// from /apps/inkcards/ or the legacy /inkcards/decks/; the demo and world
// capitals decks are built in. Review state per deck lives in
// /.crosspoint/apps/inkcards/<deck>.rev in Inkcards' own format.
// Days come from the clock (X3); without one each new session counts as a day.
// Heap: one card's text at a time plus a small per-card queue.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace inkcards {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"inkcards", StrId::STR_CARDS_TITLE, StrId::STR_CARDS_SUMMARY, &create,
                                                  nullptr};

struct DeckEntry {
  std::string label;
  std::string source;  // "builtin:demo", "builtin:capitals" or an SD path
  std::string stateKey;
};

class DeckListActivity final : public UiListActivity {
 public:
  explicit DeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Inkcards", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<DeckEntry> entries;
  std::vector<freeink::ui::ListItem> rows;
};

class StudyActivity final : public Activity {
 public:
  StudyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, DeckEntry entry);
  ~StudyActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct State;
  void loadCurrent();
  void grade(uint8_t grade);
  void save();

  DeckEntry entry;
  std::unique_ptr<State> state;
  bool revealed = false;
  bool failed = false;
};

}  // namespace inkcards
