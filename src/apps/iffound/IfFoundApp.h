#pragma once

// If Found: shows the owner's contact note so a lost reader can be returned.
// The note is a plain text file the owner puts on the card, read in this order:
//   /if_found.txt (or if_found / if_found.txt.txt at the card root, as in vCodex)
//   /apps/iffound/if_found.txt
// Adapted from CPR-vCodex (franssjz/cpr-vcodex, IfFoundActivity), MIT.
// Peak heap: the note (8 KB cap) plus its wrapped lines while the screen is open.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"
#include "util/ButtonNavigator.h"

namespace inkhome::iffound {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"iffound", StrId::STR_IF_FOUND_TITLE, StrId::STR_IF_FOUND_SUMMARY, &create,
                                         nullptr};

class IfFoundActivity final : public Activity {
 public:
  explicit IfFoundActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("IfFound", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadContent();
  int bodyTop() const;
  int visibleBodyLines() const;
  int maxScroll() const;

  ButtonNavigator buttonNavigator;
  std::vector<std::string> introLines;
  std::vector<std::string> bodyLines;
  int scrollOffset = 0;
};

}  // namespace inkhome::iffound
