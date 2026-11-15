/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/detection_count_receiver.h
 * 职责：接收早期方案的 UDP JSON 跌倒/未跌倒计数并校验。
 * 设计原因：该模块与 RTSP 解码独立，保留用于兼容旧部署；当前 TCP 状态链路由 status-service 展示。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once
#include <QObject>
#include <QUdpSocket>

struct DetectionCounts { int fall = -1; int noFall = -1; bool fresh = false; };
Q_DECLARE_METATYPE(DetectionCounts)

class DetectionCountReceiver final : public QObject {
    Q_OBJECT
public:
    explicit DetectionCountReceiver(QObject *parent = nullptr);
    bool start(quint16 port = 5001);
    void processPayloadForTest(const QByteArray &payload, qint64 nowMs);
    void checkExpiryForTest(qint64 nowMs);
signals:
    void countsChanged(const DetectionCounts &counts);
private slots:
    void readPendingDatagrams();
private:
    bool acceptPayload(const QByteArray &payload, qint64 nowMs);
    QUdpSocket socket_;
    qint64 lastValidMs_ = -1;
    bool staleEmitted_ = false;
};
