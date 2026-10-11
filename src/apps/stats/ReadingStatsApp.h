#pragma once

// Reading stats: today, last 7 days, streak and total; a reading heatmap sized
// to the panel; recent books with their reading time; export and import in
// CPR-vCodex's format (/exports/stats_exported). Layout comes entirely from the
// renderer's size, so the same screen fits X3 and X4 in any orientation.
// Heap: the resident history (ReadingStatsTracker) plus a short list of indices.

#include <memory>
#include <vector>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"

namespace inkhome::stats {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"stats", StrId::STR_STATS_TITLE, StrId::STR_STATS_SUMMARY, &create, nullptr};

class ReadingStatsActivity final : public Activity {
 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Notice : uint8_t { None, Exported, ExportFailed, ConfirmImport, Imported, ImportFailed };

  void rebuildBookOrder();
  int drawSummary(int top);
  int drawHeatmap(int top);
  void drawBooks(int top, int bottom);

  std::vector<uint16_t> bookOrder;  // indices into data().books, most recent first
  int bookScroll = 0;
  int visibleBooks = 1;
  Notice notice = Notice::None;
};

}  // namespace inkhome::stats
