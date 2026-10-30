#!/usr/bin/env bash
# Builds and tests the project inside Docker. Usage:
#   scripts/docker-test.sh                 # g++, Debug
#   scripts/docker-test.sh clang++         # clang++, Debug
#   scripts/docker-test.sh g++ ON          # g++ with ASan + UBSan
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${1:-g++}"
sanitize="${2:-OFF}"
docker build --target test --progress=plain \
  --build-arg CXX_COMPILER="${compiler}" --build-arg SANITIZE="${sanitize}" .
