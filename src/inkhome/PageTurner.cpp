#include "PageTurner.h"

#include <Arduino.h>
#include <BleKeyboardHost.h>
#include <Logging.h>

#include "CrossPointSettings.h"

namespace inkhome::pageturner {

namespace {

constexpr uint32_t MIN_FREE_HEAP = 100 * 1024;  // NimBLE takes about 40 to 60 KB
constexpr unsigned long RETRY_MS = 60000;
constexpr unsigned long RECENT_MS = 60000;

bool appOwns = false;
bool startedHere = false;
bool pendingNext = false;
bool pendingPrev = false;
unsigned long lastKeyMs = 0;
unsigned long lastAttemptMs = 0;
bool attempted = false;

freeink::BleKeyboardHost& ble() { return freeink::BleKeyboardHost::getInstance(); }

}  // namespace

void setAppOwnsBluetooth(const bool owned) {
  appOwns = owned;
  if (owned) startedHere = false;  // the app takes over (and stops) the stack
}

void tick(const bool readerActive) {
  if (appOwns) return;
  const bool want = SETTINGS.blePageTurner && readerActive;
  if (!want) {
    if (startedHere && ble().isRunning()) {
      ble().end();
      LOG_INF("TURN", "Bluetooth off");
    }
    startedHere = false;
    attempted = false;
    pendingNext = pendingPrev = false;
    return;
  }
  if (!startedHere) {
    if (attempted && millis() - lastAttemptMs < RETRY_MS) return;
    attempted = true;
    lastAttemptMs = millis();
    const uint32_t before = ESP.getFreeHeap();
    if (before < MIN_FREE_HEAP) {
      LOG_INF("TURN", "Free heap %lu too low for Bluetooth", static_cast<unsigned long>(before));
      return;
    }
    startedHere = ble().isRunning() || ble().begin("Inkhome");
    LOG_INF("TURN", "Bluetooth %s; free heap %lu before, %lu after", startedHere ? "on" : "failed",
            static_cast<unsigned long>(before), static_cast<unsigned long>(ESP.getFreeHeap()));
    if (!startedHere) return;
  }
  ble().poll();
  freeink::KeyEvent ev;
  while (ble().popKey(ev)) {
    using SK = freeink::SpecialKey;
    const bool next = ev.special == SK::PageDown || ev.special == SK::Right || ev.special == SK::Down ||
                      ev.special == SK::Enter || (ev.special == SK::None && ev.ch == ' ');
    const bool prev = ev.special == SK::PageUp || ev.special == SK::Left || ev.special == SK::Up ||
                      ev.special == SK::Backspace;
    if (next) pendingNext = true;
    if (prev) pendingPrev = true;
    if (next || prev) lastKeyMs = millis();
  }
}

bool takeNext() {
  const bool v = pendingNext;
  pendingNext = false;
  return v;
}

bool takePrev() {
  const bool v = pendingPrev;
  pendingPrev = false;
  return v;
}

bool recentlyUsed() { return lastKeyMs != 0 && millis() - lastKeyMs < RECENT_MS; }

}  // namespace inkhome::pageturner
