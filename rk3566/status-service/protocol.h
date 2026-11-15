/*
 * 交付版注释
 * 文件：rk3566/status-service/protocol.h
 * 职责：声明严格 STATUS 文本协议、格式化、行缓冲和状态超时接口。
 * 设计原因：TCP 没有消息边界；独立协议层统一处理半包、粘包、数值范围和字段一致性。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#ifndef K230_RK3566_PROTOCOL_H
#define K230_RK3566_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* K230 发送给 RK3566 的一条状态消息。 */
typedef struct {
    unsigned int sequence;
    unsigned int total;
    unsigned int standing;
    unsigned int fallen;
    int is_fall; /* 0 = SAFE，1 = FALL */
} DetectionStatus;

/* 缓存 TCP 收到但尚未凑成完整一行的数据。 */
typedef struct {
    char data[1024];
    size_t length;
} LineBuffer;

/* 解析一行 STATUS 文本。成功返回 0，失败返回 -1。 */
int parse_status_line(const char *line, DetectionStatus *status);

/* 把状态格式化为可通过 TCP 发送的一行文本。成功返回写入字符数。 */
int format_status_line(const DetectionStatus *status, char *output,
                       size_t output_size);

/* 把 TCP 新收到的字节追加到缓存中。缓存溢出返回 -1。 */
int line_buffer_append(LineBuffer *buffer, const char *data, size_t length);

/*
 * 若缓存中已有完整的 '\n' 结尾行，复制到 output 并从缓存移除。
 * 返回 1：取到一行；0：数据尚不完整；-1：输出缓冲区太小。
 */
int line_buffer_take_line(LineBuffer *buffer, char *output,
                          size_t output_size);

/*
 * 只根据最后一条合法状态的单调时钟时间判断是否过期。
 * socket 上的非法行、半包或连接事件都不能延长已有 SAFE/FALL 的寿命。
 */
int status_is_stale(int has_status, uint64_t last_valid_ms,
                    uint64_t now_ms, uint64_t timeout_ms);

#endif
