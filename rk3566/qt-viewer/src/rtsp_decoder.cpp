/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/rtsp_decoder.cpp
 * 职责：在工作线程中用 FFmpeg 打开 RTSP、解码视频并转换为 QImage。
 * 设计原因：FFmpeg 阻塞 I/O 不应占用 GUI 线程；中断回调确保停止和重连能够及时退出。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "rtsp_decoder.h"

#include <QDateTime>

#include "frame_rate_meter.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace {
QString ffmpegError(int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return QString::fromLocal8Bit(buffer);
}
}

RtspDecoder::RtspDecoder(QObject *parent) : QObject(parent) {}

int RtspDecoder::interruptCallback(void *opaque)
{
    return static_cast<RtspDecoder *>(opaque)->stopRequested_.load() ? 1 : 0;
}

void RtspDecoder::stopDecoding()
{
    stopRequested_.store(true);
}

void RtspDecoder::startDecoding(const QString &url)
{
    // 每次连接都创建新的 RtspDecoder；不要在这里清零停止标志。否则 GUI 在
    // queued start 真正执行前调用 stop() 时，取消请求会被这一行覆盖。
    AVFormatContext *format = avformat_alloc_context();
    // FFmpeg 的网络读取可能长时间阻塞；interrupt_callback 查询原子停止标志，
    // 使“停止/关闭窗口”不必等到网络超时，且不会跨线程销毁正在使用的上下文。
    format->interrupt_callback.callback = &RtspDecoder::interruptCallback;
    format->interrupt_callback.opaque = this;
    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "udp", 0);
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "stimeout", "3000000", 0);
    int result = avformat_open_input(&format, url.toUtf8().constData(), nullptr, &options);
    av_dict_free(&options);
    if (result < 0) { emit failed(QStringLiteral("连接失败：%1").arg(ffmpegError(result))); if (format) avformat_free_context(format); emit finished(); return; }
    result = avformat_find_stream_info(format, nullptr);
    const int streamIndex = result < 0 ? -1 : av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) { emit failed(QStringLiteral("读取视频流失败：%1").arg(ffmpegError(result))); avformat_close_input(&format); emit finished(); return; }
    AVStream *stream = format->streams[streamIndex];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext *context = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!context || avcodec_parameters_to_context(context, stream->codecpar) < 0 || avcodec_open2(context, codec, nullptr) < 0) { emit failed(QStringLiteral("无法打开 H.264 解码器")); if (context) avcodec_free_context(&context); avformat_close_input(&format); emit finished(); return; }
    AVFrame *frame = av_frame_alloc(); AVPacket *packet = av_packet_alloc();
    SwsContext *sws = nullptr; FrameRateMeter meter; qint64 lastStats = 0;
    while (!stopRequested_.load() && av_read_frame(format, packet) >= 0) {
        if (packet->stream_index == streamIndex && avcodec_send_packet(context, packet) >= 0) {
            while (!stopRequested_.load() && avcodec_receive_frame(context, frame) == 0) {
                sws = sws_getCachedContext(sws, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), frame->width, frame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
                // sws_scale 把解码器可能输出的 YUV/NV12 等格式统一转成 Qt 可绘制
                // 的 RGBA。QImage 本身拥有像素内存；这里再 copy()，明确让跨线程
                // 信号携带独立副本，避免以后改成外部缓冲构造 QImage 时引入悬空引用。
                QImage image(frame->width, frame->height, QImage::Format_RGBA8888);
                uint8_t *dst[] = { image.bits() }; int lines[] = { image.bytesPerLine() };
                sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, lines);
                emit frameReady(image.copy());
                const qint64 now = QDateTime::currentMSecsSinceEpoch(); meter.recordFrame(now);
                if (now - lastStats >= 1000) { emit statsReady({meter.framesPerSecond(), image.size(), meter.frameCount()}); lastStats = now; }
            }
        }
        av_packet_unref(packet);
    }
    sws_freeContext(sws); av_packet_free(&packet); av_frame_free(&frame); avcodec_free_context(&context); avformat_close_input(&format);
    emit finished();
}
