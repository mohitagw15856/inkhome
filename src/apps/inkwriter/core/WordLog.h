#pragma once

// Words written per day in InkWriter, kept as "day words" lines (days since
// 1970-01-01). Pure, host-tested; the app persists it on the card.

#include <cstdint>
#include <string>
#include <vector>

namespace inkwriter {

struct WordDay {
  uint32_t day = 0;
  uint32_t words = 0;
};

class WordLog {
 public:
  static constexpr size_t MAX_DAYS = 800;  // a little over two years

  static WordLog parse(const std::string& text);
  std::string serialise() const;

  void add(uint32_t day, uint32_t words);
  // Words on days in [firstDay, lastDay].
  uint64_t total(uint32_t firstDay, uint32_t lastDay) const;
  const std::vector<WordDay>& days() const { return days_; }

 private:
  std::vector<WordDay> days_;  // ascending by day
};

}  // namespace inkwriter
