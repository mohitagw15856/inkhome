#include "XthWallpaper.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>
#include <vector>

#include "Xtc/XtcTypes.h"

namespace inkhome::wallpaper {

namespace {

constexpr const char* FOLDERS[] = {"/Pushed Images", "/sleep", "/.sleep"};
constexpr size_t MAX_CANDIDATES = 64;
constexpr size_t HEADER_BYTES = 22;
constexpr size_t MAX_IMAGE_BYTES = 160 * 1024;

bool hasXthExtension(const std::string& name) {
  if (name.size() < 5) return false;
  const std::string ext = name.substr(name.size() - 4);
  return strcasecmp(ext.c_str(), ".xth") == 0 || strcasecmp(ext.c_str(), ".xtg") == 0;
}

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

bool drawXth(const GfxRenderer& renderer, const std::string& path, const sleep::Overlays& overlays) {
  HalFile file;
  if (!Storage.openFileForRead("XTH", path.c_str(), file)) return false;
  xtc::XtgPageHeader header{};
  if (file.read(&header, sizeof(header)) != static_cast<int>(sizeof(header))) return false;
  const bool twoBit = header.magic == xtc::XTH_MAGIC;
  if (!twoBit && header.magic != xtc::XTG_MAGIC) {
    LOG_ERR("XTH", "%s is not an XTH/XTG image", path.c_str());
    return false;
  }
  const int width = header.width;
  const int height = header.height;
  const size_t colBytes = (static_cast<size_t>(height) + 7) / 8;
  const size_t expected = twoBit ? static_cast<size_t>(width) * colBytes * 2
                                 : ((static_cast<size_t>(width) + 7) / 8) * static_cast<size_t>(height);
  if (width == 0 || height == 0 || expected > MAX_IMAGE_BYTES) {
    LOG_ERR("XTH", "%s: unsupported size %dx%d", path.c_str(), width, height);
    return false;
  }
  auto data = makeUniqueNoThrow<uint8_t[]>(expected);
  if (!data) {
    LOG_ERR("XTH", "OOM: %u bytes for %s", static_cast<unsigned>(expected), path.c_str());
    return false;
  }
  if (!file.seek(HEADER_BYTES) || file.read(data.get(), expected) != static_cast<int>(expected)) return false;
  file.close();

  const uint8_t* pixels = data.get();
  const size_t planeSize = static_cast<size_t>(width) * colBytes;
  auto value = [&](const int x, const int y) -> uint8_t {
    if (twoBit) {
      const size_t at = static_cast<size_t>(width - 1 - x) * colBytes + static_cast<size_t>(y / 8);
      const int shift = 7 - (y % 8);
      return static_cast<uint8_t>((((pixels[at] >> shift) & 1) << 1) | ((pixels[at + planeSize] >> shift) & 1));
    }
    const size_t at = static_cast<size_t>(y) * ((width + 7) / 8) + static_cast<size_t>(x / 8);
    return ((pixels[at] >> (7 - (x % 8))) & 1) ? 0 : 3;  // XTG: 1 = white
  };

  // Centre on the panel; a larger picture is cropped evenly.
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int offX = (screenW - width) / 2;
  const int offY = (screenH - height) / 2;
  const int x0 = offX < 0 ? -offX : 0;
  const int y0 = offY < 0 ? -offY : 0;
  const int x1 = offX < 0 ? x0 + screenW : width;
  const int y1 = offY < 0 ? y0 + screenH : height;

  // Draws pixels whose value passes `include` in the given colour.
  auto drawWhere = [&](auto include, const bool black) {
    for (int y = y0; y < y1; ++y) {
      for (int x = x0; x < x1; ++x) {
        if (include(value(x, y), x + offX, y + offY)) renderer.drawPixel(x + offX, y + offY, black);
      }
    }
  };

  renderer.clearScreen();
  if (!twoBit || overlays.any()) {
    drawWhere([](uint8_t v, int x, int y) { return ditherBlack(v, x, y); }, true);
    sleep::draw(renderer, overlays);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    LOG_DBG("XTH", "Sleep picture %s (black and white)", path.c_str());
    return true;
  }

  // Same passes as XtcReaderActivity's 2-bit pages: BW base, then the LSB and
  // MSB grey planes, then restore the BW frame for the wake-up refresh.
  drawWhere([](uint8_t v, int, int) { return v >= 1; }, true);
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
