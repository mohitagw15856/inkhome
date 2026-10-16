#include "WallpaperApp.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "inkhome/SdState.h"
#include "inkhome/XthWallpaper.h"

namespace fui = freeink::ui;

namespace wallpaper {

namespace {

constexpr int ROW_ROTATE = 0;

// Pinning only shows on the picture sleep modes; switch others to Custom.
void useCustomSleepMode() {
  const auto mode = SETTINGS.sleepScreen;
  if (mode == CrossPointSettings::CUSTOM || mode == CrossPointSettings::CUSTOM_STATS ||
      mode == CrossPointSettings::COVER_CUSTOM) {
    return;
  }
  SETTINGS.sleepScreen = CrossPointSettings::CUSTOM;
  SETTINGS.saveToFile();
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<WallpaperListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- list

void WallpaperListActivity::onEnter() {
  UiListActivity::onEnter();
  refresh();
}

void WallpaperListActivity::onExit() {
  rows = {};
  names = {};
  folders = {};
  paths = {};
  UiListActivity::onExit();
}

void WallpaperListActivity::refresh() {
  paths.clear();
  pinned.clear();
  if (inkhome::sdMounted()) {
    paths = inkhome::wallpaper::listAll();
    inkhome::wallpaper::pinnedPath(pinned);
  }
  names.assign(paths.size(), std::string());
  folders.assign(paths.size(), std::string());
  for (size_t i = 0; i < paths.size(); ++i) {
    const auto slash = paths[i].find_last_of('/');
    names[i] = paths[i].substr(slash + 1);
    folders[i] = slash == 0 ? std::string("/") : paths[i].substr(1, slash - 1);
  }
  rows.assign(paths.size() + 1, fui::ListItem{});
  rows[0].label = tr(STR_WALL_ROTATE);
  rows[0].subtitle = tr(STR_WALL_ROTATE_HINT);
  rows[0].value = pinned.empty() ? tr(STR_WALL_IN_USE) : nullptr;
  rows[0].actionValue = ROW_ROTATE;
  for (size_t i = 0; i < paths.size(); ++i) {
    rows[i + 1].label = names[i].c_str();
    rows[i + 1].subtitle = folders[i].c_str();
    rows[i + 1].value = paths[i] == pinned ? tr(STR_WALL_IN_USE) : nullptr;
    rows[i + 1].actionValue = static_cast<int16_t>(i + 1);
  }
}

int WallpaperListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* WallpaperListActivity::headerTitle() const { return tr(STR_WALL_TITLE); }

void WallpaperListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  if (index == ROW_ROTATE) {
    if (!inkhome::wallpaper::setPinned("")) LOG_ERR("WALL", "Could not clear the pin");
    refresh();
    requestUpdate();
    return;
  }
  auto preview = makeUniqueNoThrow<PreviewActivity>(renderer, mappedInput, paths[static_cast<size_t>(index - 1)]);
  if (!preview) return;
  startActivityForResult(std::move(preview), [this](const ActivityResult&) {
    refresh();
    requestUpdate();
  });
}

void WallpaperListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- preview

void PreviewActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void PreviewActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(Button::Confirm) && !failed) {
    if (!chosen) {
      chosen = inkhome::wallpaper::setPinned(path);
      if (chosen) useCustomSleepMode();
    }
    finish();
  }
}

void PreviewActivity::render(RenderLock&&) {
  if (inkhome::wallpaper::isXth(path)) {
    // Drawn and shown as the sleep screen will show it (greyscale, no hints).
    failed = !inkhome::wallpaper::drawXth(renderer, path, inkhome::sleep::Overlays{});
  } else {
    renderer.clearScreen();
    HalFile file;
    failed = true;
    if (Storage.openFileForRead("WALL", path, file)) {
      Bitmap bitmap(file, true, false);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        const int x = std::max(0, (renderer.getScreenWidth() - bitmap.getWidth()) / 2);
        const int y = std::max(0, (renderer.getScreenHeight() - bitmap.getHeight()) / 2);
        failed = !renderer.drawBitmap(bitmap, x, y, renderer.getScreenWidth(), renderer.getScreenHeight());
      }
    }
    if (!failed) renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  }
  if (failed) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_WALL_FAILED), true,
                              EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  }
}

}  // namespace wallpaper
