#pragma once

// Inbox: books and pictures sent to the reader land in /inbox (upload them
// with CrossPoint's File Transfer over Wi-Fi, choosing the inbox folder, or
// copy them there on a computer). Confirm opens a book in the reader or a
// picture in the viewer. No new network service: File Transfer does the sending.

#include <memory>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace inkhome::inbox {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"inbox", StrId::STR_INBOX_TITLE, StrId::STR_INBOX_SUMMARY, &create, nullptr};

class InboxActivity final : public UiListActivity {
 public:
  explicit InboxActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Inbox", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<std::string> names;
  std::vector<freeink::ui::ListItem> rows;
  freeink::ui::ListItem emptyRow;
};

}  // namespace inkhome::inbox
