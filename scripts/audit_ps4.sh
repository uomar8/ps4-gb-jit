#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?Usage: audit_ps4.sh <eboot.elf> <pkg_path> <build_dir>}"
PKG_PATH="${2:?Usage: audit_ps4.sh <eboot.elf> <pkg_path> <build_dir>}"
BUILD_DIR="${3:?Usage: audit_ps4.sh <eboot.elf> <pkg_path> <build_dir>}"

echo "=== Tier 3 Automated PS4 Binary & PKG Audit ==="
FAIL=0

# 1. Check for forbidden AVX2 / FMA3 instructions on AMD Jaguar (btver2)
echo "[1/5] Checking ISA (zero AVX2/FMA3)..."
if llvm-objdump -d "$ELF_PATH" | grep -iE '\b(vfmadd|vfmsub|vfnmadd|vfnmsub|vpermd|vpermq|vperm2i128|vpblendd|vpbroadcast|vpgather|vpmaskmov|vpsllv|vpsrav|vpsrlv|ymm[0-9]|zmm[0-9])'; then
    echo "[AUDIT FAIL] Found AVX2/FMA3/256-bit YMM instruction in $ELF_PATH!"
    FAIL=1
else
    echo "  OK: zero AVX2/FMA3/YMM instructions."
fi

# 2. Check for forbidden undefined symbols
echo "[2/5] Checking undefined symbols..."
UNDEF=$(llvm-nm -u "$ELF_PATH" || true)
for sym in _exit getcwd posix_spawn timer_create lstat readlink; do
    if echo "$UNDEF" | grep -w "$sym"; then
        echo "[AUDIT FAIL] Forbidden undefined symbol: $sym"
        FAIL=1
    fi
done
echo "  OK: no forbidden undefined symbols."

# 3. Check .su stack frame usage <= 32768 bytes
echo "[3/5] Checking .su stack frame sizes (<= 32768 B)..."
SU_FILES=$(find "$BUILD_DIR" -name '*.su' 2>/dev/null || true)
if [ -n "$SU_FILES" ]; then
    while IFS= read -r su; do
        while IFS=$'\t' read -r func bytes kind; do
            if [ -n "${bytes:-}" ] && [ "$bytes" -gt 32768 ] 2>/dev/null; then
                echo "[AUDIT FAIL] Stack frame > 32 KiB: $func ($bytes B, $kind)"
                FAIL=1
            fi
        done < "$su"
    done <<< "$SU_FILES"
    echo "  OK: all stack frames <= 32 KiB."
else
    echo "  WARN: no .su files found in $BUILD_DIR."
fi

# 4. Verify SC entries in PKG and required retail PRXs in PFS payload
echo "[4/5] Checking PKG SC entries and PFS payload (eboot.bin, libc.prx, libSceFios2.prx)..."
PKGTOOL="${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core"
if [ ! -x "$PKGTOOL" ]; then
    PKGTOOL_CMD=(dotnet --roll-forward Major "${OO_PS4_TOOLCHAIN}/bin/windows/PkgTool.Core.dll")
else
    PKGTOOL_CMD=("$PKGTOOL")
fi

ENTRIES=$("${PKGTOOL_CMD[@]}" pkg_listentries "$PKG_PATH")
echo "$ENTRIES"
for sc_entry in PARAM_SFO ICON0_PNG PIC0_PNG; do
    if ! echo "$ENTRIES" | grep -q "$sc_entry"; then
        echo "[AUDIT FAIL] Missing SC entry $sc_entry in $PKG_PATH"
        FAIL=1
    fi
done

PKG_STAGE="build/pkg_stage"
for pfs_file in eboot.bin sce_module/libc.prx sce_module/libSceFios2.prx; do
    if [ ! -s "$PKG_STAGE/$pfs_file" ]; then
        echo "[AUDIT FAIL] Missing or empty PFS payload file: $PKG_STAGE/$pfs_file"
        FAIL=1
    fi
    if ! grep -q "targ_path=\"$pfs_file\"" "$PKG_STAGE/pkg.gp4"; then
        echo "[AUDIT FAIL] $pfs_file not declared in $PKG_STAGE/pkg.gp4"
        FAIL=1
    fi
done
echo "  OK: SC entries and PFS retail PRXs verified."

# 5. Validate PKG integrity
echo "[5/5] Running PkgTool.Core pkg_validate..."
if ! "${PKGTOOL_CMD[@]}" pkg_validate "$PKG_PATH"; then
    echo "[AUDIT FAIL] pkg_validate failed on $PKG_PATH"
    FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
    echo "=== Tier 3 PS4 Audit FAILED ==="
    exit 1
fi
echo "=== Tier 3 PS4 Audit PASSED ==="
