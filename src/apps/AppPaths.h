#pragma once

#include <HalStorage.h>

#include <cstdio>

namespace inkhome::apps {

// Per-app SD layout:
//   /apps/<id>/               user content the owner copies over (recipes, decks)
//   /.crosspoint/apps/<id>/   app state the firmware writes (progress, settings)
constexpr char CONTENT_ROOT[] = "/apps";
constexpr char STATE_ROOT[] = "/.crosspoint/apps";

inline void contentDir(const char* id, char* out, size_t outSize) { snprintf(out, outSize, "%s/%s", CONTENT_ROOT, id); }
inline void stateDir(const char* id, char* out, size_t outSize) { snprintf(out, outSize, "%s/%s", STATE_ROOT, id); }

// Creates both folders for an app; cheap when they already exist.
inline void ensureDirs(const char* id) {
  char path[64];
  contentDir(id, path, sizeof(path));
  Storage.mkdir(path, true);
  stateDir(id, path, sizeof(path));
  Storage.mkdir(path, true);
}

}  // namespace inkhome::apps
