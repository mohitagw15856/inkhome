#include "XthWallpaper.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>

#include "Xtc/XtcTypes.h"

namespace inkhome::wallpaper {

namespace {

constexpr const char* FOLDERS[] = {"/Pushed Images", "/sleep", "/.sleep"};
constexpr size_t MAX_CANDIDATES = 64;
constexpr size_t HEADER_BYTES = 22;
constexpr size_t MAX_IMAGE_BYTES = 160 * 1024;
constexpr size_t STREAM_BYTES = 4096;  // two halves for the 2-bit planes

constexpr char PIN_DIR[] = "/.crosspoint/apps/wallpaper";
constexpr char PIN_PATH[] = "/.crosspoint/apps/wallpaper/pinned.txt";

bool hasExtension(const std::string& name, const char* ext) {
  return name.size() > 4 && strcasecmp(name.c_str() + name.size() - 4, ext) == 0;
}

bool hasXthExtension(const std::string& name) { return hasExtension(name, ".xth") || hasExtension(name, ".xtg"); }

// 4x4 Bayer thresholds for the black-and-white rendering of grey levels.
bool ditherBlack(const uint8_t value, const int x, const int y) {
  static constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  // XTH: 0 white, 1 dark grey, 2 light grey, 3 black.
  static constexpr uint8_t DARKNESS[4] = {0, 11, 5, 16};
  return BAYER[y & 3][x & 3] < DARKNESS[value & 3];
}

}  // namespace

bool pickXth(std::string& outPath, const bool bmpAvailable) {
  std::vector<std::string> candidates;
  candidates.reserve(16);
  for (const char* folder : FOLDERS) {
    if (!Storage.exists(folder)) continue;
    for (const auto& entry : Storage.listFiles(folder, MAX_CANDIDATES)) {
      if (candidates.size() >= MAX_CANDIDATES) break;
      std::string name = entry.c_str();
      const auto slash = name.find_last_of('/');
      if (slash != std::string::npos) name = name.substr(slash + 1);
      if (name.empty() || name[0] == '.' || !hasXthExtension(name)) continue;
      candidates.push_back(std::string(folder) + "/" + name);
    }
  }
  if (candidates.empty()) return false;
  if (bmpAvailable && random(2) == 0) return false;
  outPath = candidates[static_cast<size_t>(random(static_cast<long>(candidates.size())))];
  return true;
}

bool isXth(const std::string& path) { return hasXthExtension(path); }

std::vector<std::string> listAll() {
  std::vector<std::string> out;
  out.reserve(32);
  if (Storage.exists("/sleep.bmp")) out.emplace_back("/sleep.bmp");
  for (const char* folder : FOLDERS) {
    if (!Storage.exists(folder)) continue;
    std::vector<std::string> names;
    for (const auto& entry : Storage.listFiles(folder, MAX_CANDIDATES)) {
      std::string name = entry.c_str();
      const auto slash = name.find_last_of('/');
      if (slash != std::string::npos) name = name.substr(slash + 1);
      if (name.empty() || name[0] == '.') continue;
      if (hasXthExtension(name) || hasExtension(name, ".bmp")) names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    std::transform(names.begin(), names.end(), std::back_inserter(out),
                   [folder](const std::string& name) { return std::string(folder) + "/" + name; });
  }
  return out;
}

bool pinnedPath(std::string& out) {
  if (!Storage.exists(PIN_PATH)) return false;
  out = Storage.readFile(PIN_PATH).c_str();
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
  return !out.empty() && Storage.exists(out.c_str());
}

bool setPinned(const std::string& path) {
  if (path.empty()) return !Storage.exists(PIN_PATH) || Storage.remove(PIN_PATH);
  Storage.mkdir(PIN_DIR, true);
  return Storage.writeFile(PIN_PATH, String(path.c_str()));
}

bool drawXth(const GfxRenderer& renderer, const std::string& path, const sleep::Overlays& overlays) {
  HalFile file;
  if (!Storage.openFileForRead("XTH", path.c_str(), file)) {
    LOG_ERR("XTH", "Cannot open %s", path.c_str());
    return false;
  }
  xtc::XtgPageHeader header{};
  if (file.read(&header, sizeof(header)) != static_cast<int>(sizeof(header))) {
    LOG_ERR("XTH", "%s: short header", path.c_str());
    return false;
  }
  const bool twoBit = header.magic == xtc::XTH_MAGIC;
  if (!twoBit && header.magic != xtc::XTG_MAGIC) {
    LOG_ERR("XTH", "%s is not an XTH/XTG image (magic %08lx)", path.c_str(), static_cast<unsigned long>(header.magic));
    return false;
  }
  if (header.compression != 0) {
    LOG_ERR("XTH", "%s: compressed images are not supported (%u)", path.c_str(), header.compression);
    return false;
  }
  const int width = header.width;
  const int height = header.height;
  const size_t colBytes = (static_cast<size_t>(height) + 7) / 8;
  const size_t rowBytes = (static_cast<size_t>(width) + 7) / 8;
  const size_t planeSize = static_cast<size_t>(width) * colBytes;
  const size_t expected = twoBit ? planeSize * 2 : rowBytes * static_cast<size_t>(height);
  if (width == 0 || height == 0 || expected > MAX_IMAGE_BYTES || file.fileSize() < HEADER_BYTES + expected) {
    LOG_ERR("XTH", "%s: unsupported size %dx%d (file %u bytes)", path.c_str(), width, height,
            static_cast<unsigned>(file.fileSize()));
    return false;
  }

  // The picture streams from the card through one small buffer, so it works
  // with a fragmented heap (a whole 480x800 picture would need 96 KB at once).
  auto buffer = makeUniqueNoThrow<uint8_t[]>(STREAM_BYTES);
  if (!buffer) {
    LOG_ERR("XTH", "OOM: %u byte stream buffer", static_cast<unsigned>(STREAM_BYTES));
    return false;
  }
  uint8_t* const buf = buffer.get();

  // Centre on the panel; a larger picture is cropped evenly.
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int offX = (screenW - width) / 2;
  const int offY = (screenH - height) / 2;
  const int x0 = offX < 0 ? -offX : 0;
  const int y0 = offY < 0 ? -offY : 0;
  const int x1 = offX < 0 ? x0 + screenW : width;
  const int y1 = offY < 0 ? y0 + screenH : height;

  bool readOk = true;
  auto readAt = [&](const size_t offset, uint8_t* dst, const size_t n) {
    if (!readOk) return false;
    readOk = file.seek(HEADER_BYTES + offset) && file.read(dst, n) == static_cast<int>(n);
    if (!readOk) LOG_ERR("XTH", "%s: read failed at %u", path.c_str(), static_cast<unsigned>(offset));
    return readOk;
  };

  // Draws pixels whose value (0 white .. 3 black) passes `include`.
  auto drawWhere = [&](auto include, const bool black) {
    if (twoBit) {
      // Two planes of columns, stored right to left, 8 vertical pixels a byte.
      const size_t cols = std::max<size_t>(1, STREAM_BYTES / 2 / colBytes);
      for (size_t c = 0; c < static_cast<size_t>(width); c += cols) {
        const size_t n = std::min(cols, static_cast<size_t>(width) - c);
        if (!readAt(c * colBytes, buf, n * colBytes) ||
            !readAt(planeSize + c * colBytes, buf + STREAM_BYTES / 2, n * colBytes)) {
          return;
        }
        for (size_t i = 0; i < n; ++i) {
          const int x = width - 1 - static_cast<int>(c + i);
          if (x < x0 || x >= x1) continue;
          const uint8_t* hi = buf + i * colBytes;
          const uint8_t* lo = buf + STREAM_BYTES / 2 + i * colBytes;
          for (int y = y0; y < y1; ++y) {
            const int shift = 7 - (y % 8);
            const uint8_t v = static_cast<uint8_t>((((hi[y / 8] >> shift) & 1) << 1) | ((lo[y / 8] >> shift) & 1));
            if (include(v, x + offX, y + offY)) renderer.drawPixel(x + offX, y + offY, black);
          }
        }
      }
      return;
    }
    // XTG: rows of 8 pixels a byte, 1 = white.
    const size_t rows = std::max<size_t>(1, STREAM_BYTES / rowBytes);
    for (int r = y0; r < y1; r += static_cast<int>(rows)) {
      const size_t n = std::min(rows, static_cast<size_t>(y1 - r));
      if (!readAt(static_cast<size_t>(r) * rowBytes, buf, n * rowBytes)) return;
      for (size_t i = 0; i < n; ++i) {
        const int y = r + static_cast<int>(i);
        const uint8_t* row = buf + i * rowBytes;
        for (int x = x0; x < x1; ++x) {
          const uint8_t v = ((row[x / 8] >> (7 - (x % 8))) & 1) ? 0 : 3;
          if (include(v, x + offX, y + offY)) renderer.drawPixel(x + offX, y + offY, black);
        }
      }
    }
  };

  renderer.clearScreen();
  if (!twoBit || overlays.any()) {
    drawWhere([](uint8_t v, int x, int y) { return ditherBlack(v, x, y); }, true);
    if (!readOk) return false;
    sleep::draw(renderer, overlays);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    LOG_DBG("XTH", "Sleep picture %s (black and white)", path.c_str());
    return true;
  }

  // Same passes as XtcReaderActivity's 2-bit pages: BW base, then the LSB and
  // MSB grey planes, then restore the BW frame for the wake-up refresh.
  drawWhere([](uint8_t v, int, int) { return v >= 1; }, true);
  if (!readOk) return false;
  renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  renderer.clearScreen(0x00);
  drawWhere([](uint8_t v, int, int) { return v == 1; }, false);
  renderer.copyGrayscaleLsbBuffers();
  renderer.clearScreen(0x00);
  drawWhere([](uint8_t v, int, int) { return v == 1 || v == 2; }, false);
  renderer.copyGrayscaleMsbBuffers();
  renderer.displayGrayBuffer();
  renderer.clearScreen();
  drawWhere([](uint8_t v, int, int) { return v >= 1; }, true);
  renderer.cleanupGrayscaleWithFrameBuffer();
  LOG_DBG("XTH", "Sleep picture %s (greyscale)", path.c_str());
  return true;
}

}  // namespace inkhome::wallpaper
