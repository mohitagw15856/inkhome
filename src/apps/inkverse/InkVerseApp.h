#pragma once

// InkVerse: poems and quotations, one at a time, and as a sleep screen.
// Native port of InkVerse (github.com/mohitagw15856/inkverse, MIT).
//
// Collections (.verse, see that repo's docs/FORMAT.md) come from
// /apps/inkverse/ and the legacy /inkverse/ folder; two public-domain
// collections are built in. "Use on sleep screen" stores the collection in
// /.crosspoint/apps/inkverse/sleep.txt and selects the Verse sleep mode, which
// shows the next verse each time the reader sleeps.
// Peak heap: one parsed collection (file capped at 32 KB).

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "core/VerseDoc.h"

class GfxRenderer;

namespace inkverse {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"inkverse", StrId::STR_VERSE_TITLE, StrId::STR_VERSE_SUMMARY, &create,
                                                  nullptr};

// A verse wrapped for the panel in the largest font that fits it.
struct VerseLayout {
  int fontId = 0;
  std::vector<std::string> body;
};

struct CollectionEntry {
  std::string label;
  std::string source;  // "builtin:poetry", "builtin:stoic" or an SD path
};

class CollectionListActivity final : public UiListActivity {
 public:
  explicit CollectionListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("InkVerse", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<CollectionEntry> entries;
  std::vector<freeink::ui::ListItem> rowItems;
};

class VerseActivity final : public Activity {
 public:
  VerseActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, CollectionEntry entry)
      : Activity("Verse", renderer, mappedInput), entry(std::move(entry)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void layoutVerse();

  CollectionEntry entry;
  Collection collection;
  bool loaded = false;
  size_t index = 0;
  VerseLayout layout;
  int scroll = 0;
  int visibleLines = 1;
  bool savedForSleep = false;
};

}  // namespace inkverse
