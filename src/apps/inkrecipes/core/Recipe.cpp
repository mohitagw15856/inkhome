// From InkRecipes (github.com/mohitagw15856/inkrecipes, MIT), unchanged apart from stepText.
#include "Recipe.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace inkrecipes {

namespace {

const double kFractions[] = {0.0, 0.125, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0};

double snapFraction(double frac) {
  double best = 0.0;
  double bestDist = 1e9;
  for (double f : kFractions) {
    const double d = std::fabs(frac - f);
    if (d < bestDist) {
      bestDist = d;
      best = f;
    }
  }
  return best;
}

}  // namespace

bool parseRecipe(const std::string& text, Recipe& out) {
  out = Recipe{};
  bool first = true;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos);
    const bool last = (end == text.size());
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (first) {
      if (line != "RCP 1") return false;
      first = false;
      continue;
    }
    if (line.empty()) {
      if (last) break;
      continue;
    }

    const char tag = line[0];
    const std::string rest = line.size() >= 2 ? line.substr(2) : std::string();
    switch (tag) {
      case 'M': {
        const size_t sp = rest.find(' ');
        const std::string key = rest.substr(0, sp);
        const std::string value = sp == std::string::npos ? "" : rest.substr(sp + 1);
        if (key == "title") out.title = value;
        if (key == "serves") out.serves = atoi(value.c_str());
        if (key == "time") out.minutes = atoi(value.c_str());
        break;
      }
      case 'I': {
        // "I <qty>|<unit>|<name>", qty is decimal or empty.
        Ingredient ing;
        const size_t bar1 = rest.find('|');
        const size_t bar2 = bar1 == std::string::npos ? std::string::npos
                                                      : rest.find('|', bar1 + 1);
        if (bar1 == std::string::npos || bar2 == std::string::npos) break;
        const std::string qty = rest.substr(0, bar1);
        ing.unit = rest.substr(bar1 + 1, bar2 - bar1 - 1);
        ing.name = rest.substr(bar2 + 1);
        ing.quantity = qty.empty() ? 0.0 : atof(qty.c_str());
        out.ingredients.push_back(ing);
        break;
      }
      case 'S': {
        Step s;
        s.seconds = static_cast<uint32_t>(strtoul(rest.c_str(), nullptr, 10));
        out.steps.push_back(s);
        break;
      }
      case 'T': {
        if (!out.steps.empty()) out.steps.back().lines.push_back(rest);
        break;
      }
      default:
        break;  // forward compatible
    }
    if (last) break;
  }
  if (out.serves < 1) out.serves = 1;
  return !out.steps.empty();
}

double scaleQuantity(double quantity, int fromServes, int toServes) {
  if (quantity <= 0 || fromServes < 1 || toServes < 1) return quantity;
  const double raw = quantity * static_cast<double>(toServes) / fromServes;
  if (raw >= 100.0) return std::round(raw / 10.0) * 10.0;
  if (raw >= 10.0) return std::round(raw);
  const double whole = std::floor(raw);
  const double snapped = snapFraction(raw - whole);
  double result = whole + snapped;
  if (result <= 0.0) result = 0.125;  // never scale an ingredient away entirely
  return result;
}

std::string formatQuantity(double q) {
  if (q <= 0) return "";
  const double whole = std::floor(q + 1e-9);
  const double frac = q - whole;
  struct Name { double value; const char* text; };
  const Name names[] = {{0.125, "1/8"}, {0.25, "1/4"}, {1.0 / 3.0, "1/3"},
                        {0.5, "1/2"},   {2.0 / 3.0, "2/3"}, {0.75, "3/4"}};
  const char* fracText = nullptr;
  for (const auto& n : names) {
    if (std::fabs(frac - n.value) < 0.02) {
      fracText = n.text;
      break;
    }
  }
  char buf[32];
  if (fracText && whole > 0) {
    std::snprintf(buf, sizeof(buf), "%d %s", static_cast<int>(whole), fracText);
  } else if (fracText) {
    std::snprintf(buf, sizeof(buf), "%s", fracText);
  } else if (std::fabs(frac) < 0.02) {
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(q)));
  } else {
    std::snprintf(buf, sizeof(buf), "%.1f", q);
  }
  return buf;
}

std::string stepText(const Step& step) {
  std::string out;
  for (const auto& line : step.lines) {
    if (!out.empty()) out.push_back(' ');
    out += line;
  }
  return out;
}

}  // namespace inkrecipes
