/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/frame_rate_meter.cpp
 * 职责：按时间窗口计算实时帧率。
 * 设计原因：帧率统计独立后可做确定性单元测试，也不会污染解码逻辑。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "frame_rate_meter.h"

void FrameRateMeter::reset()
{
    recentFramesMs_.clear();
    frameCount_ = 0;
}

void FrameRateMeter::recordFrame(qint64 nowMs)
{
    recentFramesMs_.append(nowMs);
    ++frameCount_;

    while (!recentFramesMs_.isEmpty() && recentFramesMs_.front() < nowMs - 1000) {
        recentFramesMs_.removeFirst();
    }
}

double FrameRateMeter::framesPerSecond() const
{
    if (recentFramesMs_.size() < 2) {
        return 0.0;
    }

    const qint64 elapsedMs = recentFramesMs_.back() - recentFramesMs_.front();
    if (elapsedMs <= 0) {
        return 0.0;
    }

    return (static_cast<double>(recentFramesMs_.size()) * 1000.0) /
        static_cast<double>(elapsedMs);
}

quint64 FrameRateMeter::frameCount() const
{
    return frameCount_;
}
