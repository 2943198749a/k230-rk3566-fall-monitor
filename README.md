# K230 / RK3566 跌倒监控

本仓库只保留以下四部分（2026-11-15 更新）：

| 目录 | 内容 |
| --- | --- |
| `k230-client/` | 用户指定的桌面版 `status_client_simple.c`，原样上传 |
| `rk3566-qt/` | RK3566 Qt 工程源码，入口 `rk.pro` |
| `k230-model/` | 本套 Linux 推理程序配套的 `yolov5n-falldown.kmodel` |
| `k230-rtsp/` | 推理结果发布、视频采集/OSD/H.264/RTSP 源码及必要构建脚本 |

## 数据流

状态：YOLO 推理 → `/tmp/yolo_fall_boxes` → 简化版客户端 → RK3566 Qt TCP 服务 → ACK。

视频：K230 摄像头 → OSD / H.264 编码 → RTSP → RK3566 Qt / FFmpeg。

`k230-rtsp/source/ai_demo/` 提供文件客户端配套的推理和检测结果发布；`source/rtsp/` 是推流程序，`source/librtsp_server/` 是配套 RTSP 服务库。仓库当前目录不再提供 FIFO 客户端、双核 RT-Smart 版本、独立状态服务端、模拟客户端及旧版界面。

## 构建入口

```sh
gcc -std=c11 -Wall -Wextra k230-client/status_client_simple.c -o status_client -lm
```

客户端 IP、端口和文件路径由源码内宏配置，修改后重新编译。Qt 用 qmake 打开 `rk3566-qt/rk.pro`；视频解码需匹配的 FFmpeg 库。

K230 推流与推理用 `k230-rtsp/scripts/build.sh`，需提供匹配的 `k230_linux_sdk`、staging 和交叉工具链，通过 `SDK_ROOT` / `TOOLCHAIN_ROOT` 设置路径。源包对应 SDK commit 为 `02d4630a639247146ec3660d4ae9f83f017913c6`、nncase 2.11.0，部署前核对板端镜像。

此套 RTSP 默认地址为 `rtsp://192.168.10.2:8556/yolo`；Qt 保留原始默认地址，部署时在界面中修改为实际地址。TCP 状态端口和 RTSP 端口是独立通道。

## 范围

模型取自本套 Linux 源包，未混用先前 RT-Smart 的同名模型。原始许可和源文件声明保留。本次客户端通过主机 GCC 编译检查，没有重新进行板端验证。未提供训练数据或训练脚本。
