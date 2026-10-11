#!/usr/bin/env bash
# Runs cppcheck on Inkhome's own sources the way CI sees them: same flags as
# platformio.ini, but without resolved include paths (CI's fresh checkout has
# none), which changes some findings. Exit 1 on any finding.
set -euo pipefail
cd "$(dirname "$0")/../.."
CPPCHECK="${CPPCHECK:-$HOME/.platformio/packages/tool-cppcheck/cppcheck}"
out="$("$CPPCHECK" --enable=all --check-level=exhaustive --std=c++20 --language=c++ \
  --suppress=missingIncludeSystem --suppress=missingInclude --suppress=unusedFunction \
  --suppress=unmatchedSuppression --suppress=checkersReport --suppress=badBitmaskCheck --inline-suppr --quiet \
  --template='{file}:{line}: [{severity}:{id}] {message}' \
  src/apps src/inkhome 2>&1 || true)"
if [ -n "$out" ]; then
  echo "$out"
  exit 1
fi
echo "cppcheck (CI-style): clean"
