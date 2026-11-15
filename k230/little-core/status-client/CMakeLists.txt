# 交付版注释
# 文件：k230/little-core/status-client/CMakeLists.txt
# 职责：构建小核状态 TCP 客户端并链接 DataFIFO reader 库。
# 设计原因：通过 K230_SDK_ASSET_ROOT 显式选择与板端固件匹配的 SDK。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

cmake_minimum_required(VERSION 3.16)
project(k230_status_tcp_client LANGUAGES CXX)

set(K230_SDK_ASSET_ROOT "" CACHE PATH "K230 SDK containing DataFIFO headers and slave library")
if(NOT K230_SDK_ASSET_ROOT)
    message(FATAL_ERROR "Set K230_SDK_ASSET_ROOT to a complete K230 SDK path")
endif()

add_executable(k230_status_tcp_client status_tcp_client.cpp)
target_include_directories(k230_status_tcp_client PRIVATE
    ../../common
    ${K230_SDK_ASSET_ROOT}/src/common/cdk/user/component/datafifo/include
    ${K230_SDK_ASSET_ROOT}/src/big/mpp/include
    ${K230_SDK_ASSET_ROOT}/src/big/mpp/include/comm)
target_link_directories(k230_status_tcp_client PRIVATE
    ${K230_SDK_ASSET_ROOT}/src/common/cdk/user/component/datafifo/slave/lib)
target_link_libraries(k230_status_tcp_client PRIVATE datafifo pthread)
install(TARGETS k230_status_tcp_client DESTINATION bin)
