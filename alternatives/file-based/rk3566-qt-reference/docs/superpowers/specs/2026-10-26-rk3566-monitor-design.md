# RK3566 跌倒监控服务端设计

## 意图

在 RK3566 上提供一个 Qt 桌面监控台，接收 K230 客户端的跌倒识别统计与 RTSP 画面，让现场人员一眼看到当前画面、人数和网络连接情况。

## 目标

- 固定监听 `192.168.10.3:9000`，提供启动/停止控制。
- 解析以换行结束的 `STATUS` 文本消息，并兼容 TCP 粘包、半包。
- 校验 `seq`、`total`、`standing`、`fallen`、`state`；有效消息回复 `ACK seq=<seq>\n`，无效消息回复 `ERR reason=invalid_status\n`。
- 显示连接状态、客户端地址、最后接收时间、未跌倒/跌倒/总人数，以及按时间记录的事件日志。
- 当 `fallen > 0` 时使用明显红色告警卡片；否则显示绿色安全状态。
- 播放 K230 RTSP 地址 `rtsp://192.168.50.2:8554/BackChannelTest`；画面不可用时在视频区域显示明确的错误原因。
- 网络 API（监听、连接、收包、解析、回包、断开）使用 Doxygen 风格中文注释。

## 非目标

- 不修改 K230 客户端协议或实现 YOLO 推理。
- 不做历史数据库、多客户端聚合、认证与远程控制。

## 方案比较

1. 外部 GStreamer 播放窗口：可利用现有 GStreamer，但不能自然嵌入 Qt 面板，交互与部署较差。
2. Qt Multimedia 嵌入播放（选定）：用 `QMediaPlayer + QVideoWidget` 直接嵌入监控台，状态与视频在同一窗口展示；需要 `qtmultimedia5-dev` 及可用的 GStreamer 多媒体后端。

## 实现轮廓

- `Widget` 独占一个 `QTcpServer` 和当前 `QTcpSocket`；新连接替换旧连接并记录日志。
- 每个 socket 维护接收缓冲区，循环按 `\n` 提取完整帧；一帧一 ACK。
- 通过正则/键值提取校验格式和数字一致性，避免部分 TCP 数据被误解析。
- 重新组织 `.ui`：深色标题区、左侧 RTSP 视频区、右侧三个统计卡与告警横幅、底部日志区和控制按钮；用 QSS 统一视觉。
- 启动时填入所需 IP/端口，实际绑定失败时在日志和状态区明确展示错误。
- 使用 Qt 5 `QMediaPlayer` 加载 RTSP URL，并将视频输出绑定到 `QVideoWidget`；记录媒体错误并给出重新播放按钮。

## 验证

- 用 qmake/make 编译 Qt 工程。
- 使用本机 TCP 客户端发送有效、拆分和非法 `STATUS` 帧，确认状态更新及 ACK/ERR。
- 在 K230 RTSP 地址可达时确认视频显示；无流或网络中断时确认界面显示错误但 TCP 服务继续运行。
- 启动 Qt Creator 打开 `rk.pro`。

## 环境发现

本机为 Qt 5.15.13，已发现 GStreamer 的 `rtspsrc`，但尚未安装 Qt Multimedia 开发库。实现与编译前需安装 `qtmultimedia5-dev`；运行时还需要目标机的 Qt Multimedia/GStreamer 后端支持对应的视频编码。
