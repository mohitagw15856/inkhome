#pragma once

// Focus timer (Pomodoro for reading): pick a length, read, and a break screen
// appears when it ends. The timer lives in RAM, so sleeping or restarting the
// reader cancels it. Heap: none beyond the screens while they are open.

#include <cstdint>
#include <memory>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace inkhome::focus {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"focus", StrId::STR_FOCUS_TITLE, StrId::STR_FOCUS_SUMMARY, &create, nullptr};

void start(uint8_t minutes);
void stop();
bool active();
uint8_t lengthMinutes();
// Main loop: shows the break screen when the time is up.
void tick(bool readerActive);

class FocusActivity final : public UiListActivity {
 public:
  explicit FocusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Focus", renderer, mappedInput) {}
  void onEnter() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  static constexpr int LENGTH_COUNT = 4;
  freeink::ui::ListItem rows[LENGTH_COUNT + 2];  // lengths, writing sprint, stop
  char labels[LENGTH_COUNT][24] = {};
  int rowCount = 0;
};

class BreakActivity final : public Activity {
 public:
  explicit BreakActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FocusBreak", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

}  // namespace inkhome::focus
