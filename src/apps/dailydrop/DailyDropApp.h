#pragma once

// DailyDrop: a daily digest (.drop: news, weather, calendar, a quote) read
// offline. Native port of DailyDrop (github.com/mohitagw15856/dailydrop, MIT).
// Digests come from /apps/dailydrop/ and the legacy /dailydrop/. "Fetch today's
// drop" uses the Wi-Fi networks already saved in CrossPoint and the URL in
// /apps/dailydrop/config.txt (url=...), downloads once, switches Wi-Fi off and
// restarts quietly (as CrossPoint's own Wi-Fi screens do). Nothing runs in the
// background. Heap: one digest (24 KB cap) while it is open.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace dailydrop {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"dailydrop", StrId::STR_DROP_TITLE, StrId::STR_DROP_SUMMARY, &create,
                                                  nullptr};

class DropListActivity final : public UiListActivity {
 public:
  explicit DropListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("DailyDrop", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::string url;
  std::vector<std::string> paths;  // newest first
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> rows;
};

class DropViewActivity final : public Activity {
 public:
  DropViewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
      : Activity("DailyDropView", renderer, mappedInput), path(std::move(path)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  struct Line {
    enum class Kind : uint8_t { Heading, Text, Rule } kind;
    std::string text;
  };
  struct Section {
    std::string name;
    std::vector<Line> lines;
  };

 private:
  std::string path;
  std::string title;
  std::vector<Section> sections;
  size_t section = 0;
  int scroll = 0;
  int visibleLines = 1;
  bool loaded = false;
};

class FetchActivity final : public Activity {
 public:
  FetchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string url)
      : Activity("DailyDropFetch", renderer, mappedInput), url(std::move(url)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { Connecting, Downloading, Done, Failed };
  void download();

  std::string url;
  State state = State::Connecting;
  bool started = false;
};

}  // namespace dailydrop
