#include "InkRecipesApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppPaths.h"
#include "components/UITheme.h"
#include "core/SampleRecipe.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkrecipes {

namespace {

constexpr char APP_ID[] = "inkrecipes";
constexpr char LEGACY_DIR[] = "/inkrecipes";
constexpr size_t MAX_RECIPES = 100;
constexpr size_t MAX_RECIPE_BYTES = 16 * 1024;
constexpr int MIN_SERVINGS = 1;
constexpr int MAX_SERVINGS = 12;
// Timer repaint cadence: e-ink friendly corner updates, faster near the end.
constexpr unsigned long TIMER_PAINT_MS = 15000;
constexpr unsigned long TIMER_FINAL_PAINT_MS = 1000;
constexpr unsigned long TIMER_FINAL_WINDOW_MS = 10000;

#ifndef OMIT_FONTS
constexpr int COOK_FONT_ID = NOTOSANS_18_FONT_ID;
#else
constexpr int COOK_FONT_ID = UI_12_FONT_ID;
#endif

bool endsWithRcp(const std::string& name) {
  return name.size() > 4 &&
         (name.compare(name.size() - 4, 4, ".rcp") == 0 || name.compare(name.size() - 4, 4, ".RCP") == 0);
}

// "one-pan-pasta.rcp" -> "One pan pasta"
std::string labelFromFile(const std::string& file) {
  std::string label = file.substr(0, file.size() - 4);
  std::replace_if(label.begin(), label.end(), [](char c) { return c == '-' || c == '_'; }, ' ');
  if (!label.empty() && label[0] >= 'a' && label[0] <= 'z') label[0] = static_cast<char>(label[0] - 'a' + 'A');
  return label;
}

void addFolder(const char* dir, std::vector<RecipeEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& entry : Storage.listFiles(dir, MAX_RECIPES)) {
    if (out.size() >= MAX_RECIPES) return;
    std::string name = entry.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.empty() || name[0] == '.' || !endsWithRcp(name)) continue;
    out.push_back({labelFromFile(name), std::string(dir) + "/" + name});
  }
}

void formatTime(char* out, const size_t size, const unsigned long seconds) {
  snprintf(out, size, "%lu:%02lu", seconds / 60, seconds % 60);
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<RecipeListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- list

void RecipeListActivity::onEnter() {
  UiListActivity::onEnter();
  entries.clear();
  entries.reserve(8);
  entries.push_back({tr(STR_RECIPES_BUILT_IN), {}});
  if (inkhome::sdMounted()) {
    inkhome::apps::ensureDirs(APP_ID);
    char dir[48];
    inkhome::apps::contentDir(APP_ID, dir, sizeof(dir));
    addFolder(dir, entries);
    addFolder(LEGACY_DIR, entries);
  }
  std::sort(entries.begin() + 1, entries.end(),
            [](const RecipeEntry& a, const RecipeEntry& b) { return a.label < b.label; });

  rowItems.clear();
  rowItems.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    fui::ListItem item;
    item.label = entries[i].label.c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
  LOG_INF("RCP", "%u recipes", static_cast<unsigned>(entries.size()));
}

void RecipeListActivity::onExit() {
  rowItems = {};
  entries = {};
  UiListActivity::onExit();
}

int RecipeListActivity::listCount() const { return static_cast<int>(rowItems.size()); }

const char* RecipeListActivity::headerTitle() const { return tr(STR_RECIPES_TITLE); }

void RecipeListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto activity = makeUniqueNoThrow<RecipeActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!activity) {
    LOG_ERR("RCP", "OOM: recipe screen");
    return;
  }
  app.clearTapFlash();
  activityManager.pushActivity(std::move(activity));
}

void RecipeListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- recipe

bool RecipeActivity::load() {
  std::string text;
  if (entry.path.empty()) {
    text = SAMPLE_RECIPE;
  } else {
    HalFile file;
    if (!inkhome::sdMounted() || !Storage.openFileForRead("RCP", entry.path.c_str(), file)) return false;
    const size_t size = file.fileSize();
    if (size == 0 || size > MAX_RECIPE_BYTES) {
      LOG_ERR("RCP", "Recipe size %u outside 1..%u bytes", static_cast<unsigned>(size),
              static_cast<unsigned>(MAX_RECIPE_BYTES));
      return false;
    }
    text.resize(size);
    if (file.read(text.data(), size) != static_cast<int>(size)) return false;
  }
  return parseRecipe(text, recipe);
}

void RecipeActivity::onEnter() {
  Activity::onEnter();
  if (!load()) {
    mode = Mode::LoadFailed;
  } else {
    servings = std::clamp(recipe.serves, MIN_SERVINGS, MAX_SERVINGS);
    mode = Mode::Ingredients;
  }
  fullRefreshNext = true;
  requestUpdate();
}

void RecipeActivity::onExit() {
  recipe = Recipe{};
  stepLines = {};
  Activity::onExit();
}

void RecipeActivity::goToStep(const size_t index) {
  step = index;
  timerRunning = false;
  timerDone = false;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
  const std::string text = stepText(recipe.steps[step]);
  stepLines = renderer.wrappedText(COOK_FONT_ID, text.c_str(), width, 24, EpdFontFamily::BOLD);
  fullRefreshNext = true;
  requestUpdate();
}

void RecipeActivity::loop() {
  switch (mode) {
    case Mode::LoadFailed:
      if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
          mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        finish();
      }
      return;
    case Mode::Ingredients:
      loopIngredients();
      return;
    case Mode::Cooking:
      loopCooking();
      return;
  }
}

void RecipeActivity::loopIngredients() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  const int rows = ingredientRowCount();
  if (mappedInput.wasPressed(Button::Down) || mappedInput.wasPressed(Button::PageForward)) {
    selected = (selected + 1) % rows;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Up) || mappedInput.wasPressed(Button::PageBack)) {
    selected = (selected + rows - 1) % rows;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Right) && servings < MAX_SERVINGS) {
    ++servings;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Left) && servings > MIN_SERVINGS) {
    --servings;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    if (selected < static_cast<int>(recipe.ingredients.size())) {
      auto& ing = recipe.ingredients[static_cast<size_t>(selected)];
      ing.ticked = !ing.ticked;
      requestUpdate();
    } else {
      mode = Mode::Cooking;
      goToStep(0);
    }
  }
}

void RecipeActivity::loopCooking() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    timerRunning = false;
    mode = Mode::Ingredients;
    fullRefreshNext = true;
    requestUpdate();
    return;
  }
  const bool next = mappedInput.wasPressed(Button::Right) || mappedInput.wasPressed(Button::Down) ||
                    mappedInput.wasPressed(Button::PageForward);
  const bool prev = mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::Up) ||
                    mappedInput.wasPressed(Button::PageBack);
  if (next && step + 1 < recipe.steps.size()) {
    goToStep(step + 1);
    return;
  }
  if (prev && step > 0) {
    goToStep(step - 1);
    return;
  }
  if (mappedInput.wasReleased(Button::Confirm)) {
    const uint32_t seconds = recipe.steps[step].seconds;
    if (timerRunning) {
      timerRunning = false;
    } else if (seconds > 0) {
      timerRunning = true;
      timerDone = false;
      timerEndMs = millis() + seconds * 1000UL;
      lastTimerPaintMs = millis();
    }
    requestUpdate();
    return;
  }

  if (timerRunning) {
    const unsigned long now = millis();
    if (static_cast<long>(now - timerEndMs) >= 0) {
      timerRunning = false;
      timerDone = true;
      fullRefreshNext = true;  // a full flash is the e-ink alert
      requestUpdate();
    } else {
      const unsigned long remaining = timerEndMs - now;
      const unsigned long interval = remaining <= TIMER_FINAL_WINDOW_MS ? TIMER_FINAL_PAINT_MS : TIMER_PAINT_MS;
      if (now - lastTimerPaintMs >= interval) {
        lastTimerPaintMs = now;
        requestUpdate();
      }
    }
  }
}

void RecipeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  if (mode == Mode::LoadFailed) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, entry.label.c_str());
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_RECIPES_LOAD_FAILED), true,
                              EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (mode == Mode::Ingredients) {
    renderIngredients();
  } else {
    renderCooking();
  }

  const auto refresh = fullRefreshNext ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH;
  fullRefreshNext = false;
  renderer.displayBuffer(refresh);
}

void RecipeActivity::renderIngredients() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int side = metrics.contentSidePadding;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int rowHeight = lineHeight + 8;

  char subtitle[64];
  if (recipe.minutes > 0) {
    snprintf(subtitle, sizeof(subtitle), "%s %d  |  %d %s", tr(STR_RECIPES_SERVES), servings, recipe.minutes,
             tr(STR_RECIPES_MINUTES));
  } else {
    snprintf(subtitle, sizeof(subtitle), "%s %d", tr(STR_RECIPES_SERVES), servings);
  }
  const char* title = recipe.title.empty() ? entry.label.c_str() : recipe.title.c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title, subtitle);

  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int visible = std::max(1, (bottom - top) / rowHeight);
  const int rows = ingredientRowCount();
  if (selected < scrollTop) scrollTop = selected;
  if (selected >= scrollTop + visible) scrollTop = selected - visible + 1;
  scrollTop = std::clamp(scrollTop, 0, std::max(0, rows - visible));

  const int textWidth = pageWidth - side * 2 - 16;
  char line[160];
  for (int row = scrollTop; row < std::min(rows, scrollTop + visible); ++row) {
    const int y = top + (row - scrollTop) * rowHeight;
    const bool isSelected = row == selected;
    if (isSelected) renderer.fillRect(side - 6, y - 2, pageWidth - (side - 6) * 2, rowHeight - 2, true);

    if (row < static_cast<int>(recipe.ingredients.size())) {
      const auto& ing = recipe.ingredients[static_cast<size_t>(row)];
      const std::string qty = formatQuantity(scaleQuantity(ing.quantity, recipe.serves, servings));
      snprintf(line, sizeof(line), "%s  %s%s%s%s%s", ing.ticked ? "[x]" : "[  ]", qty.c_str(),
               qty.empty() || ing.unit.empty() ? "" : " ", ing.unit.c_str(), qty.empty() ? "" : " ", ing.name.c_str());
    } else {
      snprintf(line, sizeof(line), "%s", tr(STR_RECIPES_START_COOKING));
    }
    const std::string fitted = renderer.truncatedText(UI_12_FONT_ID, line, textWidth);
    const bool isStart = row == static_cast<int>(recipe.ingredients.size());
    renderer.drawText(UI_12_FONT_ID, side, y + 2, fitted.c_str(), !isSelected,
                      isStart ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }

  const bool onStart = selected == static_cast<int>(recipe.ingredients.size());
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), onStart ? tr(STR_RECIPES_COOK) : tr(STR_RECIPES_TICK), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void RecipeActivity::renderCooking() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int side = metrics.contentSidePadding;
  const Step& s = recipe.steps[step];

  char title[48];
  snprintf(title, sizeof(title), "%s %u / %u", tr(STR_RECIPES_STEP), static_cast<unsigned>(step + 1),
           static_cast<unsigned>(recipe.steps.size()));
  char timerText[32] = {};
  if (timerRunning) {
    const unsigned long now = millis();
    const unsigned long remaining = timerEndMs > now ? (timerEndMs - now + 999) / 1000 : 0;
    formatTime(timerText, sizeof(timerText), remaining);
  } else if (timerDone) {
    snprintf(timerText, sizeof(timerText), "%s", tr(STR_RECIPES_TIME_UP));
  } else if (s.seconds > 0) {
    char t[16];
    formatTime(t, sizeof(t), s.seconds);
    snprintf(timerText, sizeof(timerText), "%s %s", tr(STR_RECIPES_TIMER), t);
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title,
                 timerText[0] ? timerText : nullptr);

  const int lineHeight = renderer.getLineHeight(COOK_FONT_ID);
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  int y = top;
  for (const auto& line : stepLines) {
    if (y + lineHeight > bottom) break;
    renderer.drawText(COOK_FONT_ID, side, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += lineHeight;
  }

  if (timerDone) {
    // Inverted banner so a finished timer is visible from across the kitchen.
    const int bannerHeight = renderer.getLineHeight(COOK_FONT_ID) * 2;
    const int bannerY = bottom - bannerHeight;
    renderer.fillRect(0, bannerY, pageWidth, bannerHeight, true);
    renderer.drawCenteredText(COOK_FONT_ID, bannerY + bannerHeight / 4, tr(STR_RECIPES_TIME_UP), false,
                              EpdFontFamily::BOLD);
  }

  const char* confirm = timerRunning ? tr(STR_RECIPES_STOP) : (s.seconds > 0 ? tr(STR_RECIPES_START) : "");
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, step > 0 ? tr(STR_RECIPES_PREV) : "",
                                            step + 1 < recipe.steps.size() ? tr(STR_RECIPES_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

}  // namespace inkrecipes
