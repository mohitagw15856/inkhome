#pragma once

#include <array>

#include "activities/UiListActivity.h"

// Shown at start-up when no SD card mounts. Upstream stops on a static
// "SD card error" message; Inkhome keeps the reader usable instead: retry the
// card, open the apps that work without one, or switch to the other firmware.
// Holding Power still sleeps as usual.
class NoSdCardActivity final : public UiListActivity {
 public:
  explicit NoSdCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void onBackButton() override {}
  void drawFooter() override;

  std::array<freeink::ui::ListItem, 3> rows{};
};
