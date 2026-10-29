#pragma once

// Bluetooth memory: with NimBLE linked, the Arduino core keeps the BLE
// controller and host memory (about 36 KB) reserved from boot, whether or not
// Bluetooth ever starts. Wi-Fi needs that room. Every Wi-Fi screen ends with a
// silent restart, so it hands the memory to the heap on entry while Bluetooth
// is idle; Bluetooth is available again after the restart.

namespace inkhome::blemem {

// Release idle Bluetooth memory to the heap for the rest of this boot.
// Returns true when it is released (or already was).
bool releaseForWifi();
// Released this boot: Bluetooth cannot start again until a restart.
bool releasedForWifi();

}  // namespace inkhome::blemem
