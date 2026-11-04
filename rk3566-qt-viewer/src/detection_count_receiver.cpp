/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/detection_count_receiver.cpp
 * 职责：接收早期方案的 UDP JSON 跌倒/未跌倒计数并校验。
 * 设计原因：该模块与 RTSP 解码独立，保留用于兼容旧部署；当前 TCP 状态链路由 status-service 展示。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "detection_count_receiver.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

DetectionCountReceiver::DetectionCountReceiver(QObject *parent) : QObject(parent) { qRegisterMetaType<DetectionCounts>("DetectionCounts"); connect(&socket_, &QUdpSocket::readyRead, this, &DetectionCountReceiver::readPendingDatagrams); }
bool DetectionCountReceiver::start(quint16 port) { return socket_.bind(QHostAddress::AnyIPv4, port, QUdpSocket::DontShareAddress); }
void DetectionCountReceiver::processPayloadForTest(const QByteArray &payload, qint64 nowMs) { acceptPayload(payload, nowMs); }
void DetectionCountReceiver::checkExpiryForTest(qint64 nowMs) { if (lastValidMs_ >= 0 && !staleEmitted_ && nowMs - lastValidMs_ > 2000) { staleEmitted_ = true; emit countsChanged({}); } }
bool DetectionCountReceiver::acceptPayload(const QByteArray &payload, qint64 nowMs) {
    QJsonParseError error; const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QJsonObject object = document.object(); const QJsonValue fall = object.value("fall"); const QJsonValue noFall = object.value("no_fall");
    if (!fall.isDouble() || !noFall.isDouble()) return false;
    const double f = fall.toDouble(), n = noFall.toDouble();
    if (f < 0 || n < 0 || f > INT_MAX || n > INT_MAX || f != int(f) || n != int(n)) return false;
    lastValidMs_ = nowMs; staleEmitted_ = false; emit countsChanged({int(f), int(n), true}); return true;
}
void DetectionCountReceiver::readPendingDatagrams() { while (socket_.hasPendingDatagrams()) { QByteArray payload; payload.resize(int(socket_.pendingDatagramSize())); socket_.readDatagram(payload.data(), payload.size()); acceptPayload(payload, QDateTime::currentMSecsSinceEpoch()); } }
