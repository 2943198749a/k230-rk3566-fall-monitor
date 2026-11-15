# FFmpeg RTSP TCP 嵌入播放设计

## 意图

替换 RK3566 上无法工作的 Qt Multimedia/GStreamer 后端，以已验证可用的 FFmpeg 协议栈在现有 Qt 视频区域内显示 K230 RTSP 画面。

## 环境证据

- 板端 `ffplay -rtsp_transport tcp` 能播放 K230 流。
- Qt `QMediaPlayer` 报“Could not open resource for reading and writing”。
- 板端 GStreamer RTSP pipeline 异常退出。
- RK3566 已有 FFmpeg 4.2.7 及 `libavformat-dev`、`libavcodec-dev`、`libavutil-dev` 开发库。

## 目标

- 使用 FFmpeg 强制 RTSP over TCP。
- 后台线程打开流、解码视频、转为 RGB24，并通过 Qt 信号把 `QImage` 送回主线程显示。
- 显示首帧前保留“连接视频流”提示；连接失败或中断显示准确错误，但不影响 TCP 服务端。
- “重连视频”与“应用配置”安全停止旧解码线程后以新 RTSP 地址重启。
- 移除 Qt Multimedia 的运行时依赖，保留当前 UI 视频区域。

## 非目标

- 不播放音频、不录制视频、不改变 K230 流编码。
- 不改 TCP STATUS 协议、告警界面或开机自启服务。

## 方案比较

1. ffplay 独立窗口：已经可播放，但不能嵌入当前 Qt 界面。
2. FFmpeg 解码 + QImage（选定）：复用已验证协议栈，视频嵌入 Qt 监控台。
3. 修复 GStreamer：板端 pipeline 本身异常，排查成本高且不确定。

## 实现

- 新增 `RtspDecoder` QObject，移至专用 `QThread`；使用 `avformat_open_input` 的 `rtsp_transport=tcp` 选项。
- 找到视频流后，用 `AVCodecContext` 解码，`sws_scale` 转成 RGB24，并发送 `frameReady(QImage)`。
- `Widget` 用 `QLabel` 代替 `QVideoWidget` 承载帧，按保持比例缩放。
- 应用关闭或重连时通过原子停止标志中断读帧、等待线程退出，避免 UI 线程阻塞。
- `.pro` 链接 `avformat avcodec avutil swscale`。
