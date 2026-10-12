#include "InboxApp.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkhome::inbox {

namespace {
constexpr char DIR[] = "/inbox";
constexpr int MAX_FILES = 100;

bool readable(const std::string& name) {
  return FsHelpers::hasEpubExtension(name) || FsHelpers::hasTxtExtension(name) || FsHelpers::hasXtcExtension(name) ||
         FsHelpers::hasMarkdownExtension(name) || FsHelpers::hasBmpExtension(name) || FsHelpers::hasPngExtension(name);
}
}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<InboxActivity>(renderer, mappedInput);
}

void InboxActivity::onEnter() {
  UiListActivity::onEnter();
  names.clear();
  if (sdMounted()) {
    Storage.mkdir(DIR, true);
    for (const auto& e : Storage.listFiles(DIR, MAX_FILES)) {
      std::string name = e.c_str();
      const auto slash = name.find_last_of('/');
      if (slash != std::string::npos) name = name.substr(slash + 1);
      if (!name.empty() && name[0] != '.' && readable(name)) names.push_back(name);
    }
    std::sort(names.begin(), names.end());
  }
  rows.assign(names.size(), fui::ListItem{});
  for (size_t i = 0; i < names.size(); ++i) {
    rows[i].label = names[i].c_str();
    rows[i].actionValue = static_cast<int16_t>(i);
  }
}

void InboxActivity::onExit() {
  rows = {};
  names = {};
  UiListActivity::onExit();
}

int InboxActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* InboxActivity::headerTitle() const { return tr(STR_INBOX_TITLE); }

void InboxActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  // The reader handles books and BMP pictures alike.
  activityManager.goToReader(std::string(DIR) + "/" + names[static_cast<size_t>(index)]);
}

void InboxActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  if (rows.empty()) {
    emptyRow.label = tr(STR_INBOX_EMPTY);
    emptyRow.enabled = false;
    props.items = &emptyRow;
    props.count = 1;
    props.inputMask = fui::InputNone;
    screen.list(props);
    return;
  }
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

}  // namespace inkhome::inbox
