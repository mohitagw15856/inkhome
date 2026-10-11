#pragma once

#include <I18n.h>

#include <cstddef>
#include <memory>

class Activity;
class GfxRenderer;
class MappedInputManager;

namespace inkhome::apps {

// One on-device app. Each app lives in src/apps/<id>/ and registers a single
// descriptor in AppRegistry.cpp; nothing else in the firmware knows about it.
struct AppDescriptor {
  // Stable folder name: user content in /apps/<id>/, state in /.crosspoint/apps/<id>/.
  const char* id;
  StrId nameId;
  StrId summaryId;
  // Creates the app's root activity. Returns nullptr on allocation failure.
  std::unique_ptr<Activity> (*create)(GfxRenderer& renderer, MappedInputManager& mappedInput);
  // Hardware or build gate; nullptr means always available.
  bool (*isAvailable)();
};

// Apps that are compiled in and available on this device, in launcher order.
size_t availableCount();
const AppDescriptor& availableAt(size_t index);

}  // namespace inkhome::apps
