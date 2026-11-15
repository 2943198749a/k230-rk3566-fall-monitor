# 交付版注释
# 文件：k230/CMakeLists.txt
# 职责：K230 大核应用的顶层构建入口。
# 设计原因：集中声明 SDK/nncase 路径并进入 big-core，避免每个源文件重复维护依赖。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

cmake_minimum_required(VERSION 3.16)
project(falldown_rtsp C CXX)

# This application is built directly rather than through ai_poc's umbrella
# CMake project, so make the SDK and Nncase locations explicit.
if (DEFINED K230_SDK_ROOT AND NOT K230_SDK_ROOT STREQUAL "")
    set(k230_sdk "${K230_SDK_ROOT}")
else()
    message(FATAL_ERROR
        "请通过 -DK230_SDK_ROOT=/absolute/path/to/k230_sdk 指定完整 K230 SDK。"
        "交付包不捆绑官方 SDK，不能再依赖原工作区的相对目录。")
endif()
set(nncase_sdk_root "${k230_sdk}/src/big/nncase")
set(CMAKE_EXE_LINKER_FLAGS "-T ${k230_sdk}/src/reference/ai_poc/cmake/link.lds --static")

# These are inherited by normal ai_poc builds. This standalone package needs
# them explicitly for the fall model runtime and its OpenCV helpers.
include_directories(${nncase_sdk_root}/riscv64/nncase/include)
include_directories(${nncase_sdk_root}/riscv64/nncase/include/nncase/runtime)
include_directories(${nncase_sdk_root}/riscv64)
include_directories(${k230_sdk}/src/big/utils/lib/opencv/include/opencv4)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
link_directories(${nncase_sdk_root}/riscv64/nncase/lib)
link_directories(${k230_sdk}/src/big/utils/lib/opencv/lib ${k230_sdk}/src/big/utils/lib/opencv/lib/opencv4/3rdparty)
link_directories(${k230_sdk}/src/big/mpp/userapps/lib)

# 交付包把原目录 big 重命名为更直观的 big-core，因此这里只改变目录名，
# 目标名、源码和链接关系仍与已验证版本一致。
add_subdirectory(big-core)
