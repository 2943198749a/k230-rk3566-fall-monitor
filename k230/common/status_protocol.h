/*
 * 交付版注释
 * 文件：k230/common/status_protocol.h
 * 职责：定义跨 K230 大核/小核 DataFIFO 传输的固定 64 字节状态 ABI。
 * 设计原因：魔数、版本号、对齐和 static_assert 防止两端结构体布局不一致造成静默误读。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <cstdint>

constexpr std::uint32_t kDetectionStatusMagic = 0x46414C4Cu; // "FALL"
constexpr std::uint32_t kDetectionStatusVersion = 1u;

// K230 DataFIFO uses a cache-line-sized item.  Keep the wire message at one
// 64-byte cache line so both the big and little cores use an ABI-valid item.
struct alignas(64) DetectionStatus {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t sequence;
    std::uint32_t total;
    std::uint32_t standing;
    std::uint32_t fallen;
};

static_assert(sizeof(DetectionStatus) == 64,
              "DetectionStatus must remain one 64-byte wire record");

inline bool is_valid_detection_status(const DetectionStatus &status)
{
    if (status.magic != kDetectionStatusMagic ||
        status.version != kDetectionStatusVersion ||
        status.standing > status.total) {
        return false;
    }

    return status.fallen == status.total - status.standing;
}
