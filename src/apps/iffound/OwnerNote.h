#pragma once

#include <string>

// The owner's If Found note (UTF-8, LF line ends), or empty when there is
// none or no card. Kept apart from IfFoundApp.h so the sleep screen does not
// include UI headers.
namespace inkhome::iffound {

std::string ownerNote();

}  // namespace inkhome::iffound
