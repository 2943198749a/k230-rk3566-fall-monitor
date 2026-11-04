/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/app_config.h
 * 职责：保存 Qt 播放器的默认 RTSP 地址和重试配置。
 * 设计原因：把部署环境变量与界面/解码代码分离，换网络地址时只改一处。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <QString>

inline const QString kDefaultRtspUrl =
    QStringLiteral("rtsp://192.168.1.100:8554/BackChannelTest");
