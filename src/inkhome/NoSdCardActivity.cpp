#include "NoSdCardActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_system.h>

#include "MappedInputManager.h"
#include "apps/AppsActivity.h"
#include "apps/fwswitch/FirmwareSwitchApp.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
enum Row : int { RETRY = 0, APPS, SWITCH_FIRMWARE };
}

NoSdCardActivity::NoSdCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("NoSdCard", renderer, mappedInput) {}

void NoSdCardActivity::onEnter() {
  UiListActivity::onEnter();
  rows[RETRY].label = tr(STR_NO_SD_RETRY);
  rows[RETRY].subtitle = tr(STR_NO_SD_RETRY_HINT);
  rows[APPS].label = tr(STR_APPS);
  rows[APPS].subtitle = tr(STR_NO_SD_APPS_HINT);
  rows[SWITCH_FIRMWARE].label = tr(STR_FW_SWITCH_TITLE);
  rows[SWITCH_FIRMWARE].subtitle = tr(STR_FW_SWITCH_SUMMARY);
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) rows[i].actionValue = static_cast<int16_t>(i);
}

int NoSdCardActivity::listCount() const { return static_cast<int>(rows.size()); }

const char* NoSdCardActivity::headerTitle() const { return tr(STR_NO_SD_TITLE); }

void NoSdCardActivity::activateIndex(const int index) {
  app.clearTapFlash();
  switch (index) {
    case RETRY:
      // A clean restart re-runs the full SD and settings start-up path.
      LOG_INF("NOSD", "Restarting to retry the SD card");
      esp_restart();
      break;
    case APPS: {
      auto activity = makeUniqueNoThrow<AppsActivity>(renderer, mappedInput);
      if (activity) activityManager.pushActivity(std::move(activity));
      break;
    }
    case SWITCH_FIRMWARE: {
      auto activity = inkhome::fwswitch::create(renderer, mappedInput);
      if (activity) activityManager.pushActivity(std::move(activity));
      break;
    }
    default:
      break;
  }
}

void NoSdCardActivity::drawFooter() {
  // No Back label: there is nowhere to go back to without a card.
  const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void NoSdCardActivity::buildScreen(UiScreen& screen) {
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
