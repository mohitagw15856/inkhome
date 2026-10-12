#pragma once

// inkkit::ByteReader over an open SD file, for the cores that parse through
// Inkkit's byte streams (StoryScapes, InkQuest).

#include <HalStorage.h>

#include "ByteStream.h"

namespace inkkit {

class HalFileReader final : public ByteReader {
 public:
  explicit HalFileReader(HalFile& f) : file(f), length(static_cast<uint32_t>(f.fileSize())) {}
  bool seek(const uint32_t pos) override {
    if (pos > length || !file.seek(pos)) return false;
    cursor = pos;
    return true;
  }
  uint32_t position() const override { return cursor; }
  uint32_t size() const override { return length; }
  size_t read(void* dst, const size_t n) override {
    const int got = file.read(dst, n);
    if (got <= 0) return 0;
    cursor += static_cast<uint32_t>(got);
    return static_cast<size_t>(got);
  }

 private:
  HalFile& file;
  uint32_t length;
  uint32_t cursor = 0;
};

}  // namespace inkkit
