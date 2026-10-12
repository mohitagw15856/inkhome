#include "BatteryHealth.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

#ifndef SIMULATOR
#include <BoardConfig.h>
#include <Wire.h>
#endif

namespace inkhome::battery {

namespace {

constexpr uint8_t BQ27220_ADDR = 0x55;
// BQ27220 standard commands (TI SLUUBD4), all 16-bit little endian.
constexpr uint8_t REG_TEMPERATURE = 0x06;  // 0.1 K
constexpr uint8_t REG_VOLTAGE = 0x08;      // mV
constexpr uint8_t REG_CURRENT = 0x0C;      // mA, signed
constexpr uint8_t REG_REMAINING = 0x10;    // mAh
constexpr uint8_t REG_FULL_CHARGE = 0x12;  // mAh
constexpr uint8_t REG_CYCLES = 0x2A;
constexpr uint8_t REG_SOC = 0x2C;     // %
constexpr uint8_t REG_HEALTH = 0x2E;  // %
constexpr uint8_t REG_DESIGN = 0x3C;  // mAh
constexpr char LOG_DIR[] = "/.crosspoint/apps/battery";
constexpr char LOG_PATH[] = "/.crosspoint/apps/battery/history.csv";
constexpr size_t MAX_LOG_BYTES = 16 * 1024;
constexpr unsigned long REFRESH_MS = 30000;

#ifndef SIMULATOR
bool reg16(const uint8_t reg, uint16_t& out) {
  Wire.beginTransmission(BQ27220_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(BQ27220_ADDR, static_cast<uint8_t>(2), static_cast<uint8_t>(true)) < 2) return false;
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  out = static_cast<uint16_t>(lo | (hi << 8));
  return true;
}
#endif

// day,fullChargeMah,cycles,health
struct LogEntry {
  uint32_t day;
  uint16_t fullCharge;
};

std::vector<LogEntry> readLog() {
  std::vector<LogEntry> out;
  if (!sdMounted() || !Storage.exists(LOG_PATH)) return out;
  HalFile file;
  if (!Storage.openFileForRead("BAT", LOG_PATH, file) || file.fileSize() > MAX_LOG_BYTES) return out;
  std::string text(file.fileSize(), '\0');
  if (file.read(text.data(), text.size()) != static_cast<int>(text.size())) return out;
  out.reserve(64);
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    char* next = nullptr;
    const unsigned long day = strtoul(line.c_str(), &next, 10);
    if (!next || *next != ',') continue;
    const unsigned long fcc = strtoul(next + 1, nullptr, 10);
    out.push_back({static_cast<uint32_t>(day), static_cast<uint16_t>(fcc)});
  }
  return out;
}

}  // namespace

bool hasGauge() {
#ifdef SIMULATOR
  return true;
#else
  return BoardConfig::ACTIVE.batteryGauge.gaugeAddr == BQ27220_ADDR;
#endif
}

GaugeReading read() {
  GaugeReading r;
#ifdef SIMULATOR
  r = {true, 87, 3982, -38, 1305, 1450, 1500, 42, 96, 241};
#else
  if (!hasGauge()) return r;
  powerManager.getBatteryPercentage();  // brings the gauge's I2C bus up if it is not already
  uint16_t v = 0;
  if (!reg16(REG_SOC, v)) return r;
  r.percent = v;
  if (reg16(REG_VOLTAGE, v)) r.millivolts = v;
  if (reg16(REG_CURRENT, v)) r.currentMa = static_cast<int16_t>(v);
  if (reg16(REG_REMAINING, v)) r.remainingMah = v;
  if (reg16(REG_FULL_CHARGE, v)) r.fullChargeMah = v;
  if (reg16(REG_DESIGN, v)) r.designMah = v;
  if (reg16(REG_CYCLES, v)) r.cycles = v;
  if (reg16(REG_HEALTH, v)) r.healthPercent = v;
  if (reg16(REG_TEMPERATURE, v)) r.tenthsC = static_cast<int16_t>(static_cast<int>(v) - 2731);
  r.valid = true;
#endif
  return r;
}

void logDaily(const uint32_t dayOrdinal) {
  if (dayOrdinal == 0 || !hasGauge() || !sdMounted()) return;
  const auto entries = readLog();
  if (!entries.empty() && entries.back().day == dayOrdinal) return;
  const GaugeReading r = read();
  if (!r.valid || r.fullChargeMah == 0) return;
  Storage.mkdir(LOG_DIR, true);
  std::string text = Storage.exists(LOG_PATH) ? std::string(Storage.readFile(LOG_PATH).c_str()) : std::string();
  // Keep the file small: drop the oldest days once it nears the cap.
  while (text.size() > MAX_LOG_BYTES - 64) {
    const size_t cut = text.find('\n');
    if (cut == std::string::npos) {
      text.clear();
      break;
    }
    text.erase(0, cut + 1);
  }
  char line[48];
  snprintf(line, sizeof(line), "%u,%u,%u,%u\n", static_cast<unsigned>(dayOrdinal),
           static_cast<unsigned>(r.fullChargeMah), static_cast<unsigned>(r.cycles),
           static_cast<unsigned>(r.healthPercent));
  text += line;
  Storage.writeFile(LOG_PATH, String(text.c_str()));
}

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<BatteryHealthActivity>(renderer, mappedInput);
}

void BatteryHealthActivity::onEnter() {
  Activity::onEnter();
  reading = read();
  lastReadMs = millis();
  requestUpdate();
}

void BatteryHealthActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (millis() - lastReadMs >= REFRESH_MS) {
    reading = read();
    lastReadMs = millis();
    requestUpdate();
  }
}

void BatteryHealthActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  const int bigH = renderer.getLineHeight(UI_12_FONT_ID);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_BATTERY_TITLE));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  if (!reading.valid) {
    renderer.drawText(UI_12_FONT_ID, side, y, tr(STR_BATTERY_UNAVAILABLE), true);
  } else {
    char value[64];
    auto row = [&](const char* label, const char* text) {
      renderer.drawText(UI_10_FONT_ID, side, y, label, true);
      const int w = renderer.getTextWidth(UI_12_FONT_ID, text, EpdFontFamily::BOLD);
      renderer.drawText(UI_12_FONT_ID, width - side - w, y - (bigH - smallH) / 2, text, true, EpdFontFamily::BOLD);
      y += bigH + 6;
    };
    snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(reading.healthPercent));
    row(tr(STR_BATTERY_HEALTH), value);
    snprintf(value, sizeof(value), "%u / %u mAh", static_cast<unsigned>(reading.fullChargeMah),
             static_cast<unsigned>(reading.designMah));
    row(tr(STR_BATTERY_CAPACITY), value);
    snprintf(value, sizeof(value), "%u", static_cast<unsigned>(reading.cycles));
    row(tr(STR_BATTERY_CYCLES), value);
    snprintf(value, sizeof(value), "%u%%  %u mV", static_cast<unsigned>(reading.percent),
             static_cast<unsigned>(reading.millivolts));
    row(tr(STR_BATTERY_CHARGE), value);
    snprintf(value, sizeof(value), "%+d mA", static_cast<int>(reading.currentMa));
    row(reading.currentMa > 0 ? tr(STR_BATTERY_CHARGING) : tr(STR_BATTERY_DRAW), value);
    snprintf(value, sizeof(value), "%d.%d C", reading.tenthsC / 10, std::abs(reading.tenthsC % 10));
    row(tr(STR_BATTERY_TEMPERATURE), value);

    // Full-charge capacity over the logged days.
    const auto log = readLog();
    y += metrics.verticalSpacing;
    renderer.drawText(UI_10_FONT_ID, side, y, tr(STR_BATTERY_HISTORY), true);
    y += smallH + 4;
    const int chartBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
    const int chartH = std::max(30, chartBottom - y);
    const int chartW = width - side * 2;
    renderer.drawRect(side, y, chartW, chartH);
    if (log.size() >= 2) {
      uint16_t lo = log.front().fullCharge;
      uint16_t hi = lo;
      for (const auto& e : log) {
        lo = std::min(lo, e.fullCharge);
        hi = std::max(hi, e.fullCharge);
      }
      if (hi == lo) hi = lo + 1;
      const size_t first = log.size() > 120 ? log.size() - 120 : 0;
      const size_t count = log.size() - first;
      int px = -1;
      int py = -1;
      for (size_t i = 0; i < count; ++i) {
        const int x = side + 4 + static_cast<int>(i * (chartW - 8) / (count - 1));
        const int yy = y + chartH - 4 - (log[first + i].fullCharge - lo) * (chartH - 8) / (hi - lo);
        if (px >= 0) renderer.drawLine(px, py, x, yy);
        px = x;
        py = yy;
      }
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, y + chartH / 2 - smallH / 2, tr(STR_BATTERY_NO_HISTORY), true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkhome::battery
