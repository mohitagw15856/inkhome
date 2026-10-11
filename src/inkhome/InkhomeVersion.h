#pragma once

#include <cstdio>

namespace inkhome {

// Versions look like <major>.<minor>.<patch>.<fork>[-suffix]; a missing fork
// segment counts as 0 so plain upstream tags still parse. Returns true when
// `latest` is strictly newer than `current`.
inline bool isVersionNewer(const char* latest, const char* current) {
  int l[4] = {0, 0, 0, 0};
  int c[4] = {0, 0, 0, 0};
  if (std::sscanf(latest, "%d.%d.%d.%d", &l[0], &l[1], &l[2], &l[3]) < 3) return false;
  if (std::sscanf(current, "%d.%d.%d.%d", &c[0], &c[1], &c[2], &c[3]) < 3) return false;
  for (int i = 0; i < 4; ++i) {
    if (l[i] != c[i]) return l[i] > c[i];
  }
  return false;
}

}  // namespace inkhome
