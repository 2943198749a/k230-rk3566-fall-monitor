/*
 * 交付版注释
 * 文件：k230/big-core/fall_count.h
 * 职责：把最终 NMS 检测框统计为 total/standing/fallen 状态。
 * 设计原因：只统计最终结果，避免把候选框或 NMS 前的重复框当成多人。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

#include <cstdint>
#include <vector>

#include "status_protocol.h"

constexpr int kFallLabel = 0;
constexpr int kNoFallLabel = 1;

inline DetectionStatus count_detection_labels(const std::vector<int> &labels,
                                              std::uint32_t sequence)
{
    DetectionStatus status{
        kDetectionStatusMagic,
        kDetectionStatusVersion,
        sequence,
        0u,
        0u,
        0u,
    };

    for (const int label : labels) {
        if (label == kFallLabel) {
            ++status.fallen;
            ++status.total;
        } else if (label == kNoFallLabel) {
            ++status.standing;
            ++status.total;
        }
    }

    return status;
}
