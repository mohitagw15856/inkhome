#include <gtest/gtest.h>

#include "apps/inkrecipes/core/Recipe.h"
#include "apps/inkrecipes/core/SampleRecipe.h"

using namespace inkrecipes;

TEST(InkRecipes, BuiltInRecipeParses) {
  Recipe r;
  ASSERT_TRUE(parseRecipe(SAMPLE_RECIPE, r));
  EXPECT_EQ(r.title, "Shakshuka");
  EXPECT_EQ(r.serves, 2);
  EXPECT_EQ(r.ingredients.size(), 10u);
  EXPECT_EQ(r.steps.size(), 5u);
  EXPECT_EQ(r.steps[0].seconds, 480u);
}

TEST(InkRecipes, StepTextJoinsWrappedLines) {
  Recipe r;
  ASSERT_TRUE(parseRecipe(SAMPLE_RECIPE, r));
  EXPECT_EQ(stepText(r.steps[0]), "Warm the oil in a lidded pan and soften the onion and pepper.");
}

TEST(InkRecipes, RejectsWrongMagicAndEmpty) {
  Recipe r;
  EXPECT_FALSE(parseRecipe("RCP 2\nS 0\nT x\n", r));
  EXPECT_FALSE(parseRecipe("RCP 1\nM title No steps\n", r));
  EXPECT_FALSE(parseRecipe("", r));
}

TEST(InkRecipes, CrlfAndUnknownTagsTolerated) {
  Recipe r;
  ASSERT_TRUE(parseRecipe("RCP 1\r\nM title T\r\nX future\r\nS 0\r\nT one\r\n", r));
  EXPECT_EQ(r.title, "T");
  EXPECT_EQ(stepText(r.steps[0]), "one");
}

TEST(InkRecipes, ScalingSnapsToKitchenFractions) {
  EXPECT_DOUBLE_EQ(scaleQuantity(1, 2, 3), 1.5);
  EXPECT_EQ(formatQuantity(scaleQuantity(1, 2, 3)), "1 1/2");
  EXPECT_EQ(formatQuantity(scaleQuantity(2, 4, 1)), "1/2");
  EXPECT_EQ(formatQuantity(scaleQuantity(400, 2, 3)), "600");
  EXPECT_GT(scaleQuantity(0.125, 12, 1), 0.0);  // never scales to zero
  EXPECT_EQ(formatQuantity(0), "");
}
