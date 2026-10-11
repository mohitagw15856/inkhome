#pragma once

// Text handling for the If Found app. Pure C++ so it runs in host tests.
// Adapted from CPR-vCodex (franssjz/cpr-vcodex, src/util/IfFoundFile.cpp), MIT.

#include <cstddef>
#include <string>

namespace inkhome::iffound {

// Largest file read from the card; a contact note never needs more.
constexpr size_t MAX_BYTES = 8192;

// True for the root file names vCodex accepts, compared case-insensitively:
// if_found, if_found.txt and the Windows double-extension if_found.txt.txt.
bool isCandidateName(const std::string& name);

// Decodes UTF-8 (with or without BOM) and UTF-16 LE/BE (with or without BOM)
// to UTF-8, and turns CRLF and lone CR into LF.
std::string normalise(std::string raw);

}  // namespace inkhome::iffound
