#pragma once

// Motion sensor: live acceleration and rotation from the X3's IMU, which way
// up the reader thinks it is, and whether the experimental gestures are on.
// For trying shake and face-down and reporting how they behave. X3 only.

#include <memory>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"

namespace inkhome::motion {

std::unique_ptr<Activity> createTest(GfxRenderer& renderer, MappedInputManager& mappedInput);
bool hasImu();

constexpr apps::AppDescriptor TEST_DESCRIPTOR{"motion", StrId::STR_MOTION_TITLE, StrId::STR_MOTION_SUMMARY, &createTest,
                                              &hasImu};

class MotionTestActivity final : public Activity {
 public:
  explicit MotionTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("MotionTest", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  unsigned long lastDrawMs = 0;
};

}  // namespace inkhome::motion
