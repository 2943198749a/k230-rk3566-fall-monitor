#ifndef RTSPDECODER_H
#define RTSPDECODER_H

#include <QImage>
#include <QThread>
#include <QUrl>

/**
 * @brief 在独立线程中通过 FFmpeg 解码 K230 的 RTSP 视频。
 *
 * Qt 的 QWidget 只能由主线程访问；本类只在工作线程完成网络收包、
 * 视频解码和像素格式转换，再用 frameReady() 将独立拥有的 QImage
 * 安全传回主线程显示。RTSP 连接固定强制使用 TCP，行为与 ffplay 的
 * `-rtsp_transport tcp` 参数一致。
 */
class RtspDecoder : public QThread
{
    //使用信号槽
    Q_OBJECT
public:
    /** @brief 创建解码线程，不会立即打开网络连接。 */
    //explicit：禁止隐式类型转换，    parentc:父销毁时子也销毁
    explicit RtspDecoder(QObject *parent = nullptr);
    /** @brief 析构前请求停止并等待线程退出，避免访问已销毁的 UI。 */
    ~RtspDecoder() override;
    /** @brief 设置下一次 start() 要打开的 RTSP 地址；线程运行期间不可修改。 */
    void setUrl(const QUrl &url);
    /** @brief 请求中断 av_read_frame() 并最多等待 2.5 秒释放 FFmpeg 资源。 */
    void stop();

signals:
    /** @brief 成功解出一帧 RGB 图像；接收方应以 QueuedConnection 更新 UI。 */
    void frameReady(const QImage &frame);
    /** @brief 打开、探测、解码或断流时发出的中文错误说明。 */
    void errorOccurred(const QString &message);
    /** @brief 已成功建立流并完成解码器初始化。 */
    void streamOpened();

protected:
    /** @brief QThread 入口：执行 RTSP/TCP 打开、解码、RGB 转换与资源回收。 */
    void run() override;

private:
    /**
     * 由 GUI 线程在 start() 前设置、由 run() 在工作线程中读取的 URL。
     * 调用约定禁止在线程运行时写入，因而不需要额外锁。
     */
    QUrl m_url;
};

#endif
