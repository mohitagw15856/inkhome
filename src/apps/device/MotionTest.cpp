#include "MotionTest.h"

#include <GfxRenderer.h>
#include <HalTiltSensor.h>
#include <I18n.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "inkhome/Motion.h"

namespace inkhome::motion {

namespace {
constexpr unsigned long REDRAW_MS = 1000;  // e-ink: about once a second is plenty
}

bool hasImu() { return halTiltSensor.isAvailable(); }

std::unique_ptr<Activity> createTest(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MotionTestActivity>(renderer, mappedInput);
}

void MotionTestActivity::onEnter() {
  Activity::onEnter();
  setTestScreenOpen(true);
  requestUpdate();
}

void MotionTestActivity::onExit() {
  setTestScreenOpen(false);
  Activity::onExit();
}

void MotionTestActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (millis() - lastDrawMs >= REDRAW_MS) {
    lastDrawMs = millis();
    requestUpdate();
  }
}

void MotionTestActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID) + 4;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_MOTION_TITLE));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  const Reading r = latest();
  char line[96];
  if (!r.valid) {
    renderer.drawText(UI_12_FONT_ID, side, y, tr(STR_MOTION_WAITING), true);
  } else {
    const float g = std::sqrt(r.ax * r.ax + r.ay * r.ay + r.az * r.az);
    snprintf(line, sizeof(line), "%s  x %+.2f  y %+.2f  z %+.2f g", tr(STR_MOTION_ACCEL), r.ax, r.ay, r.az);
    renderer.drawText(UI_12_FONT_ID, side, y, line, true);
    y += lineH;
    snprintf(line, sizeof(line), "%s  %.2f g", tr(STR_MOTION_TOTAL), g);
    renderer.drawText(UI_12_FONT_ID, side, y, line, true);
    y += lineH;
    snprintf(line, sizeof(line), "%s  x %+.0f  y %+.0f  z %+.0f deg/s", tr(STR_MOTION_GYRO), r.gx, r.gy, r.gz);
    renderer.drawText(UI_12_FONT_ID, side, y, line, true);
    y += lineH * 2;
  }

  const int faceUp = learntFaceUpSign();
  snprintf(line, sizeof(line), "%s: %s", tr(STR_MOTION_FACE_UP),
           faceUp == 0  ? tr(STR_MOTION_LEARNING)
           : faceUp > 0 ? "+Z"
                        : "-Z");
  renderer.drawText(UI_10_FONT_ID, side, y, line, true);
  y += lineH;
  snprintf(line, sizeof(line), "%s: %s", tr(STR_SHAKE_ACTION),
           SETTINGS.motionShakeBookmark == 2   ? tr(STR_SHAKE_LOOKUP_OPT)
           : SETTINGS.motionShakeBookmark == 1 ? tr(STR_SHAKE_BOOKMARK_OPT)
                                               : tr(STR_STATE_OFF));
  renderer.drawText(UI_10_FONT_ID, side, y, line, true);
  y += lineH;
  snprintf(line, sizeof(line), "%s: %s", tr(STR_FLIP_SLEEP),
           SETTINGS.motionFlipSleep ? tr(STR_STATE_ON) : tr(STR_STATE_OFF));
  renderer.drawText(UI_10_FONT_ID, side, y, line, true);
  y += lineH * 2;
  for (const auto& l : renderer.wrappedText(SMALL_FONT_ID, tr(STR_MOTION_HINT), width - side * 2, 6)) {
    renderer.drawText(SMALL_FONT_ID, side, y, l.c_str(), true);
    y += renderer.getLineHeight(SMALL_FONT_ID);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

}  // namespace inkhome::motion
