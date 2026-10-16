#include "SendNote.h"

#include <HalStorage.h>
#include <Logging.h>
#include <WebServer.h>

#include <string>

#include "apps/notes/core/NotesFormat.h"
#include "inkhome/SdState.h"
#include "network/html/SendPageHtml.generated.h"

namespace inkhome::web {

namespace {

constexpr char INBOX_DIR[] = "/inbox";
constexpr size_t MAX_TEXT_BYTES = 32 * 1024;

}  // namespace

void serveSendPage(WebServer& server) {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", SendPageHtml, sizeof(SendPageHtml));
}

void handleSendNote(WebServer& server) {
  const String title = server.arg("title");
  const String link = server.arg("link");
  const String text = server.arg("text");
  if (text.isEmpty() && link.isEmpty()) {
    server.send(400, "text/plain", "Nothing to send");
    return;
  }
  if (text.length() > MAX_TEXT_BYTES) {
    server.send(413, "text/plain", "Too long (32 KB at most)");
    return;
  }
  if (!sdMounted()) {
    server.send(503, "text/plain", "No SD card");
    return;
  }
  const std::string heading = title.isEmpty() ? std::string("Note") : std::string(title.c_str());
  Storage.mkdir(INBOX_DIR, true);

  // "Title.md", or "Title 2.md" and so on when the name is taken.
  std::string base = notes::fileNameFor(heading);
  base.resize(base.size() - 3);  // drop ".md"
  std::string path = std::string(INBOX_DIR) + "/" + base + ".md";
  for (int n = 2; Storage.exists(path.c_str()) && n < 100; ++n) {
    path = std::string(INBOX_DIR) + "/" + base + " " + std::to_string(n) + ".md";
  }

  std::string body = "# " + heading + "\n\n";
  if (!link.isEmpty()) body += std::string(link.c_str()) + "\n\n";
  body += text.c_str();
  body += "\n";
  HalFile out;
  if (!Storage.openFileForWrite("SEND", path, out) || out.write(body.data(), body.size()) != body.size()) {
    LOG_ERR("SEND", "Could not write %s", path.c_str());
    server.send(500, "text/plain", "Could not save on the card");
    return;
  }
  LOG_INF("SEND", "Saved %s", path.c_str());
  server.send(200, "text/plain", path.substr(sizeof(INBOX_DIR)).c_str());
}

}  // namespace inkhome::web
