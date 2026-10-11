#include <gtest/gtest.h>

#include <string>

#include "apps/iffound/core/IfFoundText.h"

using inkhome::iffound::isCandidateName;
using inkhome::iffound::normalise;

TEST(IfFoundText, AcceptsVcodexFileNames) {
  EXPECT_TRUE(isCandidateName("if_found.txt"));
  EXPECT_TRUE(isCandidateName("/IF_FOUND.TXT"));
  EXPECT_TRUE(isCandidateName("If_Found"));
  EXPECT_TRUE(isCandidateName("if_found.txt.txt"));
  EXPECT_FALSE(isCandidateName("if_found.md"));
  EXPECT_FALSE(isCandidateName("notes.txt"));
}

TEST(IfFoundText, StripsUtf8BomAndNormalisesNewlines) {
  EXPECT_EQ(normalise("\xEF\xBB\xBFMo\r\n07700 900000\rmo@example.com"), "Mo\n07700 900000\nmo@example.com");
}

TEST(IfFoundText, DecodesUtf16LeWithBom) {
  const std::string raw("\xFF\xFEH\0i\0\r\0\n\0\xA3\0", 12);  // "Hi\r\n£"
  EXPECT_EQ(normalise(raw), "Hi\n\xC2\xA3");
}

TEST(IfFoundText, DecodesUtf16BeWithoutBom) {
  const std::string raw("\0O\0K", 4);
  EXPECT_EQ(normalise(raw), "OK");
}

TEST(IfFoundText, DecodesSurrogatePairs) {
  const std::string raw("\xFF\xFE\x3D\xD8\x00\xDE", 6);  // U+1F600
  EXPECT_EQ(normalise(raw), "\xF0\x9F\x98\x80");
}

TEST(IfFoundText, PlainAsciiUntouched) { EXPECT_EQ(normalise("Call me"), "Call me"); }
