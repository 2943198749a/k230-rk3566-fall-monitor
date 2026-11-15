# RK3566 7 寸全屏与可配置地址 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 1024×600 七寸屏全屏显示监控台，并让监听端点和 RTSP 地址可在界面中修改。

**Architecture:** `Widget` 保存当前监听地址、端口与 RTSP URL，并通过顶部输入控件编辑。`applyConfiguration()` 校验候选值，停止旧端点后重启 `QTcpServer` 和 `QMediaPlayer`；布局压缩为 600 像素高的全屏界面。

**Tech Stack:** C++17、Qt 5.12+ Widgets/Network/Multimedia。

**Spec:** `docs/superpowers/specs/2026-10-26-7inch-config-design.md`

## Global Constraints

- 默认监听 `192.168.10.3:9000`。
- 默认 RTSP 为 `rtsp://192.168.10.2:8554/BackChannelTest`。
- 无效配置不改变当前生效的监听与视频端点。
- 不修改 K230 TCP 协议或 RTSP 解码逻辑。

## Review Focus

- 端口 0、超过 65535 或非数字输入必须拒绝。
- 无效 IPv4 和非 rtsp URL 不得让服务停止。
- 应用配置后旧客户端必须断开，新的监听端点生效。
- 1024×600 下日志、配置栏和告警区必须均可见。

---

### Task 1: 可配置端点和全屏布局

**Files:**
- Modify: `widget.h`
- Modify: `widget.cpp`
- Modify: `main.cpp`

**Interfaces:**
- Produces: `Widget::applyConfiguration()`，读取 `QLineEdit`/`QSpinBox` 并返回到有效端点。

- [ ] **Step 1: 写入配置校验的失败 QTest**

  断言合法 IPv4、1–65535 端口、`rtsp` URL 通过；错误 IP、端口和 URL 拒绝。

- [ ] **Step 2: 运行测试确认失败**

  Run: `make -C build -f Makefile.configtest`
  Expected: 因校验 API 不存在而失败。

- [ ] **Step 3: 实现配置校验和应用行为**

  默认值使用规范固定值；更新成员端点后调用现有停止/启动与播放器逻辑；在顶部放置输入框和应用按钮。

- [ ] **Step 4: 将窗口设为全屏并压缩布局**

  在 `main.cpp` 调用 `showFullScreen()`；日志高度约 80 像素，移除固定的大尺寸视频最小高度。

- [ ] **Step 5: 本机编译和协议回归**

  Run: `qmake rk.pro -o build/Makefile && make -C build -j2 && ./build/statusprotocol_test -txt`
  Expected: 编译成功，7 项协议测试通过。

### Task 2: 板端部署验证

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 更新运行说明**

  说明配置栏默认值与应用配置行为。

- [ ] **Step 2: 部署并在 RK3566 编译启动**

  Run: 通过 SSH 同步源代码，板端 `qmake`、`make`，然后以 `DISPLAY=:0` 启动。
  Expected: AArch64 可执行文件运行并监听默认端口。
