#!/bin/bash
# 交付版注释
# 文件：k230/little-core/rtsp-server/build.sh
# 职责：创建独立构建目录并配置、编译、安装 rtspServer。
# 设计原因：out-of-source 构建不污染源码；脚本固定常用命令，减少手工参数错误。
# 修改原则：仅补充说明；运行行为与来源文件保持一致。

set -eu

# K230_SDK_ROOT 必须指向与板端固件同一套 SDK，避免头文件/静态库 ABI
# 不一致。TOOLCHAIN_BIN 可覆盖，默认使用官方 Xuantie 工具链目录。
: "${K230_SDK_ROOT:?请先 export K230_SDK_ROOT=/absolute/path/to/k230_sdk}"
TOOLCHAIN_BIN="${TOOLCHAIN_BIN:-/opt/toolchain/Xuantie-900-gcc-linux-5.10.4-glibc-x86_64-V2.6.0/bin}"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/out"
export PATH="${PATH}:${TOOLCHAIN_BIN}"

# 只清理脚本目录内的固定 out 目录，不使用来自外部输入的删除路径。
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install" \
      -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/Riscv64.cmake" \
      -DK230_SDK_ROOT="${K230_SDK_ROOT}"

cmake --build "${BUILD_DIR}" --parallel
cmake --install "${BUILD_DIR}"
