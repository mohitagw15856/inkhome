// The .rcp recipe format and the servings scaler. Pure logic, host-tested.
// From InkRecipes (github.com/mohitagw15856/inkrecipes, MIT); format spec in
// that repo's docs/FORMAT.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inkrecipes {

struct Ingredient {
  double quantity = 0;  // 0 means "no quantity" (to taste)
  std::string unit;     // may be empty
  std::string name;
  bool ticked = false;  // runtime checklist state
};

struct Step {
  uint32_t seconds = 0;            // 0 = no timer
  std::vector<std::string> lines;  // pre-wrapped for the big cooking type
};

struct Recipe {
  std::string title;
  int serves = 1;
  int minutes = 0;
  std::vector<Ingredient> ingredients;
  std::vector<Step> steps;
};

// Parses a whole .rcp document. False on bad magic or no steps.
bool parseRecipe(const std::string& text, Recipe& out);

// Scales `quantity` from `fromServes` to `toServes` with sensible rounding:
//  - quantities under 10 snap to the nearest kitchen fraction
//    (1/8, 1/4, 1/3, 1/2, 2/3, 3/4) or half unit;
//  - 10 to 100 round to whole numbers;
//  - 100 and above round to the nearest 10.
double scaleQuantity(double quantity, int fromServes, int toServes);

// Formats a scaled quantity the way a cook expects: fractions for small
// amounts ("1 1/2"), integers otherwise ("450"). Empty for zero.
std::string formatQuantity(double quantity);

// A step's pre-wrapped T lines joined back into one paragraph, so the device
// can re-wrap it for its own panel width.
std::string stepText(const Step& step);

}  // namespace inkrecipes
