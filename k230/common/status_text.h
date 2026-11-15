/*
 * 交付版注释
 * 文件：k230/common/status_text.h
 * 职责：把二进制 DetectionStatus 格式化为换行分隔的 TCP 文本协议。
 * 设计原因：文本协议便于 RK3566、nc 和日志直接观察，换行提供 TCP 应用层消息边界。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <string>

#include "status_protocol.h"

inline std::string format_status_line(const DetectionStatus &status)
{
    const char *state = status.fallen > 0 ? "FALL" : "SAFE";
    return "STATUS seq=" + std::to_string(status.sequence) +
           " total=" + std::to_string(status.total) +
           " standing=" + std::to_string(status.standing) +
           " fallen=" + std::to_string(status.fallen) +
           " state=" + state + "\n";
}
