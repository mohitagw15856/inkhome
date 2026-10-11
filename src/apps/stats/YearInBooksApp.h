#pragma once

// Year in books: hours, days read, books finished, longest streak, most-read
// book, busiest month and favourite weekday for one year, laid out as a card.
// Confirm saves it as a BMP in /exports; Left saves it into /sleep so it joins
// the Custom sleep pictures. Up/Down change the year.

#include <memory>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"

namespace inkhome::stats {

std::unique_ptr<Activity> createYear(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor YEAR_DESCRIPTOR{"year", StrId::STR_YEAR_TITLE, StrId::STR_YEAR_SUMMARY, &createYear,
                                              nullptr};

class YearInBooksActivity final : public Activity {
 public:
  explicit YearInBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("YearInBooks", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Save : uint8_t { None, ToExports, ToSleep };
  enum class Notice : uint8_t { None, Saved, SavedSleep, Failed };

  void drawCard();

  int year = 2026;
  Save pendingSave = Save::None;
  Notice notice = Notice::None;
};

}  // namespace inkhome::stats
