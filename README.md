# K230 + RK3566 YOLO 跌倒监控

2026-11-15 同步当前本地完整交付版本（2026-11-02 整理），并保留独立文件状态客户端分支。主版本由 K230 大核运行 YOLO 推理、OSD 和 H.264 编码，小核从 DataFIFO 转发视频与检测状态，RK3566 接收显示。

## 最新配套源码

| 目录 | 功能 |
| --- | --- |
| `k230/big-core/` | YOLO 跌倒检测前后处理、KModel 推理、视频采集/叠框/编码与状态发布 |
| `k230/little-core/rtsp-server/` | 小核 Linux RTSP 服务，接收视频 FIFO |
| `k230/little-core/status-client/` | 与上述大核匹配的 FIFO → TCP 状态客户端 |
| `k230/common/` | 大小核共享的状态布局和发布策略 |
| `k230/model/yolov5n-falldown.kmodel` | 本地配套部署模型，约 2.2 MB |
| `rk3566/status-service/` | 纯 C TCP 状态服务、协议与测试；其中模拟客户端不是板端 FIFO 客户端 |
| `rk3566/qt-viewer/` | 配套 Qt/FFmpeg 播放器及原有计数接收模块 |
| `docs/`、`scripts/` | 构建、部署、协议说明与验证脚本 |

## 版本选择

主版本请使用整套 `k230/` 与 `rk3566/`，不要混用其他版本的 DataFIFO ABI、RTSP 程序和固件。Qt 播放器与 TCP 状态服务仍是独立模块，不宣称已经整合到同一个状态界面。

`alternatives/file-based/` 保留另一套文件交换状态的实现：

- `status-client/simple/status_client_simple.c`：你确认过的简化版，读取 `/tmp/yolo_fall_boxes` 后发送 STATUS/接收 ACK。
- `status-client/src/status_client.c`：完整文件状态客户端及测试。
- `rk3566-qt-reference/`：从当前 `rk35` Qt 工作工程同步的 TCP 状态界面与 FFmpeg 解码实现。
- `video-server/`：这条文件链路的配套 RTSP 与 YOLO 发布源码。

仓库原有 `k230-server/` 和 `rk3566-qt-viewer/` 是先前发布入口，保留供比较，主入口以本页目录表为准。

## 构建与运行

从 [编译部署运行](docs/03-编译部署运行.md) 和 [版本依赖与限制](docs/06-版本依赖与限制.md) 开始。K230 需匹配的 SDK、交叉工具链、nncase/媒体依赖；构建路径应按本地环境设置。板端可执行文件未纳入这次源码同步，需要根据源码构建，文档中 `k230/bin` 运行路径代表原交付包的已有二进制布局。

Linux 主机可验证 C 状态协议：

```sh
cd rk3566/status-service
make test
```

## 算法与验证范围

已包含 YOLO 部署推理代码、前后处理、配套 `.kmodel`、测试图和运行示例。本地交付目录未提供训练数据、训练脚本、`.pt` 或 `.onnx`，因此本仓库不能单独复现模型训练/量化过程。保留 SDK/第三方原始声明，不把集成模块宣称为独立原创。

本次进行了源码同步、文件完整性与敏感信息检查，没有重新进行板端编译烧录或性能验证。上一次交付说明的测试记录应结合其日期理解。

本次主机验证（Ubuntu / GCC）：C 协议单元测试全部通过；状态服务端、模拟客户端和文件简化版客户端均通过编译。该结果不代替 K230 RTSP/YOLO 或 STM32 板端验证。
