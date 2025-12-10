#!/usr/bin/env bash
set -uo pipefail

TEST_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN_DIR="$TEST_DIR/build/bin"

# Ensure output directory exists (used by some tests)
mkdir -p "$TEST_DIR/output"

if [ ! -d "$BIN_DIR" ]; then
  echo "Tests not built. Run test/scripts/build_tests.sh first."
  exit 1
fi

echo "Running tests in $BIN_DIR"
total=0
failures=0
failed_list=()
for exe in "$BIN_DIR"/*; do
  if [ -x "$exe" ]; then
    name=$(basename "$exe")
    echo "\n--- Running $name ---"
    "$exe"
    rc=$?
    total=$((total + 1))
    if [ $rc -ne 0 ]; then
      echo "Test $name failed with exit code $rc"
      failures=$((failures + 1))
      failed_list+=("$name (exit $rc)")
    fi
  fi
done

echo "\nRan $total test(s). Failures: $failures"
if [ $failures -eq 0 ]; then
  echo "All tests passed"
  exit 0
else
  echo "Failed tests:"
  for f in "${failed_list[@]}"; do
    echo "  - $f"
  done
  exit 1
fi
