#!/usr/bin/env bash
set -euo pipefail

echo "=== Tier 1 Static Lint & HAL Boundary Check ==="
FAIL=0

check_ban() {
    local pattern="$1"
    local dirs="$2"
    local reason="$3"
    if grep -rnE --include='*.c' --include='*.h' "$pattern" $dirs; then
        echo "[LINT FAIL] Forbidden pattern ($reason): $pattern"
        FAIL=1
    fi
}

# 1. Forbidden Orbis/POSIX traps across all src/
check_ban '\bgetcwd\s*\(' "src" "getcwd() calls missing syscall 326 -> SIGSYS on PS5 compat"
check_ban '\blstat\s*\(' "src" "lstat() fails on Orbis"
check_ban '\breadlink\s*\(' "src" "readlink() fails on Orbis"
check_ban '\baccess\s*\(' "src" "access() returns EPERM on FW 13.52"
check_ban '\bdup\s*\(' "src" "dup() fails with EPERM on retail hardware"
check_ban '\btimer_create\s*\(' "src" "timer_create(SIGEV_THREAD) hangs forever on Orbis"
check_ban '\b_exit\s*\(' "src" "_exit() raises SIGSYS on retail Orbis; use sceSystemServiceLoadExec"
check_ban '\bposix_spawn\s*\(' "src" "posix_spawn crashes on broken GOT on Orbis"
check_ban 'ORBIS_PAD_BUTTON_SHARE' "src" "DS4 Share is intercepted by Orbis OS"

# 2. Strict HAL boundary: zero libSce*, <orbis/*>, or <windows.h> in src/core/ or src/ui/
check_ban '(#include\s*<orbis/|#include\s*<windows\.h>|\bsce[A-Z])' "src/core src/ui" "Platform API leaked outside src/hal/"

if [ "$FAIL" -ne 0 ]; then
    echo "=== Tier 1 Lint FAILED ==="
    exit 1
fi
echo "=== Tier 1 Lint PASSED ==="
