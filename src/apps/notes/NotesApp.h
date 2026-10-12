#pragma once

// Bookmarks to notes: writes each book's bookmarks (position and the opening
// words of the page) as Markdown in /notes/inkhome/<title>.md, for Obsidian
// or any notes app. Generated files live in their own folder so they never
// overwrite notes written by hand. The current book's file also refreshes
// when the reader sleeps. Uses upstream's bookmark files read-only.

#include <memory>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"

namespace inkhome::notes {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"notes", StrId::STR_NOTES_TITLE, StrId::STR_NOTES_SUMMARY, &create, nullptr};

// Writes the notes file for one book; returns the number of bookmarks written.
int exportBook(const std::string& bookPath);
// Before sleep: refresh the open book's notes.
void exportCurrentBook();

class NotesActivity final : public UiListActivity {
 public:
  explicit NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Notes", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  struct Book {
    std::string path;
    std::string title;
    std::string summary;
  };
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawFooter() override;

  std::vector<Book> books;
  std::vector<freeink::ui::ListItem> rows;
  freeink::ui::ListItem emptyRow;
};

}  // namespace inkhome::notes
