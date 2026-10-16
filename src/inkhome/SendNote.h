#pragma once

// File Transfer's "Send a note" page: text or a link typed on a phone or
// computer arrives in /inbox as a Markdown file, opened from Apps, Inbox.

class WebServer;

namespace inkhome::web {

void serveSendPage(WebServer& server);
void handleSendNote(WebServer& server);

}  // namespace inkhome::web
