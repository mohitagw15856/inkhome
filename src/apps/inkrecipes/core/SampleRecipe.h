#pragma once

// Built-in recipe so the app is useful before any card content is copied
// over, and without an SD card at all. From InkRecipes' recipes/built.
namespace inkrecipes {

constexpr char SAMPLE_RECIPE[] = R"RCP(RCP 1
M title Shakshuka
M serves 2
M time 30
I 2|tbsp|olive oil
I 1||onion, sliced
I 1||red pepper, sliced
I 2|clove|garlic, crushed
I 1|tsp|ground cumin
I 1|tsp|smoked paprika
I 1|can|chopped tomatoes
I 4||eggs
I ||salt, to taste
I ||parsley, to garnish
S 480
T Warm the oil in a lidded pan and soften
T the onion and pepper.
S 60
T Add the garlic, cumin and paprika and
T cook until fragrant.
S 600
T Pour in the tomatoes, season, and simmer
T until thickened.
S 360
T Make four wells and crack in the eggs;
T cover and cook until the whites set.
S 0
T Scatter with parsley and serve from the
T pan.
)RCP";

}  // namespace inkrecipes
