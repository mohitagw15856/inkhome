#pragma once

#include <HalStorage.h>

#include <cstdlib>

namespace inkhome {

// Simulator-only: INKHOME_SIM_NO_SD=1 exercises the no-card start-up path,
// since the simulator's card (a host folder) always mounts.
inline bool simulateNoSd() {
#ifdef SIMULATOR
  return std::getenv("INKHOME_SIM_NO_SD") != nullptr;
#else
  return false;
#endif
}

inline bool sdMounted() { return !simulateNoSd() && Storage.ready(); }

}  // namespace inkhome
