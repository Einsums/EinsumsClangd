#!/usr/bin/env bash
# Tiny test runner for EinsumsTidy checks. Used by ctest; can also be run
# manually:
#
#   ./run_test.sh <path-to-EinsumsTidy.dylib> <test.cpp> "<expected-substring>"
#
# Exits 0 if every line containing "EXPECT:" gets a diagnostic, AND the
# diagnostic's text contains <expected-substring>. Exits non-zero otherwise.
#
# We don't use the upstream ``check_clang_tidy.py`` because it ships only
# inside an LLVM source tree; binary distributions (including the conda env
# we use) drop it. A sed-and-grep harness keeps the test self-contained.
set -euo pipefail

PLUGIN="$1"
TEST_FILE="$2"
EXPECTED="$3"

CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"

# Suppress the standard-library / system-header noise; we only care about
# the einsums-* checks. ``--quiet`` keeps the summary line quiet too.
output=$("$CLANG_TIDY" \
    --load="$PLUGIN" \
    --checks='-*,einsums-*' \
    --quiet \
    "$TEST_FILE" \
    -- -std=c++20 \
    2>&1 || true)

echo "$output"

# Every "EXPECT:" line should produce at least one diagnostic line whose
# message includes $EXPECTED. We extract the line numbers tagged EXPECT,
# then for each one verify a matching diagnostic was emitted.
expect_lines=$(grep -n "EXPECT:" "$TEST_FILE" | cut -d: -f1 || true)
status=0
for ln in $expect_lines; do
    if ! echo "$output" | grep -E ":$ln:[0-9]+: warning:.*$EXPECTED" > /dev/null; then
        echo "FAIL: line $ln expected diagnostic containing '$EXPECTED'" >&2
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    exit 1
fi
echo "OK: all EXPECT lines matched"
