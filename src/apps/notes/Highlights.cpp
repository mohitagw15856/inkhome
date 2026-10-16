#include "Highlights.h"

#include <HalStorage.h>
#include <Logging.h>

#include "core/NotesFormat.h"
#include "inkhome/SdState.h"

namespace inkhome::notes {

namespace {

constexpr char HIGHLIGHTS_DIR[] = "/notes/inkhome/highlights";
constexpr size_t MAX_FILE_BYTES = 128 * 1024;

}  // namespace

bool saveHighlight(const std::string& title, const std::string& passage, const int percent) {
  if (!sdMounted() || passage.empty()) return false;
  Storage.mkdir(HIGHLIGHTS_DIR, true);
  const std::string path = std::string(HIGHLIGHTS_DIR) + "/" + fileNameFor(title.empty() ? "Untitled" : title);

  std::string text;
  {
    HalFile in;
    if (Storage.openFileForRead("HIL", path.c_str(), in)) {
      const size_t size = in.fileSize();
      if (size > MAX_FILE_BYTES) {
        LOG_ERR("HIL", "%s is full", path.c_str());
        return false;
      }
      text.assign(size, '\0');
      if (size > 0 && in.read(text.data(), size) != static_cast<int>(size)) return false;
    }
  }
  if (text.empty()) text = highlightsHeader(title);
  text += highlightEntry(passage, percent);

  // Write beside the file, then swap, so a power cut keeps the old copy.
  const std::string tmp = path + ".tmp";
  {
    HalFile out;
    if (!Storage.openFileForWrite("HIL", tmp, out)) return false;
    if (out.write(text.data(), text.size()) != text.size()) return false;
  }
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
  return Storage.rename(tmp.c_str(), path.c_str());
}

}  // namespace inkhome::notes
