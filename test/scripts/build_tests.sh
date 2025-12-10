#!/usr/bin/env bash
set -euo pipefail

# This script should live in test/scripts and be invoked from the workspace root
TEST_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOT_DIR="$(cd "$TEST_DIR/.." && pwd)"
BUILD_DIR="$TEST_DIR/build"

mkdir -p "$BUILD_DIR"
cbuild_type=${CMAKE_BUILD_TYPE:-Debug}
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=$cbuild_type
cmake --build "$BUILD_DIR" --config ${CMAKE_BUILD_TYPE:-Debug}

echo "Built tests. Executables are in $BUILD_DIR/bin"
