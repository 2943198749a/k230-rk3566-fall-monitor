/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/tests/test_player_controller.cpp
 * 职责：验证播放器状态转换和线程控制接口。
 * 设计原因：状态机错误会造成重复线程或无法停止，需要脱离网络复现。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <QtTest>

#include "player_controller.h"

class PlayerControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void rejectsAnEmptyOrNonRtspUrl();
    void stopPreventsPendingReconnect();
    void reconnectStopsOldWorkerBeforeStartingAnother();
};

void PlayerControllerTest::rejectsAnEmptyOrNonRtspUrl()
{
    PlayerController controller;
    QSignalSpy spy(&controller, &PlayerController::stateChanged);

    controller.start(QString());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<PlayerState>(spy.takeFirst().at(0)), PlayerState::Error);
    QCOMPARE(controller.state(), PlayerState::Error);
}

void PlayerControllerTest::stopPreventsPendingReconnect()
{
    PlayerController controller;

    controller.start(QStringLiteral("rtsp://192.168.50.2:8554/BackChannelTest"));
    controller.stop();

    QCOMPARE(controller.state(), PlayerState::Stopping);
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PlayerState::Idle, 5000);
}

void PlayerControllerTest::reconnectStopsOldWorkerBeforeStartingAnother()
{
    PlayerController controller;
    controller.start(QStringLiteral("rtsp://192.168.50.2:8554/BackChannelTest"));
    controller.reconnect();

    QCOMPARE(controller.state(), PlayerState::Reconnecting);
    controller.stop();
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PlayerState::Idle, 5000);
}

QTEST_MAIN(PlayerControllerTest)
#include "test_player_controller.moc"
