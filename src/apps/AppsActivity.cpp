#include "AppsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "AppRegistry.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

AppsActivity::AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("Apps", renderer, mappedInput) {}

void AppsActivity::onEnter() {
  UiListActivity::onEnter();
  const size_t count = inkhome::apps::availableCount();
  rowItems.clear();
  rowItems.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const auto& app = inkhome::apps::availableAt(i);
    fui::ListItem item;
    item.label = I18N.get(app.nameId);
    item.subtitle = I18N.get(app.summaryId);
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
}

int AppsActivity::listCount() const { return static_cast<int>(rowItems.size()); }

const char* AppsActivity::headerTitle() const { return tr(STR_APPS); }

void AppsActivity::onBackButton() { activityManager.goHome(HomeMenuItem::APPS); }

void AppsActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  const auto& descriptor = inkhome::apps::availableAt(static_cast<size_t>(index));
  auto activity = descriptor.create(renderer, mappedInput);
  if (!activity) {
    LOG_ERR("APPS", "OOM: could not start %s", descriptor.id);
    return;
  }
  LOG_INF("APPS", "Opening %s", descriptor.id);
  app.clearTapFlash();
  activityManager.pushActivity(std::move(activity));
}

void AppsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    emptyItem.label = tr(STR_NO_APPS);
    emptyItem.enabled = false;
    fui::ListProps props;
    props.items = &emptyItem;
    props.count = 1;
    props.inputMask = fui::InputNone;
    screen.list(props);
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
