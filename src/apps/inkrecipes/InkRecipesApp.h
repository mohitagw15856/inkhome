#pragma once

// InkRecipes: recipe list, ingredient checklist with a servings scaler, and a
// cooking mode with one step per page in large type and per-step timers.
// Native port of InkRecipes (github.com/mohitagw15856/inkrecipes, MIT).
//
// Recipes (.rcp, see that repo's docs/FORMAT.md) are read from /apps/inkrecipes/
// and the legacy /inkrecipes/ folder; one built-in recipe works without a card.
// Peak heap: one parsed recipe (file capped at 16 KB) plus the wrapped step.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "core/Recipe.h"

namespace inkrecipes {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"inkrecipes", StrId::STR_RECIPES_TITLE, StrId::STR_RECIPES_SUMMARY,
                                                  &create, nullptr};

// Empty path means the built-in recipe.
struct RecipeEntry {
  std::string label;
  std::string path;
};

class RecipeListActivity final : public UiListActivity {
 public:
  explicit RecipeListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("InkRecipes", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<RecipeEntry> entries;
  std::vector<freeink::ui::ListItem> rowItems;
};

class RecipeActivity final : public Activity {
 public:
  RecipeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, RecipeEntry entry)
      : Activity("Recipe", renderer, mappedInput), entry(std::move(entry)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // Only while a step timer is running; reading a recipe lets the reader sleep as usual.
  bool preventAutoSleep() override { return mode == Mode::Cooking && timerRunning; }

 private:
  enum class Mode : uint8_t { Ingredients, Cooking, LoadFailed };

  bool load();
  void loopIngredients();
  void loopCooking();
  void renderIngredients();
  void renderCooking();
  void goToStep(size_t index);
  int ingredientRowCount() const { return static_cast<int>(recipe.ingredients.size()) + 1; }  // + "Start cooking"

  RecipeEntry entry;
  Recipe recipe;
  Mode mode = Mode::Ingredients;
  int servings = 1;
  int selected = 0;
  int scrollTop = 0;
  size_t step = 0;
  std::vector<std::string> stepLines;  // current step wrapped for the panel

  bool timerRunning = false;
  bool timerDone = false;
  unsigned long timerEndMs = 0;
  unsigned long lastTimerPaintMs = 0;
  bool fullRefreshNext = false;
};

}  // namespace inkrecipes
