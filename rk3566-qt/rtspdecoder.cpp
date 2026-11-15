#include "rtspdecoder.h"

#ifdef HAVE_FFMPEG
//调用C库
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace {
/**
 * 将 FFmpeg 的负错误码转换为可记录和展示的文字。
 * FFmpeg 不使用 C++ 异常，所有调用方都必须检查返回值并将该错误
 * 文本附加到 UI 日志，才能区分认证、网络和编解码器问题。
 */
QString ffmpegError(int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, text, sizeof(text));
    return QString::fromLocal8Bit(text);
}
/**
 * FFmpeg 在网络读取过程中周期性调用此回调。
 * QThread 的中断标记由 GUI 线程设置；返回非零会让阻塞的
 * av_read_frame()/avformat_open_input() 尽快返回，从而保证关闭窗口
 * 或切换 RTSP 地址时不会无限等待网络超时。
 */
int interruptCallback(void *opaque)
{
    return static_cast<RtspDecoder *>(opaque)->isInterruptionRequested() ? 1 : 0;
}
}
#endif

/** 初始化线程对象；实际网络连接在 run() 内发生，而非构造阶段。 */
RtspDecoder::RtspDecoder(QObject *parent) : QThread(parent) {}

/** 析构时同步停止线程，确保 FFmpeg 回调不会访问已经销毁的对象。 */
RtspDecoder::~RtspDecoder() { stop(); }

/**
 * 保存下一次解码任务的地址。
 * Widget 先调用 stop()，确认线程退出后才调用本函数，因此 m_url
 * 不会被工作线程并发读取和写入。
 */
void RtspDecoder::setUrl(const QUrl &url) { m_url = url; }

/**
 * 先置中断标记，再最多等待 2.5 秒。
 * 中断回调会解除 FFmpeg 的网络阻塞；超时仍返回是为了避免不响应的
 * RTSP 服务端永久卡住 GUI 线程，后续析构阶段由 QThread 再负责收尾。
 */
void RtspDecoder::stop() { requestInterruption(); wait(2500); }

/**
 * 在工作线程中执行完整的 FFmpeg 生命周期：
 * 1. 打开 RTSP/TCP 输入并读取流信息；2. 找到并初始化视频解码器；
 * 3. 将每个解码帧转换为独立拥有内存的 RGB QImage；4. 按分配的反向
 * 顺序释放所有资源。任何失败都会发信号回 GUI 线程，而不会抛出异常。
 */
void RtspDecoder::run()
{
#ifdef HAVE_FFMPEG
    // 允许 FFmpeg 使用网络协议；重复调用在 FFmpeg 中是安全的。
    avformat_network_init();
    AVFormatContext *format = avformat_alloc_context();
    // 分配失败时不能解引用 format，也无法安装中断回调。
    if (!format) {
        emit errorOccurred(QStringLiteral("FFmpeg 输入上下文分配失败"));
        return;
    }
    format->interrupt_callback.callback = interruptCallback;
    format->interrupt_callback.opaque = this;
    AVDictionary *options = nullptr;
    // K230 的 RTSP 流要求 TCP，stimeout 使用微秒避免网络异常时永久阻塞。
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0);
    int result = avformat_open_input(&format, m_url.toString().toUtf8().constData(), nullptr, &options);
    av_dict_free(&options);
    if (result < 0) {
        emit errorOccurred(QStringLiteral("FFmpeg 打开 RTSP 失败：%1").arg(ffmpegError(result)));
        // open_input 失败时 format 仍可能由调用方持有，需要显式释放。
        avformat_free_context(format);
        return;
    }
    if ((result = avformat_find_stream_info(format, nullptr)) < 0) {
        emit errorOccurred(QStringLiteral("读取视频流信息失败：%1").arg(ffmpegError(result)));
        avformat_close_input(&format);
        return;
    }
    const int videoIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIndex < 0) {
        emit errorOccurred(QStringLiteral("RTSP 中没有视频流"));
        avformat_close_input(&format);
        return;
    }
    AVStream *stream = format->streams[videoIndex];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext *codecContext = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!codec || !codecContext
        || avcodec_parameters_to_context(codecContext, stream->codecpar) < 0
        || avcodec_open2(codecContext, codec, nullptr) < 0) {
        emit errorOccurred(QStringLiteral("无法打开视频解码器"));
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return;
    }
    // swscale 负责把摄像头的原始像素格式统一转换为 Qt 可显示的 RGB24。
    SwsContext *sws = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt, codecContext->width, codecContext->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgb = av_frame_alloc();
    const int rgbSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
    // 先验证尺寸再转换为 size_t，避免错误码被当作超大的分配请求。
    uint8_t *rgbData = rgbSize > 0 ? static_cast<uint8_t *>(av_malloc(static_cast<size_t>(rgbSize))) : nullptr;
    if (!sws || !packet || !frame || !rgb || !rgbData) {
        emit errorOccurred(QStringLiteral("FFmpeg 帧缓冲区分配失败"));
        av_free(rgbData);
        av_frame_free(&rgb);
        av_frame_free(&frame);
        av_packet_free(&packet);
        sws_freeContext(sws);
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return;
    }
    // rgb 只描述 rgbData 的布局；rgbData 的所有权仍由本函数负责释放。
    av_image_fill_arrays(rgb->data, rgb->linesize, rgbData, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
    emit streamOpened();
    while (!isInterruptionRequested() && av_read_frame(format, packet) >= 0) {
        if (packet->stream_index == videoIndex && avcodec_send_packet(codecContext, packet) == 0) {
            while (avcodec_receive_frame(codecContext, frame) == 0) {
                // QImage 先借用 rgbData，copy() 后获得独立内存，跨线程传递安全。
                sws_scale(sws, frame->data, frame->linesize, 0, codecContext->height, rgb->data, rgb->linesize);
                QImage image(rgb->data[0], codecContext->width, codecContext->height, rgb->linesize[0], QImage::Format_RGB888);
                emit frameReady(image.copy());
            }
        }
        // 每轮释放 packet 引用，避免长时间播放导致 FFmpeg 缓冲不断增长。
        av_packet_unref(packet);
    }
    // 资源按依赖关系的反向顺序释放：图像缓冲、帧、包、转换器、解码器、输入。
    av_free(rgbData);
    av_frame_free(&rgb);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws);
    avcodec_free_context(&codecContext);
    avformat_close_input(&format);
    if (!isInterruptionRequested()) emit errorOccurred(QStringLiteral("RTSP 视频流已断开"));
#else
    emit errorOccurred(QStringLiteral("未安装 FFmpeg 开发包，RTSP 视频解码不可用"));
#endif
}
