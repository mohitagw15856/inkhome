#include "IfFoundText.h"

// Adapted from CPR-vCodex (franssjz/cpr-vcodex, src/util/IfFoundFile.cpp), MIT.

#include <algorithm>
#include <cstdint>

namespace inkhome::iffound {

namespace {

std::string toLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
  return value;
}

std::string basename(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  const auto slash = value.find_last_of('/');
  return slash == std::string::npos ? value : value.substr(slash + 1);
}

void appendUtf8(std::string& out, const uint32_t cp) {
  if (cp == 0) return;
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

uint16_t readUnit(const std::string& v, const size_t at, const bool littleEndian) {
  const uint16_t a = static_cast<uint8_t>(v[at]);
  const uint16_t b = static_cast<uint8_t>(v[at + 1]);
  return littleEndian ? static_cast<uint16_t>(a | (b << 8)) : static_cast<uint16_t>((a << 8) | b);
}

std::string decodeUtf16(const std::string& v, const bool littleEndian, size_t at) {
  std::string out;
  out.reserve(v.size() / 2);
  while (at + 1 < v.size()) {
    uint32_t cp = readUnit(v, at, littleEndian);
    at += 2;
    if (cp >= 0xD800 && cp <= 0xDBFF && at + 1 < v.size()) {
      const uint16_t low = readUnit(v, at, littleEndian);
      if (low >= 0xDC00 && low <= 0xDFFF) {
        cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
        at += 2;
      }
    }
    appendUtf8(out, cp);
  }
  return out;
}

bool byteIs(const std::string& v, const size_t at, const uint8_t b) {
  return v.size() > at && static_cast<uint8_t>(v[at]) == b;
}

}  // namespace

bool isCandidateName(const std::string& name) {
  const std::string lower = toLowerAscii(basename(name));
  return lower == "if_found" || lower == "if_found.txt" || lower == "if_found.txt.txt";
}

std::string normalise(std::string raw) {
  if (byteIs(raw, 0, 0xEF) && byteIs(raw, 1, 0xBB) && byteIs(raw, 2, 0xBF)) {
    raw.erase(0, 3);
  } else if (byteIs(raw, 0, 0xFF) && byteIs(raw, 1, 0xFE)) {
    raw = decodeUtf16(raw, true, 2);
  } else if (byteIs(raw, 0, 0xFE) && byteIs(raw, 1, 0xFF)) {
    raw = decodeUtf16(raw, false, 2);
  } else if (raw.size() >= 4 && raw[1] == '\0' && raw[3] == '\0') {
    raw = decodeUtf16(raw, true, 0);
  } else if (raw.size() >= 4 && raw[0] == '\0' && raw[2] == '\0') {
    raw = decodeUtf16(raw, false, 0);
  }

  std::string out;
  out.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\r') {
      out.push_back('\n');
      if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
    } else {
      out.push_back(raw[i]);
    }
  }
  return out;
}

}  // namespace inkhome::iffound
