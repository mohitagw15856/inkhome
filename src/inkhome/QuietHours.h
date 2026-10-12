#pragma once

// Quiet hours: between 22:00 and 07:00 (local clock) the reader does a full,
// flashing refresh three times less often, so reading in the dark is gentler.
// Needs a clock (X3); without one nothing changes.
namespace inkhome {

bool quietHoursActive();
int quietHoursRefreshFrequency(int base);

}  // namespace inkhome
