# RK3566 工业上位机界面 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有 RTSP 与跌倒监控窗口改为适合现场使用的工业上位机界面。

**Architecture:** 只改 `Widget::buildInterface()` 和 `Widget::applyTheme()` 创建的布局、文字与 QSS。既有 TCP、RTSP、告警对象名和槽函数保持不变，以避免改变服务行为。

**Tech Stack:** Qt 5.15 Widgets、C++17、Qt Multimedia/Network。

**Spec:** `docs/superpowers/specs/2026-10-26-industrial-ui-design.md`

## Global Constraints

- 不修改 TCP 协议、RTSP 地址、人数处理逻辑或播放器实现。
- 使用浅灰背景、Qt 分组框、表格式状态行与普通工程按钮。
- 保留左侧视频、右侧状态数据、底部日志的工作流。

## Review Focus

- 告警更新后状态灯与文本仍必须能通过现有对象名切换颜色。
- 调整布局后视频控件在窗口缩放时仍是最大的区域。
- 开始/停止/重播按钮保持连接到原槽函数。

---

### Task 1: 工业布局与样式

**Files:**
- Modify: `widget.cpp`

**Interfaces:**
- Consumes: 现有 `m_*` 控件成员与槽函数。
- Produces: 保持现有网络/视频行为的工业上位机视觉界面。

- [ ] **Step 1: 定义 UI 验收检查**

  验收内容：窗口标题为“RK3566 跌倒检测服务端”、存在“视频监控”“服务状态”“人数统计”“通信日志”分组，且无“智慧”“实时视频”营销文案。

- [ ] **Step 2: 确认当前代码不满足检查**

  Run: `rg '智慧跌倒|实时视频' widget.cpp`
  Expected: 找到现有营销式文案。

- [ ] **Step 3: 修改 `buildInterface()` 与 `applyTheme()`**

  以 `QGroupBox` 分组视频、状态、人数和日志；用圆形 QLabel 状态灯及浅色 QSS；保持全部成员指针与信号连接。

- [ ] **Step 4: 编译和启动检查**

  Run: `qmake rk.pro -o build/Makefile && make -C build -j2 && QT_QPA_PLATFORM=offscreen timeout 3s ./build/rk`
  Expected: 编译成功，应用初始化不崩溃。

### Task 2: 回归验证与启动

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 更新运行说明中的窗口名称**

- [ ] **Step 2: 回归协议测试并启动桌面程序**

  Run: `./build/statusprotocol_test -txt && setsid -f ./build/rk`
  Expected: 7 项协议测试通过，监控台启动。
