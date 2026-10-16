#pragma once

// InkWriter: distraction-free writing with a Bluetooth (BLE) keyboard. Native
// port of InkWriter (github.com/mohitagw15856/inkwriter, MIT): the editor and
// session cores are InkWriter's; the BLE host is the FreeInk SDK's
// BleKeyboardHost. Drafts are plain Markdown in /apps/inkwriter/drafts/ (the
// legacy /inkwriter/drafts/ is listed too). Bluetooth starts when the app
// opens and stops when it closes, so it costs nothing elsewhere. Heap: NimBLE
// (about 40 to 60 KB while open) and one draft.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "core/Editor.h"
#include "core/Session.h"

namespace inkwriter {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"inkwriter", StrId::STR_WRITER_TITLE, StrId::STR_WRITER_SUMMARY,
                                                  &create, nullptr};

// Root screen: New draft, Keyboard (pairing), then the drafts.
class DraftListActivity final : public UiListActivity {
 public:
  explicit DraftListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("InkWriter", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void refresh();

  bool bleStarted = false;
  bool wasConnected = false;
  std::string keyboardLabel;
  std::vector<std::string> paths;
  std::vector<std::string> names;
  std::vector<freeink::ui::ListItem> rows;
};

class PairActivity final : public Activity {
 public:
  PairActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("InkWriterPair", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { Scanning, Choosing, Connecting, Connected, Failed };
  void rescan();

  State state = State::Scanning;
  int selected = 0;
  uint8_t shownCount = 0;
  uint32_t passkey = 0;
  bool hasPasskey = false;
  char failure[64] = {};
};

class EditorActivity final : public Activity {
 public:
  EditorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override;

 private:
  void save();
  void loadConfig();
  void noteWriting();

  std::string path;
  std::string draftName;
  Editor editor;
  Sprint sprint;
  Streak streak;
  int goalWords = 500;
  uint32_t sprintMinutes = 15;
  int sessionStartWords = 0;
  int loggedWords = 0;  // word count at the last save that logged words
  bool focusMode = false;
  bool wroteToday = false;
  bool pendingRender = false;
  uint32_t lastKeyMs = 0;
  uint32_t lastSaveMs = 0;
  uint32_t lastSprintMinute = 0;
  int fastRefreshes = 0;
  bool wasConnected = false;
  bool refused = false;  // the draft was too large to open
};

}  // namespace inkwriter
