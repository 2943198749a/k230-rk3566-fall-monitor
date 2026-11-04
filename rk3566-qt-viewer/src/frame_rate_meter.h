/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/frame_rate_meter.h
 * 职责：按时间窗口计算实时帧率。
 * 设计原因：帧率统计独立后可做确定性单元测试，也不会污染解码逻辑。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <QVector>
#include <QtGlobal>

class FrameRateMeter {
public:
    void reset();
    void recordFrame(qint64 nowMs);
    double framesPerSecond() const;
    quint64 frameCount() const;

private:
    QVector<qint64> recentFramesMs_;
    quint64 frameCount_ = 0;
};
