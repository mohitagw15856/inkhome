#pragma once

// Wallpaper: choose one picture for the Custom sleep screen instead of
// rotating through them all. Lists the stock firmware's /Pushed Images
// (.xth/.xtg) and the /sleep pictures (.bmp too), previews one full screen,
// and pins it in /.crosspoint/apps/wallpaper/pinned.txt. "Rotate" removes the
// pin. Heap: the file list, and one picture while previewing.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace wallpaper {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"wallpaper", StrId::STR_WALL_TITLE, StrId::STR_WALL_SUMMARY, &create,
                                                  nullptr};

class WallpaperListActivity final : public UiListActivity {
 public:
  explicit WallpaperListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Wallpaper", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void refresh();

  std::string pinned;
  std::vector<std::string> paths;
  std::vector<std::string> names;
  std::vector<std::string> folders;
  std::vector<freeink::ui::ListItem> rows;
};

class PreviewActivity final : public Activity {
 public:
  PreviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
      : Activity("WallpaperPreview", renderer, mappedInput), path(std::move(path)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string path;
  bool chosen = false;
  bool failed = false;
};

}  // namespace wallpaper
