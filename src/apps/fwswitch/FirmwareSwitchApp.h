#pragma once

// Switch firmware: reboots into whatever firmware sits in the other OTA slot,
// typically the stock Xteink firmware kept beside Inkhome. Nothing is erased;
// it only rewrites the boot selection in otadata (ota_boot::switchTo).
// Works without an SD card. Heap: none beyond the activity.

#include <memory>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"

namespace inkhome::fwswitch {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"fwswitch", StrId::STR_FW_SWITCH_TITLE, StrId::STR_FW_SWITCH_SUMMARY, &create,
                                         nullptr};

class FirmwareSwitchActivity final : public Activity {
 public:
  explicit FirmwareSwitchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FirmwareSwitch", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { NoOtherFirmware, Ready, Confirming, Switching, Failed };

  void inspectSlots();
  void doSwitch();

  State state = State::NoOtherFirmware;
  char runningLabel[17] = {};
  char otherLabel[17] = {};
  char otherName[33] = {};
  char otherVersion[33] = {};
};

}  // namespace inkhome::fwswitch
