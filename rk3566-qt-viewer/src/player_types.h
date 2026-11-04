/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/player_types.h
 * 职责：定义播放器状态与统计值对象。
 * 设计原因：跨线程信号使用小型值类型，减少共享可变状态。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <QMetaType>
#include <QSize>

enum class PlayerState {
    Idle,
    Connecting,
    Playing,
    Stopping,
    Reconnecting,
    Error,
};

struct PlayerStats {
    double fps = 0.0;
    QSize frameSize;
    quint64 decodedFrames = 0;
};

Q_DECLARE_METATYPE(PlayerState)
Q_DECLARE_METATYPE(PlayerStats)
