#include "AppRegistry.h"

#include <array>
#include <cassert>

#include "apps/iffound/IfFoundApp.h"

namespace inkhome::apps {

namespace {

// Launcher order. Add an app by including its header and appending its descriptor.
constexpr std::array<AppDescriptor, 1> APPS{{
    iffound::DESCRIPTOR,
}};

bool isAvailable(const AppDescriptor& app) { return app.isAvailable == nullptr || app.isAvailable(); }

}  // namespace

size_t availableCount() {
  size_t count = 0;
  for (const auto& app : APPS) {
    if (isAvailable(app)) ++count;
  }
  return count;
}

const AppDescriptor& availableAt(const size_t index) {
  size_t seen = 0;
  for (const auto& app : APPS) {
    if (!isAvailable(app)) continue;
    if (seen++ == index) return app;
  }
  // Callers index below availableCount(); an out-of-range index is a programming error.
  assert(false);
  return APPS[0];
}

}  // namespace inkhome::apps
