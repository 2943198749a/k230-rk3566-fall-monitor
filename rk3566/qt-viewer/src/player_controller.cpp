/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/player_controller.cpp
 * 职责：管理解码线程、播放器状态、统计以及用户触发的安全停止/重连。
 * 设计原因：控制层隔离界面与 FFmpeg 生命周期，避免 GUI 直接跨线程操作解码对象。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "player_controller.h"

#include <QThread>
#include "rtsp_decoder.h"

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<PlayerState>("PlayerState");
    qRegisterMetaType<PlayerStats>("PlayerStats");
}

PlayerController::~PlayerController()
{
    pendingStart_ = false;
    if (decoder_)
        decoder_->stopDecoding();
    if (workerThread_) {
        workerThread_->quit();
        // interrupt callback 会让 FFmpeg 阻塞调用退出。这里必须等线程真正结束，
        // 不能超时后继续析构带 parent 的 QThread，否则会销毁运行中线程。
        workerThread_->wait();
    }
}

void PlayerController::start(const QString &url)
{
    const QString normalizedUrl = url.trimmed();
    if (normalizedUrl.isEmpty() || !normalizedUrl.startsWith(QStringLiteral("rtsp://"))) {
        setState(PlayerState::Error, QStringLiteral("请输入有效的 rtsp:// 视频地址"));
        return;
    }

    lastUrl_ = normalizedUrl;

    if (workerThread_) {
        // 一个 decoder 的 startDecoding 正在其线程中阻塞执行，不能再向同一
        // event loop 排队第二次 start。先用原子停止标志终止旧解码，待线程的
        // finished 信号完成清理后，onWorkerFinished 再创建全新的线程/decoder。
        pendingUrl_ = normalizedUrl;
        pendingStart_ = true;
        setState(PlayerState::Reconnecting, QStringLiteral("正在停止旧连接并重连"));
        if (decoder_)
            decoder_->stopDecoding();
        return;
    }

    pendingStart_ = false;
    pendingUrl_.clear();
    setState(PlayerState::Connecting, QStringLiteral("正在连接视频流"));
    workerThread_ = new QThread(this);
    decoder_ = new RtspDecoder();
    decoder_->moveToThread(workerThread_);
    connect(workerThread_, &QThread::finished, decoder_, &QObject::deleteLater);
    connect(decoder_, &RtspDecoder::finished, workerThread_, &QThread::quit);
    connect(decoder_, &RtspDecoder::frameReady, this, &PlayerController::frameReady);
    connect(decoder_, &RtspDecoder::statsReady, this, &PlayerController::statsReady);
    connect(decoder_, &RtspDecoder::failed, this, [this](const QString &message) { setState(PlayerState::Error, message); });
    connect(workerThread_, &QThread::finished, this, &PlayerController::onWorkerFinished);
    connect(this, &PlayerController::startRequested, decoder_, &RtspDecoder::startDecoding, Qt::QueuedConnection);
    workerThread_->start();
    emit startRequested(lastUrl_);
}

void PlayerController::stop()
{
    pendingStart_ = false;
    pendingUrl_.clear();
    if (decoder_) {
        setState(PlayerState::Stopping, QStringLiteral("正在停止"));
        decoder_->stopDecoding();
    } else {
        setState(PlayerState::Idle, QStringLiteral("已停止"));
    }
}

void PlayerController::onWorkerFinished()
{
    if (sender() != workerThread_) {
        return;
    }
    decoder_ = nullptr;
    if (workerThread_) { workerThread_->deleteLater(); workerThread_ = nullptr; }
    if (pendingStart_) {
        const QString nextUrl = pendingUrl_;
        pendingStart_ = false;
        pendingUrl_.clear();
        start(nextUrl);
        return;
    }
    if (state_ == PlayerState::Stopping)
        setState(PlayerState::Idle, QStringLiteral("已停止"));
}

void PlayerController::reconnect()
{
    if (lastUrl_.isEmpty()) {
        setState(PlayerState::Error, QStringLiteral("没有可重连的视频地址"));
        return;
    }

    // start() 在已有 worker 时会先请求停止，再由 finished 回调创建新 worker。
    start(lastUrl_);
}

PlayerState PlayerController::state() const
{
    return state_;
}

void PlayerController::setState(PlayerState state, const QString &message)
{
    state_ = state;
    emit stateChanged(state_, message);
}
