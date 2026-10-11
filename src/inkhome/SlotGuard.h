#pragma once

namespace inkhome {

// True when the other OTA slot holds a bootable image that is not this
// firmware (normally the stock Xteink firmware kept beside Inkhome). The
// built-in Wi-Fi and SD updaters write to that slot, so they refuse while this
// is true; updates then go through the web flasher, which picks the slot by
// identifying stock instead of by "not running".
bool otherSlotHoldsOtherFirmware();

}  // namespace inkhome
