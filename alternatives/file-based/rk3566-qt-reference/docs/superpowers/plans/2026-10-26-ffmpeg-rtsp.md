# FFmpeg RTSP TCP 嵌入播放 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 使用 FFmpeg 的 RTSP/TCP 解码能力在当前 Qt 监控台中嵌入 K230 视频。

**Architecture:** `RtspDecoder` 在专用线程使用 libavformat/libavcodec 打开、解码和转换视频帧，通过 `QImage` 信号传递给主线程。`Widget` 以 QLabel 呈现帧并负责启动、停止和错误显示，既有 TCP 服务独立运行。

**Tech Stack:** Qt 5.12+、C++17、FFmpeg 4.2 libavformat/libavcodec/libavutil/libswscale。

**Spec:** `docs/superpowers/specs/2026-10-26-ffmpeg-rtsp-design.md`

## Global Constraints

- RTSP 必须强制使用 TCP。
- 解码不能阻塞 Qt GUI 线程。
- 视频连接失败不得影响 TCP 监听和人数统计。
- 不播放音频，不改变 K230 协议。

## Review Focus

- 重复重连不能遗留解码线程或崩溃。
- 无效 RTSP URL 必须给出错误并保持 GUI 可操作。
- 仅在主线程更新 QLabel，避免跨线程 QWidget 操作。
- 非 16:9 视频应保持比例并留黑边，不拉伸。

---

### Task 1: FFmpeg 解码器

**Files:**
- Create: `rtspdecoder.h`
- Create: `rtspdecoder.cpp`
- Modify: `rk.pro`

**Interfaces:**
- Produces: `RtspDecoder::start(const QUrl &)`, `stop()`，信号 `frameReady(const QImage &)`, `errorOccurred(const QString &)`。

- [ ] **Step 1: 写入 URL 校验/启动失败的失败测试**
- [ ] **Step 2: 实现 RTSP/TCP 打开、解码和 RGB24 转换**
- [ ] **Step 3: 编译 FFmpeg 链接并验证测试通过**

### Task 2: 接入监控台与生命周期

**Files:**
- Modify: `widget.h`
- Modify: `widget.cpp`
- Modify: `main.cpp`

- [ ] **Step 1: 用 QLabel 替换 QVideoWidget，并添加帧显示槽**
- [ ] **Step 2: 用线程托管 RtspDecoder，重连/关闭时停止并等待线程**
- [ ] **Step 3: 运行本机构建与既有协议测试**

### Task 3: 板端验证

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 更新 FFmpeg 依赖与播放说明**
- [ ] **Step 2: 部署、板端编译、重启 systemd 服务**
- [ ] **Step 3: 确认服务运行、9000 监听且日志出现 FFmpeg 首帧或明确错误**
