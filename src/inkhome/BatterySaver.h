#pragma once

// Battery saver: with the setting on, below 20% and not on USB, tilt page
// turns stop (the motion sensor sleeps), the reader's full refresh comes half
// as often and the device sleeps after at most 3 minutes idle. The battery is
// read at most every 30 seconds.

#include <cstdint>

namespace inkhome::power {

bool saverActive();
uint8_t tiltMode(uint8_t configured);
unsigned long sleepTimeoutMs(unsigned long configured);
int refreshFrequency(int base);

}  // namespace inkhome::power
