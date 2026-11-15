# Qt Core/GUI/Widgets 负责界面，Network 负责 K230 TCP STATUS 通讯。
QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    widget.cpp \
    statusprotocol.cpp \
    endpointconfig.cpp \
    rtspdecoder.cpp

HEADERS += \
    widget.h \
    statusprotocol.h \
    endpointconfig.h \
    rtspdecoder.h

# FFmpeg 是可选的视频后端。未安装开发包的桌面 Qt Kit 仍可构建并运行
# 全部监控/TCP 界面，界面会明确提示视频功能不可用。
packagesExist(libavformat libavcodec libavutil libswscale) {
    CONFIG += link_pkgconfig
    PKGCONFIG += libavformat libavcodec libavutil libswscale
    DEFINES += HAVE_FFMPEG
} else {
    message("FFmpeg development packages not found: building without RTSP decoding")
}

FORMS += \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
