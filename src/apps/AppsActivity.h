#pragma once

#include <vector>

#include "activities/UiListActivity.h"

// Apps launcher: lists the apps registered in AppRegistry and opens one on
// Confirm. Each app runs as a pushed activity, so Back from its root screen
// returns here and Back from here returns Home.
class AppsActivity final : public UiListActivity {
 public:
  explicit AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void onBackButton() override;

  // One row per available app; labels point at flash-resident i18n strings.
  std::vector<freeink::ui::ListItem> rowItems;
  freeink::ui::ListItem emptyItem;
};
