#include "WordLog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>

namespace inkwriter {

WordLog WordLog::parse(const std::string& text) {
  WordLog log;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    char* rest = nullptr;
    const unsigned long day = std::strtoul(line.c_str(), &rest, 10);
    if (rest == line.c_str() || day == 0) continue;
    const unsigned long words = std::strtoul(rest, nullptr, 10);
    log.add(static_cast<uint32_t>(day), static_cast<uint32_t>(words));
  }
  return log;
}

std::string WordLog::serialise() const {
  std::string out;
  out.reserve(days_.size() * 14);
  char line[32];
  for (const auto& d : days_) {
    snprintf(line, sizeof(line), "%lu %lu\n", static_cast<unsigned long>(d.day), static_cast<unsigned long>(d.words));
    out += line;
  }
  return out;
}

void WordLog::add(const uint32_t day, const uint32_t words) {
  if (day == 0 || words == 0) return;
  auto it = std::lower_bound(days_.begin(), days_.end(), day,
                             [](const WordDay& d, const uint32_t value) { return d.day < value; });
  if (it != days_.end() && it->day == day) {
    it->words += words;
  } else {
    days_.insert(it, WordDay{day, words});
  }
  if (days_.size() > MAX_DAYS) days_.erase(days_.begin(), days_.begin() + (days_.size() - MAX_DAYS));
}

uint64_t WordLog::total(const uint32_t firstDay, const uint32_t lastDay) const {
  return std::accumulate(days_.begin(), days_.end(), uint64_t{0}, [&](const uint64_t sum, const WordDay& d) {
    return d.day >= firstDay && d.day <= lastDay ? sum + d.words : sum;
  });
}

}  // namespace inkwriter
