#!/usr/bin/env bash
# Auto-build cho Trạm Điều Khiển Mặt Đất (C++/Qt6).
#
# Chạy được trên Linux / Raspberry Pi VÀ trên Windows qua Git Bash / MSYS2.
# Tự dò Qt, trình sinh Ninja và toolchain MinGW (trên Windows). Ghi đè bằng biến
# môi trường QT_PREFIX / MINGW_DIR nếu cần.
#
#   ./build.sh              cấu hình (nếu cần) + build Release
#   ./build.sh --clean      xoá thư mục build rồi build lại từ đầu
#   ./build.sh --debug      build kiểu Debug
#   ./build.sh --run        build xong chạy luôn ứng dụng
#   ./build.sh -j 4         giới hạn số luồng biên dịch
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="build"
BUILD_TYPE="Release"
DO_CLEAN=0
DO_RUN=0
JOBS=""

while [ $# -gt 0 ]; do
    case "$1" in
        --clean)  DO_CLEAN=1 ;;
        --run)    DO_RUN=1 ;;
        --debug)  BUILD_TYPE="Debug" ;;
        --release) BUILD_TYPE="Release" ;;
        -j)       shift; JOBS="$1" ;;
        -j*)      JOBS="${1#-j}" ;;
        -h|--help)
            grep '^#' "$0" | sed 's/^# \?//'; exit 0 ;;
        *) echo "Tham số lạ: $1" >&2; exit 2 ;;
    esac
    shift
done

# ── phát hiện hệ điều hành ────────────────────────────────────────────────────
IS_WINDOWS=0
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=1 ;;
esac

# ── số luồng ──────────────────────────────────────────────────────────────────
if [ -z "$JOBS" ]; then
    if command -v nproc >/dev/null 2>&1; then JOBS="$(nproc)";
    elif [ -n "${NUMBER_OF_PROCESSORS:-}" ]; then JOBS="$NUMBER_OF_PROCESSORS";
    else JOBS=4; fi
fi

CMAKE_ARGS=(-S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE")

# ── trình sinh: ưu tiên Ninja ─────────────────────────────────────────────────
if command -v ninja >/dev/null 2>&1; then
    CMAKE_ARGS+=(-G Ninja)
fi

# ── cấu hình riêng Windows (Qt MinGW + toolchain) ─────────────────────────────
if [ "$IS_WINDOWS" -eq 1 ]; then
    # Qt prefix: biến môi trường QT_PREFIX, hoặc bản mingw_64 mới nhất trong C:/Qt.
    if [ -z "${QT_PREFIX:-}" ]; then
        for base in /c/Qt /c/Qt6 "${QTDIR:-}"; do
            [ -d "$base" ] || continue
            cand="$(ls -d "$base"/*/mingw_64 2>/dev/null | sort -V | tail -1 || true)"
            [ -n "$cand" ] && { QT_PREFIX="$cand"; break; }
        done
    fi
    # Toolchain MinGW: biến MINGW_DIR, hoặc bản mới nhất trong C:/Qt/Tools.
    if [ -z "${MINGW_DIR:-}" ]; then
        MINGW_DIR="$(ls -d /c/Qt/Tools/mingw*_64 2>/dev/null | sort -V | tail -1 || true)"
    fi

    if [ -n "${QT_PREFIX:-}" ]; then
        echo "Qt:      $QT_PREFIX"
        CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$QT_PREFIX")
        export PATH="$QT_PREFIX/bin:$PATH"   # để chạy exe tìm được DLL Qt
    else
        echo "⚠ Không tìm thấy Qt trong C:/Qt — đặt QT_PREFIX thủ công nếu cấu hình lỗi." >&2
    fi
    if [ -n "${MINGW_DIR:-}" ]; then
        echo "MinGW:   $MINGW_DIR"
        CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER="$MINGW_DIR/bin/g++.exe"
                     -DCMAKE_C_COMPILER="$MINGW_DIR/bin/gcc.exe")
        export PATH="$MINGW_DIR/bin:$PATH"   # để Ninja gọi được g++
    fi
fi

# ── build ─────────────────────────────────────────────────────────────────────
if [ "$DO_CLEAN" -eq 1 ]; then
    echo "Xoá $BUILD_DIR/ …"
    rm -rf "$BUILD_DIR"
fi

echo "Cấu hình ($BUILD_TYPE) …"
cmake "${CMAKE_ARGS[@]}"

echo "Biên dịch (-j$JOBS) …"
cmake --build "$BUILD_DIR" -j"$JOBS"

# ── chạy thử ──────────────────────────────────────────────────────────────────
BIN="$BUILD_DIR/LiteGCS"
[ "$IS_WINDOWS" -eq 1 ] && BIN="$BUILD_DIR/LiteGCS.exe"
echo "✔ Xong: $BIN"

if [ "$DO_RUN" -eq 1 ]; then
    echo "Chạy $BIN …"
    "./$BIN"
fi
