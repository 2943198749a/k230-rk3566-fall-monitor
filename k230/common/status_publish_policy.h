/*
 * 交付版注释
 * 文件：k230/common/status_publish_policy.h
 * 职责：定义状态发布节流策略。
 * 设计原因：推理可以高频运行，但网络状态不必每帧发送；节流降低小核和网络压力。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <cstdint>

// The inference loop runs at roughly 30 FPS.  One status update per second
// keeps the small cross-core FIFO from filling before Linux attaches a reader.
constexpr std::uint32_t kStatusPublishFrameInterval = 30u;

inline bool should_publish_status(std::uint32_t sequence)
{
    return sequence != 0u &&
           sequence % kStatusPublishFrameInterval == 0u;
}
