/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/rtsp_decoder.h
 * 职责：在工作线程中用 FFmpeg 打开 RTSP、解码视频并转换为 QImage。
 * 设计原因：FFmpeg 阻塞 I/O 不应占用 GUI 线程；中断回调确保停止和重连能够及时退出。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <QObject>
#include <QImage>
#include <atomic>

#include "player_types.h"

class RtspDecoder final : public QObject {
    Q_OBJECT
public:
    explicit RtspDecoder(QObject *parent = nullptr);

public slots:
    void startDecoding(const QString &url);
    void stopDecoding();

signals:
    void frameReady(const QImage &frame);
    void statsReady(const PlayerStats &stats);
    void failed(const QString &message);
    void finished();

private:
    static int interruptCallback(void *opaque);
    std::atomic_bool stopRequested_{false};
};
