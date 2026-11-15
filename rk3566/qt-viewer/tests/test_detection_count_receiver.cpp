/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/tests/test_detection_count_receiver.cpp
 * 职责：验证 UDP JSON 计数的合法与非法输入。
 * 设计原因：保证旧状态通道收到坏包时不会污染界面状态。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <QtTest>

#include "detection_count_receiver.h"

class DetectionCountReceiverTest final : public QObject {
    Q_OBJECT
private slots:
    void replacesCountsInsteadOfAccumulating();
    void rejectsNegativeOrIncompleteJson();
    void marksCountsStaleAfterTwoSeconds();
};

void DetectionCountReceiverTest::replacesCountsInsteadOfAccumulating()
{
    DetectionCountReceiver receiver;
    QSignalSpy spy(&receiver, &DetectionCountReceiver::countsChanged);
    receiver.processPayloadForTest(R"({"fall":1,"no_fall":2})", 1000);
    receiver.processPayloadForTest(R"({"fall":0,"no_fall":1})", 1100);
    QCOMPARE(spy.count(), 2);
    const DetectionCounts last = qvariant_cast<DetectionCounts>(spy.takeLast().at(0));
    QCOMPARE(last.fall, 0);
    QCOMPARE(last.noFall, 1);
    QVERIFY(last.fresh);
}

void DetectionCountReceiverTest::rejectsNegativeOrIncompleteJson()
{
    DetectionCountReceiver receiver;
    QSignalSpy spy(&receiver, &DetectionCountReceiver::countsChanged);
    receiver.processPayloadForTest(R"({"fall":-1,"no_fall":2})", 1000);
    receiver.processPayloadForTest(R"({"fall":1})", 1100);
    QCOMPARE(spy.count(), 0);
}

void DetectionCountReceiverTest::marksCountsStaleAfterTwoSeconds()
{
    DetectionCountReceiver receiver;
    QSignalSpy spy(&receiver, &DetectionCountReceiver::countsChanged);
    receiver.processPayloadForTest(R"({"fall":1,"no_fall":2})", 1000);
    receiver.checkExpiryForTest(3001);
    const DetectionCounts last = qvariant_cast<DetectionCounts>(spy.takeLast().at(0));
    QVERIFY(!last.fresh);
}

QTEST_MAIN(DetectionCountReceiverTest)
#include "test_detection_count_receiver.moc"
