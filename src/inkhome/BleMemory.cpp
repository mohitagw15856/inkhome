#include "BleMemory.h"

#include <Logging.h>

#ifndef SIMULATOR
#include <Arduino.h>
#include <esp32-hal-bt.h>
#endif

namespace inkhome::blemem {

bool releaseForWifi() {
#ifdef SIMULATOR
  return true;
#else
  if (btMemReleased(BT_MODE_BLE)) return true;
  if (btStarted()) {
    LOG_INF("BLE", "Bluetooth is running; its memory stays reserved during Wi-Fi");
    return false;
  }
  const uint32_t before = ESP.getFreeHeap();
  if (!btMemRelease(BT_MODE_BLE)) {
    LOG_ERR("BLE", "Could not release Bluetooth memory for Wi-Fi");
    return false;
  }
  LOG_INF("BLE", "Bluetooth memory released for Wi-Fi: free heap %lu before, %lu after",
          static_cast<unsigned long>(before), static_cast<unsigned long>(ESP.getFreeHeap()));
  return true;
#endif
}

bool releasedForWifi() {
#ifdef SIMULATOR
  return false;
#else
  return btMemReleased(BT_MODE_BLE);
#endif
}

}  // namespace inkhome::blemem
