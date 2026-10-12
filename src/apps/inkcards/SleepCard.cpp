#include "SleepCard.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdlib>
#include <string>

#include "core/BuiltInDecks.h"
#include "core/DeckReader.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace inkcards {

namespace {

constexpr char STATE_DIR[] = "/.crosspoint/apps/inkcards";
// "source\ncounter\n"
constexpr char SLEEP_PATH[] = "/.crosspoint/apps/inkcards/sleep.txt";
constexpr int MARGIN = 32;

#ifndef OMIT_FONTS
constexpr int BIG_FONT_ID = NOTOSANS_18_FONT_ID;
#else
constexpr int BIG_FONT_ID = UI_12_FONT_ID;
#endif

class FileReader final : public ByteReader {
 public:
  explicit FileReader(HalFile& f) : file(f), length(static_cast<uint32_t>(f.fileSize())) {}
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

void readState(std::string& source, uint32_t& counter) {
  source = "builtin:capitals";
  counter = 0;
  if (!inkhome::sdMounted() || !Storage.exists(SLEEP_PATH)) return;
  const std::string text = Storage.readFile(SLEEP_PATH).c_str();
  const size_t nl = text.find('\n');
  if (nl == std::string::npos) return;
  source = text.substr(0, nl);
  counter = static_cast<uint32_t>(strtoul(text.c_str() + nl + 1, nullptr, 10));
}

void writeState(const std::string& source, const uint32_t counter) {
  if (!inkhome::sdMounted()) return;
  Storage.mkdir(STATE_DIR, true);
  Storage.writeFile(SLEEP_PATH, String((source + "\n" + std::to_string(counter) + "\n").c_str()));
}

}  // namespace

void rememberLastDeck(const char* source, const char* /*label*/) {
  std::string current;
  uint32_t counter = 0;
  readState(current, counter);
  if (current != source) writeState(source, 0);
}

void drawSleepCard(const GfxRenderer& renderer) {
  std::string source;
  uint32_t counter = 0;
  readState(source, counter);

  HalFile file;
  std::unique_ptr<FileReader> fileReader;
  std::unique_ptr<MemoryReader> memoryReader;
  if (source == "builtin:demo") {
    memoryReader = makeUniqueNoThrow<MemoryReader>(BUILT_IN_DEMO, BUILT_IN_DEMO_SIZE);
  } else if (source.rfind("builtin:", 0) == 0 || !inkhome::sdMounted() ||
             !Storage.openFileForRead("ICD", source.c_str(), file)) {
    memoryReader = makeUniqueNoThrow<MemoryReader>(BUILT_IN_CAPITALS, BUILT_IN_CAPITALS_SIZE);
  } else {
    fileReader = makeUniqueNoThrow<FileReader>(file);
  }
  ByteReader* const reader =
      memoryReader ? static_cast<ByteReader*>(memoryReader.get()) : static_cast<ByteReader*>(fileReader.get());
  if (!reader) return;
  DeckReader deck(*reader);
  DeckCard card;
  if (!deck.open() || deck.cardCount() == 0 || !deck.readCard(counter % deck.cardCount(), card)) return;
  writeState(source, counter + 1);

  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int textWidth = width - MARGIN * 2;
  const int bigH = renderer.getLineHeight(BIG_FONT_ID);
  const int midH = renderer.getLineHeight(UI_12_FONT_ID);
  renderer.clearScreen();

  const auto front = renderer.wrappedText(BIG_FONT_ID, card.front.c_str(), textWidth, 4, EpdFontFamily::BOLD);
  const auto back = renderer.wrappedText(UI_12_FONT_ID, card.back.c_str(), textWidth, 6);
  const int blockH = static_cast<int>(front.size()) * bigH + bigH + (card.pinyin.empty() ? 0 : midH + 4) +
                     static_cast<int>(back.size()) * midH;
  int y = (height - blockH) / 2;
  renderer.drawCenteredText(SMALL_FONT_ID, y - bigH, deck.header().name.c_str(), true);
  for (const auto& l : front) {
    renderer.drawCenteredText(BIG_FONT_ID, y, l.c_str(), true, EpdFontFamily::BOLD);
    y += bigH;
  }
  y += bigH / 2;
  renderer.drawLine(width / 3, y, width * 2 / 3, y);
  y += bigH / 2;
  if (!card.pinyin.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, card.pinyin.c_str(), true, EpdFontFamily::ITALIC);
    y += midH + 4;
  }
  for (const auto& l : back) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, l.c_str(), true);
    y += midH;
  }
}

}  // namespace inkcards
