#pragma once

// Achievements: unlocked badges first (newest first), then the rest by how
// close they are, with progress. Badges are checked whenever a reading session
// ends and saved to /.crosspoint/achievements.json in vCodex's format.

#include <memory>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace inkhome::stats {

std::unique_ptr<Activity> createAchievements(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor ACHIEVEMENTS_DESCRIPTOR{"achievements", StrId::STR_ACH_TITLE, StrId::STR_ACH_SUMMARY,
                                                      &createAchievements, nullptr};

// Checks every badge against the current stats and saves any new unlocks.
// Called by the tracker when a session ends. Returns how many were new.
int refreshAchievements();

class AchievementsActivity final : public UiListActivity {
 public:
  explicit AchievementsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Achievements", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int) override {}
  const char* headerTitle() const override { return header.c_str(); }

  std::string header;
  std::vector<std::string> labels;
  std::vector<std::string> subtitles;
  std::vector<freeink::ui::ListItem> rows;
};

}  // namespace inkhome::stats
