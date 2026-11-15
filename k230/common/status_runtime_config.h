/*
 * 交付版注释
 * 文件：k230/common/status_runtime_config.h
 * 职责：提供编译期状态 FIFO 开关。
 * 设计原因：可在保持视频链路不变的前提下隔离状态功能，便于板端分阶段排障。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

// The default production build enables the independent status FIFO.  A
// diagnostic build can turn it off without changing the video FIFO contract.
#ifndef FALLDOWN_RTSP_TCP_ENABLE_STATUS_FIFO
#define FALLDOWN_RTSP_TCP_ENABLE_STATUS_FIFO 1
#endif

inline constexpr bool kStatusReportingEnabled =
    FALLDOWN_RTSP_TCP_ENABLE_STATUS_FIFO != 0;
