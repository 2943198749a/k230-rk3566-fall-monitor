/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/retry_policy.h
 * 职责：保存尚未接入播放器控制器的有限退避策略实验代码。
 * 设计原因：保留已有单元测试与未来自动重连接口，但当前产品行为只有用户手动重连。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

class RetryPolicy {
public:
    int delayMsForAttempt(int attempt) const;
    bool mayRetry(int attempt) const;
};
