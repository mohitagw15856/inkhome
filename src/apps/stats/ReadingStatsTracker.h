#pragma once

// Records reading time into /.crosspoint/reading_stats.json (vCodex format 6).
// Driven from the main loop, so no reader activity needs to know about it:
// time counts while a reader is open and the owner has touched a button (or
// tilted to turn) in the last few minutes.
//
// Heap: the parsed history stays resident once loaded (books, days, a 128-entry
// session log). Files larger than MAX_FILE_BYTES are left alone and tracking is
// paused, rather than risking an out-of-memory parse.

#include <cstddef>
#include <cstdint>

#include "core/ReadingStatsData.h"

namespace inkhome::stats {

constexpr char STATS_PATH[] = "/.crosspoint/reading_stats.json";
constexpr char EXPORT_PATH[] = "/exports/stats_exported";  // same name vCodex imports from
constexpr size_t MAX_FILE_BYTES = 96 * 1024;

enum class LoadState : uint8_t { NotLoaded, Ready, TooLarge, Unreadable, NoCard };

// Main loop, every iteration.
void tick(bool readerActive, bool userActivity);
// Before deep sleep: closes the session and saves.
void flush();

// For the stats screens. Loads on first use.
LoadState ensureLoaded();
const ReadingStats& data();
// Days since 1970-01-01 in local time, or 0 when the clock is not set.
uint32_t todayOrdinal();
// Time in the open session that is not yet folded into data().
uint64_t pendingMs();

bool exportTo(const char* path = EXPORT_PATH);
bool importFrom(const char* path = EXPORT_PATH);

}  // namespace inkhome::stats
