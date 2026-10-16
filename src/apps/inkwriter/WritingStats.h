#pragma once

// What other screens need from InkWriter: words written per day (logged on
// every save, kept in /.crosspoint/apps/inkwriter/words.txt) and a sprint the
// Focus timer asks the next opened draft to start.

#include <cstdint>

namespace inkwriter {

// Adds `words` to today's count (ignored without a clock).
void logWords(uint32_t words);
// Words written on days [firstDay, lastDay] (days since 1970-01-01).
uint64_t wordsWritten(uint32_t firstDay, uint32_t lastDay);

void setPendingSprint(uint8_t minutes);
uint8_t takePendingSprint();

}  // namespace inkwriter
