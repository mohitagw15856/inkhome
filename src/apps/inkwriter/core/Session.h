// From InkWriter (github.com/mohitagw15856/inkwriter, MIT), unchanged apart from include paths.
// Portable session logic: word-count goal progress, sprint timer, and the
// daily writing streak. Time arrives as caller-supplied values so everything
// is host-testable.
#pragma once

#include <cstdint>
#include <string>

namespace inkwriter {

// Goal progress in eighths (0..8), for a subtle 8-segment indicator.
int goalEighths(int sessionWords, int goalWords);

// Sprint timer: fixed-duration countdown fed with millis.
class Sprint {
 public:
  void start(uint32_t nowMs, uint32_t durationMs) {
    startMs_ = nowMs;
    durationMs_ = durationMs;
    running_ = true;
  }
  void stop() { running_ = false; }
  bool running() const { return running_; }
  // Remaining whole seconds; 0 also flips running_ off (sprint over).
  uint32_t remainingSeconds(uint32_t nowMs);

 private:
  uint32_t startMs_ = 0;
  uint32_t durationMs_ = 0;
  bool running_ = false;
};

// Daily streak: consecutive days with any writing. Dates are day keys
// (days since epoch); state round-trips through a small text blob.
struct Streak {
  uint32_t lastDay = 0;  // last day key with writing
  uint32_t length = 0;   // streak length in days, includes lastDay

  // Records that writing happened on `dayKey`; returns the new length.
  uint32_t recordWriting(uint32_t dayKey);

  std::string serialise() const;
  static Streak parse(const std::string& text);
};

}  // namespace inkwriter
