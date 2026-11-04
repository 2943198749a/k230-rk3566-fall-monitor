/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/player_controller.h
 * 职责：管理解码线程、播放器状态、统计以及用户触发的安全停止/重连。
 * 设计原因：控制层隔离界面与 FFmpeg 生命周期，避免 GUI 直接跨线程操作解码对象。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <QObject>
#include <QString>

#include "player_types.h"

class PlayerController final : public QObject {
    Q_OBJECT

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    void start(const QString &url);
    void stop();
    void reconnect();
    PlayerState state() const;

signals:
    void stateChanged(PlayerState state, const QString &message);
    void startRequested(const QString &url);
    void frameReady(const QImage &frame);
    void statsReady(const PlayerStats &stats);

public slots:
    void onWorkerFinished();

private:
    void setState(PlayerState state, const QString &message = QString());

    PlayerState state_ = PlayerState::Idle;
    QString lastUrl_;
    QString pendingUrl_;
    bool pendingStart_ = false;
    class QThread *workerThread_ = nullptr;
    class RtspDecoder *decoder_ = nullptr;
};
