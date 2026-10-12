#pragma once

// Habits: today's habits with tick boxes and streaks, a native port of
// Habitink (github.com/mohitagw15856/Habitink, MIT). Uses Habitink's own files
// in /.habitink/ (habits.tsv and logs/NN.log), so its companion and firmware
// keep working with the same card. Starts with Habitink's three example
// habits when there is no habits.tsv. Needs the clock (X3).
// Heap: up to 12 habits and about a year of their logs while open.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"

namespace inkhome::habits {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"habits", StrId::STR_HABITS_TITLE, StrId::STR_HABITS_SUMMARY, &create,
                                         nullptr};

// One habit as shown today.
struct TodayItem {
  int id = 0;
  std::string name;
  bool due = false;
  bool done = false;
  int streak = 0;
};

// Today's habits from the card (empty without a card or clock).
std::vector<TodayItem> loadToday();

class HabitsActivity final : public Activity {
 public:
  explicit HabitsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Habits", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void toggle(size_t index);

  std::vector<TodayItem> items;
  int selected = 0;
  bool noClock = false;
};

}  // namespace inkhome::habits
