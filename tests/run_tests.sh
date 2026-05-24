#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Chkoupi-lang test runner
#
#  Usage:
#    ./tests/run_tests.sh                        # auto-detect binary
#    ./tests/run_tests.sh path/to/chkoupi        # explicit binary path
#    ./tests/run_tests.sh path/to/chkoupi.exe    # Windows (Git Bash / MSYS2)
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find chkoupi binary
if [[ $# -ge 1 ]]; then
    CHKOUPI="$1"
elif [[ -f "$SCRIPT_DIR/../build/Release/chkoupi.exe" ]]; then
    CHKOUPI="$SCRIPT_DIR/../build/Release/chkoupi.exe"
elif [[ -f "$SCRIPT_DIR/../build/chkoupi" ]]; then
    CHKOUPI="$SCRIPT_DIR/../build/chkoupi"
elif [[ -f "$SCRIPT_DIR/../build/Debug/chkoupi.exe" ]]; then
    CHKOUPI="$SCRIPT_DIR/../build/Debug/chkoupi.exe"
else
    echo "ERROR: Cannot find chkoupi binary. Build first or pass path as argument."
    exit 1
fi

echo "Using: $CHKOUPI"
echo "========================================"

PASS=0
FAIL=0
ERRORS=""

# ── Pass tests (expected stdout) ─────────────────────────────────────────────
for expected in "$SCRIPT_DIR"/*.expected; do
    test_name="$(basename "${expected%.expected}")"
    dz_file="$SCRIPT_DIR/${test_name}.dz"

    if [[ ! -f "$dz_file" ]]; then
        continue
    fi

    # Run the compiler and capture output
    actual=$("$CHKOUPI" "$dz_file" 2>/dev/null) || {
        echo "FAIL  $test_name  (compiler crashed)"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n  $test_name: compiler exited with error"
        continue
    }

    expected_content=$(cat "$expected")

    # Normalize line endings
    actual=$(echo "$actual" | tr -d '\r')
    expected_content=$(echo "$expected_content" | tr -d '\r')

    if [[ "$actual" == "$expected_content" ]]; then
        echo "PASS  $test_name"
        PASS=$((PASS + 1))
    else
        echo "FAIL  $test_name"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n  $test_name:"
        ERRORS="${ERRORS}\n    expected: $(echo "$expected_content" | head -3)"
        ERRORS="${ERRORS}\n    got:      $(echo "$actual" | head -3)"
    fi
done

# ── Error tests (expected compiler error) ────────────────────────────────────
for expected_err in "$SCRIPT_DIR"/*.expected_err; do
    test_name="$(basename "${expected_err%.expected_err}")"
    dz_file="$SCRIPT_DIR/${test_name}.dz"

    if [[ ! -f "$dz_file" ]]; then
        continue
    fi

    # Run the compiler — it SHOULD fail
    stderr_output=$("$CHKOUPI" "$dz_file" 2>&1 >/dev/null) && {
        echo "FAIL  $test_name  (should have failed but succeeded)"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n  $test_name: expected error but compiler succeeded"
        continue
    }

    expected_msg=$(cat "$expected_err" | tr -d '\r')
    stderr_output=$(echo "$stderr_output" | tr -d '\r')

    if echo "$stderr_output" | grep -qF "$expected_msg"; then
        echo "PASS  $test_name"
        PASS=$((PASS + 1))
    else
        echo "FAIL  $test_name  (wrong error message)"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n  $test_name:"
        ERRORS="${ERRORS}\n    expected error containing: $expected_msg"
        ERRORS="${ERRORS}\n    got: $stderr_output"
    fi
done

# ── Summary ──────────────────────────────────────────────────────────────────
echo "========================================"
TOTAL=$((PASS + FAIL))
echo "Results: $PASS/$TOTAL passed"

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo "Failures:"
    echo -e "$ERRORS"
    exit 1
else
    echo "All tests passed!"
    exit 0
fi
