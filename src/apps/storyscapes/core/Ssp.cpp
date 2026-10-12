// From StoryScapes (github.com/mohitagw15856/storyscapes, MIT), unchanged apart from include paths.
#include "Ssp.h"

#include <cstdlib>
#include <cstring>

namespace storyscapes {

namespace {

bool readU16(inkkit::ByteReader& r, uint16_t& out) {
  uint8_t b[2];
  if (r.read(b, 2) != 2) return false;
  out = static_cast<uint16_t>(b[0] | (b[1] << 8));
  return true;
}

bool readU32(inkkit::ByteReader& r, uint32_t& out) {
  uint8_t b[4];
  if (r.read(b, 4) != 4) return false;
  out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) |
        (static_cast<uint32_t>(b[3]) << 24);
  return true;
}

}  // namespace

bool parsePack(inkkit::ByteReader& reader, PackInfo& out, size_t maxPages) {
  out = PackInfo{};
  if (!reader.seek(0)) return false;

  uint8_t magic[4];
  if (reader.read(magic, 4) != 4 || std::memcmp(magic, "SSP1", 4) != 0) return false;

  uint16_t version = 0;
  uint16_t pageCount = 0;
  if (!readU16(reader, version) || version != 1) return false;
  if (!readU16(reader, pageCount)) return false;
  if (pageCount == 0 || pageCount > maxPages) return false;
  if (!readU16(reader, out.width) || !readU16(reader, out.height)) return false;

  uint16_t titleLen = 0;
  if (!readU16(reader, titleLen) || titleLen > 200) return false;
  out.title.resize(titleLen);
  if (titleLen && reader.read(&out.title[0], titleLen) != titleLen) return false;

  uint32_t thumbLen = 0;
  if (!readU16(reader, out.thumbWidth) || !readU16(reader, out.thumbHeight)) return false;
  if (!readU32(reader, thumbLen)) return false;
  out.thumbOffset = reader.position();
  if (!reader.seek(out.thumbOffset + thumbLen)) return false;

  out.pages.reserve(pageCount);
  for (uint16_t i = 0; i < pageCount; ++i) {
    uint8_t type = 0;
    uint8_t flags = 0;
    uint16_t reserved = 0;
    PageEntry e;
    if (reader.read(&type, 1) != 1 || reader.read(&flags, 1) != 1) return false;
    if (!readU16(reader, reserved)) return false;
    if (!readU32(reader, e.offset) || !readU32(reader, e.length)) return false;
    if (type > 1) return false;
    e.type = static_cast<PageType>(type);
    e.chapterStart = (flags & 1) != 0;
    if (e.offset + e.length > reader.size()) return false;
    out.pages.push_back(e);
  }
  return true;
}

bool readImageHeader(inkkit::ByteReader& reader, const PageEntry& page, ImageHeader& out) {
  if (page.type != PageType::Image1bpp || page.length < 8) return false;
  if (!reader.seek(page.offset)) return false;
  if (!readU16(reader, out.width) || !readU16(reader, out.height)) return false;
  if (!readU32(reader, out.bitsLength)) return false;
  if (out.bitsLength + 8 > page.length) return false;
  out.captionLength = page.length - 8 - out.bitsLength;
  return true;
}

std::string Progress::serialise() const { return "page " + std::to_string(page) + "\n"; }

Progress Progress::parse(const std::string& text) {
  Progress p;
  if (text.rfind("page ", 0) == 0) {
    p.page = static_cast<uint32_t>(strtoul(text.c_str() + 5, nullptr, 10));
  }
  return p;
}

}  // namespace storyscapes
