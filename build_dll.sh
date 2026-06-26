#!/usr/bin/env bash
# build_dll.sh — Build RakNetDLL for Linux x64, Windows x64, x86, and arm64
#
# Compiles each .cpp individually (parallel jobs) then links — avoids
# header-guard contamination from passing all files to one compiler call.
#
# Requires LLVM-MinGW on PATH for Windows targets:
#   https://github.com/mstorsjo/llvm-mingw/releases
#
# Usage:
#   ./build_dll.sh              # all four targets
#   ./build_dll.sh linux64
#   ./build_dll.sh win64
#   ./build_dll.sh win32
#   ./build_dll.sh win-arm64

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/Source"
DIST="$SCRIPT_DIR/../dist"

# Gather .cpp sources — skip platform stubs irrelevant to our targets
mapfile -t SOURCES < <(find "$SRC" -maxdepth 1 -name "*.cpp" \
    ! -name "*_PS3*"         \
    ! -name "*_PS4*"         \
    ! -name "*_Vita*"        \
    ! -name "*_360*"         \
    ! -name "*_720*"         \
    ! -name "*WindowsStore*" \
    ! -name "*NativeClient*" \
    | sort)

echo "[build_dll] ${#SOURCES[@]} source files"

COMMON_DEFS="-D_RAKNET_DLL -DRAKNET_C_EXPORTS"
COMMON_OPTS="-std=c++14 -O2 -w"

# Case-bridge: mixed-case symlinks → MinGW Windows SDK headers
# (RakNet uses WinSock2.h etc.; Linux FS is case-sensitive)
CASE_FIX="$SCRIPT_DIR/mingw-case-fix"

# ── Parallel compile helper ────────────────────────────────────────────────
# compile_all <objdir> <CXX> <extra_flags...>
compile_all() {
    local OBJDIR="$1"; shift
    local CXX="$1";    shift
    local EXTRA="$*"
    mkdir -p "$OBJDIR"

    local PIDS=()
    local OBJS=()
    local FAILED=0

    for SRC_FILE in "${SOURCES[@]}"; do
        local BASE
        BASE=$(basename "$SRC_FILE" .cpp)
        local OBJ="$OBJDIR/${BASE}.o"
        OBJS+=("$OBJ")

        # Compile in background
        $CXX -c $COMMON_OPTS $COMMON_DEFS $EXTRA \
            -I"$SRC" \
            "$SRC_FILE" -o "$OBJ" &
        PIDS+=($!)
    done

    # Wait for all and collect failures
    for i in "${!PIDS[@]}"; do
        if ! wait "${PIDS[$i]}"; then
            echo "  [FAIL] $(basename "${SOURCES[$i]}")" >&2
            FAILED=$((FAILED+1))
        fi
    done

    if [[ $FAILED -gt 0 ]]; then
        echo "[build_dll] $FAILED compile error(s) — aborting" >&2
        return 1
    fi

    # Echo the object list for the caller to use
    echo "${OBJS[@]}"
}

# ── Linux x64 ─────────────────────────────────────────────────────────────
build_linux64() {
    local OUT="$DIST/linux-x64"
    local OBJDIR="$OUT/obj"
    mkdir -p "$OUT"
    echo "[build_dll] Compiling Linux x64..."

    mapfile -t OBJS < <(compile_all "$OBJDIR" "g++" "-fPIC -fvisibility=default" | tr ' ' '\n')

    echo "[build_dll] Linking Linux x64..."
    g++ -shared -o "$OUT/libRakNetDLL.so" "${OBJS[@]}" -lpthread
    rm -rf "$OBJDIR"
    echo "[build_dll] ✓ Linux x64: $(du -sh "$OUT/libRakNetDLL.so" | cut -f1)"
}

# ── Windows DLL (shared compile+link logic) ───────────────────────────────
build_windows() {
    local OUT="$1"
    local CXX="$2"
    local IMPLIB="$OUT/RakNetDLL.dll.a"
    local OBJDIR="$OUT/obj"
    mkdir -p "$OUT"

    echo "[build_dll] Compiling $(basename "$OUT")..."

    mapfile -t OBJS < <(compile_all "$OBJDIR" "$CXX" \
        "-DWIN32 -D_WIN32 -idirafter $CASE_FIX" | tr ' ' '\n')

    echo "[build_dll] Linking $(basename "$OUT")..."
    $CXX -shared -fvisibility=default \
        "${OBJS[@]}" \
        -lws2_32 \
        -Wl,--out-implib,"$IMPLIB" \
        -o "$OUT/RakNetDLL.dll"

    rm -rf "$OBJDIR"
    echo "[build_dll] ✓ $(basename "$OUT"): $(du -sh "$OUT/RakNetDLL.dll" | cut -f1)"
}

build_win64()     { build_windows "$DIST/win-x64"   "x86_64-w64-mingw32-clang++"; }
build_win32()     { build_windows "$DIST/win-x32"   "i686-w64-mingw32-clang++"; }
build_win_arm64() { build_windows "$DIST/win-arm64" "aarch64-w64-mingw32-clang++"; }

# ── Entry point ───────────────────────────────────────────────────────────
TARGET="${1:-all}"
case "$TARGET" in
    linux64)   build_linux64    ;;
    win64)     build_win64      ;;
    win32)     build_win32      ;;
    win-arm64) build_win_arm64  ;;
    all)
        build_linux64
        build_win64
        build_win32
        build_win_arm64
        echo ""
        echo "══════════════════════════════════════════"
        echo " All builds complete"
        echo "──────────────────────────────────────────"
        ls -lh "$DIST"/linux-x64/libRakNetDLL.so \
                "$DIST"/win-x64/RakNetDLL.dll \
                "$DIST"/win-x32/RakNetDLL.dll \
                "$DIST"/win-arm64/RakNetDLL.dll 2>/dev/null || true
        echo ""
        echo "  C header: $SRC/raknet_c.h"
        echo "══════════════════════════════════════════"
        ;;
    *)
        echo "Usage: $0 [linux64|win64|win32|win-arm64|all]"
        exit 1
        ;;
esac
