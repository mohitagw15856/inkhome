#include "FirmwareSwitchApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#ifndef SIMULATOR
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#endif

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "inkhome/InkhomeConfig.h"
#ifndef SIMULATOR
#include "network/OtaBootSwitch.h"
#endif

namespace inkhome::fwswitch {

#ifndef SIMULATOR
namespace {

constexpr uint8_t ESP_IMAGE_MAGIC = 0xE9;
constexpr char STOCK_PROJECT_NAME[] = "arduino-lib-builder";

void copyField(char* out, const size_t outSize, const char* in, const size_t inSize) {
  const size_t n = strnlen(in, inSize);
  const size_t len = n < outSize - 1 ? n : outSize - 1;
  memcpy(out, in, len);
  out[len] = '\0';
}

}  // namespace
#endif

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<FirmwareSwitchActivity>(renderer, mappedInput);
}

void FirmwareSwitchActivity::onEnter() {
  Activity::onEnter();
  inspectSlots();
  requestUpdate();
}

void FirmwareSwitchActivity::inspectSlots() {
  state = State::NoOtherFirmware;
#ifdef SIMULATOR
  // The simulator has no flash partitions; show the dual-boot layout for screenshots.
  snprintf(runningLabel, sizeof(runningLabel), "app0");
  snprintf(otherLabel, sizeof(otherLabel), "app1");
  snprintf(otherName, sizeof(otherName), "%s", tr(STR_FW_SWITCH_STOCK));
  state = State::Ready;
#else
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running) copyField(runningLabel, sizeof(runningLabel), running->label, sizeof(running->label));

  const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
  if (!other || other == running) return;
  copyField(otherLabel, sizeof(otherLabel), other->label, sizeof(other->label));

  uint8_t magic = 0;
  if (esp_partition_read(other, 0, &magic, 1) != ESP_OK || magic != ESP_IMAGE_MAGIC) {
    LOG_INF("FWSW", "No app image in %s", otherLabel);
    return;
  }

  esp_app_desc_t desc = {};
  if (esp_ota_get_partition_description(other, &desc) == ESP_OK) {
    copyField(otherName, sizeof(otherName), desc.project_name, sizeof(desc.project_name));
    copyField(otherVersion, sizeof(otherVersion), desc.version, sizeof(desc.version));
    if (strcmp(otherName, STOCK_PROJECT_NAME) == 0) {
      // Xteink's stock builds carry Espressif's builder name and a git hash; show something readable.
      snprintf(otherName, sizeof(otherName), "%s", tr(STR_FW_SWITCH_STOCK));
      otherVersion[0] = '\0';
    } else if (strcmp(otherName, "inkhome") == 0) {
      snprintf(otherName, sizeof(otherName), "%s", inkhome::FORK_NAME_FOR_UI);
    }
  } else {
    // Older stock images may not carry a readable description; the image magic is enough to boot.
    snprintf(otherName, sizeof(otherName), "%s", tr(STR_FW_SWITCH_UNKNOWN));
    otherVersion[0] = '\0';
  }
  LOG_INF("FWSW", "Running %s; other %s holds %s %s", runningLabel, otherLabel, otherName, otherVersion);
  state = State::Ready;
#endif
}

void FirmwareSwitchActivity::doSwitch() {
#ifdef SIMULATOR
  state = State::Failed;
  requestUpdate();
#else
  const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
  if (!ota_boot::switchTo(other)) {
    LOG_ERR("FWSW", "Boot switch to %s failed", otherLabel);
    state = State::Failed;
    requestUpdate();
    return;
  }
  state = State::Switching;
  requestUpdateAndWait();
  SETTINGS.saveToFile();
  Storage.prepareForDeepSleep();
  LOG_INF("FWSW", "Restarting into %s", otherLabel);
  delay(300);
  esp_restart();
#endif
}

void FirmwareSwitchActivity::loop() {
  if (state == State::Switching) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (state == State::Confirming) {
      state = State::Ready;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (state == State::Ready || state == State::Failed) {
      state = State::Confirming;
      requestUpdate();
    } else if (state == State::Confirming) {
      doSwitch();
    }
  }
}

void FirmwareSwitchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const int textWidth = pageWidth - side * 2;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FW_SWITCH_TITLE));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  auto paragraph = [&](const char* text, const EpdFontFamily::Style style) {
    for (const auto& line : renderer.wrappedText(UI_12_FONT_ID, text, textWidth, 6, style)) {
      renderer.drawText(UI_12_FONT_ID, side, y, line.c_str(), true, style);
      y += lineHeight;
    }
    y += lineHeight / 2;
  };

  char buf[128];
  snprintf(buf, sizeof(buf), "%s: %s %s (%s)", tr(STR_FW_SWITCH_RUNNING), inkhome::FORK_NAME_FOR_UI, CROSSPOINT_VERSION,
           runningLabel);
  paragraph(buf, EpdFontFamily::REGULAR);

  const char* confirmLabel = "";
  switch (state) {
    case State::NoOtherFirmware:
      paragraph(tr(STR_FW_SWITCH_NONE), EpdFontFamily::BOLD);
      paragraph(tr(STR_FW_SWITCH_NONE_HINT), EpdFontFamily::REGULAR);
      break;
    case State::Ready:
    case State::Failed:
      snprintf(buf, sizeof(buf), "%s: %s%s%s (%s)", tr(STR_FW_SWITCH_OTHER), otherName, otherVersion[0] ? " " : "",
               otherVersion, otherLabel);
      paragraph(buf, EpdFontFamily::BOLD);
      paragraph(tr(STR_FW_SWITCH_EXPLAIN), EpdFontFamily::REGULAR);
      if (state == State::Failed) paragraph(tr(STR_FW_SWITCH_FAILED), EpdFontFamily::BOLD);
      confirmLabel = tr(STR_FW_SWITCH_ACTION);
      break;
    case State::Confirming:
      snprintf(buf, sizeof(buf), "%s: %s%s%s", tr(STR_FW_SWITCH_OTHER), otherName, otherVersion[0] ? " " : "",
               otherVersion);
      paragraph(buf, EpdFontFamily::BOLD);
      paragraph(tr(STR_FW_SWITCH_CONFIRM), EpdFontFamily::BOLD);
      confirmLabel = tr(STR_FW_SWITCH_RESTART);
      break;
    case State::Switching:
      paragraph(tr(STR_FW_SWITCH_RESTARTING), EpdFontFamily::BOLD);
      break;
  }

  if (state != State::Switching) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(state == State::Switching ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
}

}  // namespace inkhome::fwswitch
