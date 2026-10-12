// From InkWriter (github.com/mohitagw15856/inkwriter, MIT), unchanged apart from include paths.
#include "Session.h"

#include <cstdio>
#include <cstdlib>

namespace inkwriter {

int goalEighths(int sessionWords, int goalWords) {
  if (goalWords <= 0) return 0;
  if (sessionWords <= 0) return 0;
  const long long scaled = static_cast<long long>(sessionWords) * 8 / goalWords;
  return scaled >= 8 ? 8 : static_cast<int>(scaled);
}

uint32_t Sprint::remainingSeconds(uint32_t nowMs) {
  if (!running_) return 0;
  const uint32_t elapsed = nowMs - startMs_;
  if (elapsed >= durationMs_) {
    running_ = false;
    return 0;
  }
  return (durationMs_ - elapsed + 999) / 1000;
}

uint32_t Streak::recordWriting(uint32_t dayKey) {
  if (lastDay == dayKey) {
    if (length == 0) length = 1;
  } else if (lastDay != 0 && dayKey == lastDay + 1) {
    ++length;
  } else {
    length = 1;
  }
  lastDay = dayKey;
  return length;
}

std::string Streak::serialise() const {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "day %lu\nlen %lu\n", static_cast<unsigned long>(lastDay),
                static_cast<unsigned long>(length));
  return buf;
}

Streak Streak::parse(const std::string& text) {
  Streak s;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    if (line.rfind("day ", 0) == 0) {
      s.lastDay = static_cast<uint32_t>(strtoul(line.c_str() + 4, nullptr, 10));
    } else if (line.rfind("len ", 0) == 0) {
      s.length = static_cast<uint32_t>(strtoul(line.c_str() + 4, nullptr, 10));
    }
  }
  return s;
}

}  // namespace inkwriter
