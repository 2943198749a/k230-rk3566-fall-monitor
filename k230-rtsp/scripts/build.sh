#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${SDK_ROOT:?请设置 SDK_ROOT 指向已准备好的 k230_linux_sdk 目录}"
: "${TOOLCHAIN_ROOT:=/opt/toolchain/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2}"
SYSROOT="$SDK_ROOT/output/k230_canmv_v3_defconfig/staging"
CC="$TOOLCHAIN_ROOT/bin/riscv64-unknown-linux-gnu-gcc"
CXX="$TOOLCHAIN_ROOT/bin/riscv64-unknown-linux-gnu-g++"
[ -x "$CC" ] && [ -x "$CXX" ] || { echo '交叉编译器不存在' >&2; exit 1; }
[ -f "$SYSROOT/usr/include/features.h" ] || { echo 'SDK staging 未准备好' >&2; exit 1; }
[ -f "$SYSROOT/usr/lib/librtsp_server.so" ] || { echo 'SDK staging 缺少 librtsp_server.so' >&2; exit 1; }
export GCC_PATH="$TOOLCHAIN_ROOT/bin" sysroot="$SYSROOT"
mkdir -p "$BASE/build/rtsp-src" "$BASE/build/bin"
cmake -S "$BASE/source/ai_demo" -B "$BASE/build/ai" -Dfalldown_publish=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$BASE/build/ai" -j2
cp "$BASE/build/ai/falldown_publish/falldown_publish.elf" "$BASE/build/bin/"
cp "$BASE/source/rtsp/"*.cpp "$BASE/source/rtsp/"*.h "$BASE/source/rtsp/"*.c "$BASE/source/rtsp/Makefile" "$BASE/build/rtsp-src/"
make -C "$BASE/build/rtsp-src" -j2 CPP="$CXX" STAGING_DIR="$SYSROOT" CFLAGS="--sysroot=$SYSROOT -mcpu=c908v -O2 -std=c++17" LDFLAGS="--sysroot=$SYSROOT -mcpu=c908v -Wl,-rpath-link,$SYSROOT/usr/lib"
cp "$BASE/build/rtsp-src/camera_rtsp_yolo" "$BASE/build/bin/camera_rtsp_yolo_1080"
echo "完成：$BASE/build/bin"
