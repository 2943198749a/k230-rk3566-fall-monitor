# 交付版注释
# 文件：rk3566/qt-viewer/CMakeLists.txt
# 职责：查找 Qt5/FFmpeg，构建播放器核心、GUI 和四组测试。
# 设计原因：核心库与界面目标分离，让控制逻辑可以独立测试。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

cmake_minimum_required(VERSION 3.16)

project(k230_qt_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets Network Test)

enable_testing()

add_executable(test_frame_rate_meter
    src/frame_rate_meter.cpp
    tests/test_frame_rate_meter.cpp
)
target_include_directories(test_frame_rate_meter PRIVATE src)
target_link_libraries(test_frame_rate_meter PRIVATE Qt5::Core Qt5::Test)
add_test(NAME test_frame_rate_meter COMMAND test_frame_rate_meter)

add_executable(test_retry_policy
    src/retry_policy.cpp
    tests/test_retry_policy.cpp
)
target_include_directories(test_retry_policy PRIVATE src)
target_link_libraries(test_retry_policy PRIVATE Qt5::Core Qt5::Test)
add_test(NAME test_retry_policy COMMAND test_retry_policy)

add_executable(test_player_controller
    src/player_controller.cpp
    src/rtsp_decoder.cpp
    src/frame_rate_meter.cpp
    tests/test_player_controller.cpp
)
target_include_directories(test_player_controller PRIVATE src)
target_link_libraries(test_player_controller PRIVATE Qt5::Core Qt5::Gui Qt5::Test PkgConfig::FFMPEG)
add_test(NAME test_player_controller COMMAND test_player_controller)

add_executable(test_detection_count_receiver
    src/detection_count_receiver.cpp
    tests/test_detection_count_receiver.cpp
)
target_include_directories(test_detection_count_receiver PRIVATE src)
target_link_libraries(test_detection_count_receiver PRIVATE Qt5::Core Qt5::Network Qt5::Test)
add_test(NAME test_detection_count_receiver COMMAND test_detection_count_receiver)

add_library(k230_player_core
    src/frame_rate_meter.cpp
    src/retry_policy.cpp
    src/player_controller.cpp
    src/rtsp_decoder.cpp
)
target_include_directories(k230_player_core PUBLIC src)
target_link_libraries(k230_player_core PUBLIC Qt5::Core Qt5::Gui PkgConfig::FFMPEG)

add_executable(k230-qt-viewer
    src/main.cpp
    src/main_window.h
    src/main_window.cpp
    src/video_widget.h
    src/video_widget.cpp
    src/detection_count_receiver.cpp
)
target_include_directories(k230-qt-viewer PRIVATE src)
target_link_libraries(k230-qt-viewer PRIVATE k230_player_core Qt5::Widgets Qt5::Network)
