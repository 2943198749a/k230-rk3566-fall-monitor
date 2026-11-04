# 交付版注释
# 文件：k230/little-core/rtsp-server/cmake/Riscv64.cmake
# 职责：声明 K230 Linux 小核的 RISC-V 交叉编译器与发布参数。
# 设计原因：工具链文件让 CMake 在配置阶段就选择目标编译器，避免误用主机编译器。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

include(CMakeForceCompiler)

# target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# cross compiler
set(CMAKE_C_COMPILER riscv64-unknown-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-linux-gnu-g++)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-O0 -s")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O0 -s")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv64imafdcv -mabi=lp64d -mcmodel=medany")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv64imafdcv -mabi=lp64d -mcmodel=medany")
