/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/tests/test_frame_rate_meter.cpp
 * 职责：验证帧率计时窗口和复位行为。
 * 设计原因：用可控时间输入避免依赖真实摄像头帧率。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <QtTest>

#include "frame_rate_meter.h"

class FrameRateMeterTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsZeroUntilFramesArrive();
    void calculatesFramesInsideOneSecondWindow();
};

void FrameRateMeterTest::reportsZeroUntilFramesArrive()
{
    FrameRateMeter meter;

    QCOMPARE(meter.framesPerSecond(), 0.0);
    QCOMPARE(meter.frameCount(), quint64{0});
}

void FrameRateMeterTest::calculatesFramesInsideOneSecondWindow()
{
    FrameRateMeter meter;

    meter.recordFrame(1000);
    meter.recordFrame(1100);
    meter.recordFrame(1200);

    QCOMPARE(meter.framesPerSecond(), 15.0);
    QCOMPARE(meter.frameCount(), quint64{3});
}

QTEST_MAIN(FrameRateMeterTest)
#include "test_frame_rate_meter.moc"
