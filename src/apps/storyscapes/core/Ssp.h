// From StoryScapes (github.com/mohitagw15856/storyscapes, MIT), unchanged apart from include paths.
// The .ssp pack format (docs/FORMAT.md): header + page table parsing over
// inkkit's portable ByteReader, so the device streams payloads straight to
// the framebuffer and the host tests run without hardware.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../inkkit/ByteStream.h"

namespace storyscapes {

enum class PageType : uint8_t { Text = 0, Image1bpp = 1 };

struct PageEntry {
  PageType type = PageType::Text;
  bool chapterStart = false;
  uint32_t offset = 0;
  uint32_t length = 0;
};

struct PackInfo {
  std::string title;
  uint16_t width = 0;  // target panel geometry the pack was built for
  uint16_t height = 0;
  uint16_t thumbWidth = 0;
  uint16_t thumbHeight = 0;
  uint32_t thumbOffset = 0;  // 1bpp packed rows
  std::vector<PageEntry> pages;
};

// Parses the header and page table. Returns false on bad magic/truncation.
bool parsePack(inkkit::ByteReader& reader, PackInfo& out, size_t maxPages = 512);

// Image page payload header (at each image page's offset).
struct ImageHeader {
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t bitsLength = 0;
  uint32_t captionLength = 0;  // derived: payload length - 8 - bitsLength
};

bool readImageHeader(inkkit::ByteReader& reader, const PageEntry& page, ImageHeader& out);

// Reading-progress state blob ("page N of pack X"), round-tripped on SD.
struct Progress {
  uint32_t page = 0;
  std::string serialise() const;
  static Progress parse(const std::string& text);
};

}  // namespace storyscapes
