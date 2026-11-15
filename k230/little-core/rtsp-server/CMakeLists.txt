# 交付版注释
# 文件：k230/little-core/rtsp-server/CMakeLists.txt
# 职责：构建小核 rtspServer 并链接 DataFIFO、MAPI、RTSP 与 Live555 静态库。
# 设计原因：显式列出板端 SDK 依赖，使交叉编译失败时可以定位缺少的组件。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

cmake_minimum_required(VERSION 3.16)

project(rtspServer C CXX)

set(K230_SDK_ROOT "" CACHE PATH "与目标板固件匹配的完整 K230 SDK 根目录")
if(NOT K230_SDK_ROOT)
    message(FATAL_ERROR
        "请设置 -DK230_SDK_ROOT=/absolute/path/to/k230_sdk；"
        "RTSP 服务必须链接与板端媒体运行时匹配的库。")
endif()
get_filename_component(k230_sdk "${K230_SDK_ROOT}" ABSOLUTE)
set(CDK_SRC_DIR ${k230_sdk}/src/common/cdk)
set(RTSMART_SRC_DIR ${k230_sdk}/src/big/rt-smart)
set(IPCMSG_ROOT ${CDK_SRC_DIR}/user/component/ipcmsg)
set(MAPI_ROOT ${CDK_SRC_DIR}/user/mapi)
set(MPI_ROOT ${RTSMART_SRC_DIR}/userapps/mpp)
set(LIVE_ROOT ${CDK_SRC_DIR}/user/thirdparty/live)
set(MW_ROOT ${CDK_SRC_DIR}/user/middleware)

set(rtsp_server_root "${PROJECT_SOURCE_DIR}/src")

file(GLOB APP_SOURCES ${rtsp_server_root}/*.cpp)

set(BIN rtspServer)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${k230_sdk}/src/big/mpp/userapps/api/)
include_directories(${k230_sdk}/src/big/mpp/include)
include_directories(${k230_sdk}/src/big/mpp/include/comm)
include_directories(${k230_sdk}/src/common/cdk/user/component/datafifo/include)

include_directories(${IPCMSG_ROOT}/include)
include_directories(${MAPI_ROOT}/include)
include_directories(${MAPI_ROOT}/include/api)
include_directories(${MAPI_ROOT}/include/comm)
include_directories(/usr/local/include)
include_directories(${MW_ROOT}/rtsp_server/include)

link_directories(${k230_sdk}/src/common/cdk/user/component/datafifo/slave/lib)
link_directories(${k230_sdk}/src/big/mpp/userapps/lib)

link_directories(${IPCMSG_ROOT}/host/lib)
link_directories(${MAPI_ROOT}/mediaclient/lib)
link_directories(${MW_ROOT}/rtsp_server/lib)
link_directories(${LIVE_ROOT}/BasicUsageEnvironment)
link_directories(${LIVE_ROOT}/groupsock)
link_directories(${LIVE_ROOT}/liveMedia)
link_directories(${LIVE_ROOT}/UsageEnvironment)
link_directories(/usr/local/lib)


add_executable(${BIN} ${APP_SOURCES})
target_link_libraries(${BIN} -Wl,--start-group datafifo pthread sys ipcmsg mapi rtsp_server liveMedia UsageEnvironment BasicUsageEnvironment groupsock -Wl,--end-group)

install(TARGETS ${BIN} DESTINATION out)
