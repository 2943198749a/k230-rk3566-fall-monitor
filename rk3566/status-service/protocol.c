/*
 * 交付版注释
 * 文件：rk3566/status-service/protocol.c
 * 职责：实现严格 STATUS 文本协议、格式化和 TCP 行缓冲。
 * 设计原因：TCP 没有消息边界；独立协议层统一处理半包、粘包、数值范围和字段一致性。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "protocol.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_unsigned_decimal(const char *text, unsigned int *value)
{
    const unsigned char *cursor;
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == '\0')
        return -1;

    /* strtoul 接受前导 +/-，协议不接受；先逐字符限定为十进制数字。 */
    for (cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        if (!isdigit(*cursor))
            return -1;
    }

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed > UINT_MAX)
        return -1;

    *value = (unsigned int)parsed;
    return 0;
}

int parse_status_line(const char *line, DetectionStatus *status)
{
    unsigned int sequence;
    unsigned int total;
    unsigned int standing;
    unsigned int fallen;
    char sequence_text[32] = {0};
    char total_text[32] = {0};
    char standing_text[32] = {0};
    char fallen_text[32] = {0};
    char state[8] = {0};
    char extra;
    int fields;

    if (line == NULL || status == NULL)
        return -1;

    /*
     * 最后的 %c 专门用于发现多余字符。
     * 先把数字保留为字符串，避免 sscanf("%u") 接受 -1 或溢出后回绕。
     * 正确行只匹配 4 个数字 token 和 1 个 state 字符串，共 5 项；若还有
     * 任意尾随字符（包括空格后内容），%c 会成为第 6 项，从而被拒绝。
     */
    fields = sscanf(line,
                    "STATUS seq=%31s total=%31s standing=%31s fallen=%31s state=%7s%c",
                    sequence_text, total_text, standing_text, fallen_text,
                    state, &extra);
    if (fields != 5)
        return -1;

    if (parse_unsigned_decimal(sequence_text, &sequence) < 0 ||
        parse_unsigned_decimal(total_text, &total) < 0 ||
        parse_unsigned_decimal(standing_text, &standing) < 0 ||
        parse_unsigned_decimal(fallen_text, &fallen) < 0)
        return -1;

    /* 人数守恒：站立人数加跌倒人数必须等于总人数。 */
    if (standing > total || fallen != total - standing)
        return -1;

    if (strcmp(state, "SAFE") == 0) {
        if (fallen != 0)
            return -1;
        status->is_fall = 0;
    } else if (strcmp(state, "FALL") == 0) {
        if (fallen == 0)
            return -1;
        status->is_fall = 1;
    } else {
        return -1;
    }

    status->sequence = sequence;
    status->total = total;
    status->standing = standing;
    status->fallen = fallen;
    return 0;
}

int format_status_line(const DetectionStatus *status, char *output,
                       size_t output_size)
{
    const char *state;
    int written;

    if (status == NULL || output == NULL || output_size == 0 ||
        status->standing > status->total ||
        status->fallen != status->total - status->standing)
        return -1;

    state = status->is_fall ? "FALL" : "SAFE";
    if ((status->is_fall && status->fallen == 0) ||
        (!status->is_fall && status->fallen != 0))
        return -1;

    written = snprintf(output, output_size,
                       "STATUS seq=%u total=%u standing=%u fallen=%u state=%s\n",
                       status->sequence, status->total, status->standing,
                       status->fallen, state);
    if (written < 0 || (size_t)written >= output_size)
        return -1;
    return written;
}

int line_buffer_append(LineBuffer *buffer, const char *data, size_t length)
{
    // append 只接收字节，不假设一次 recv 对应一条消息。容量检查必须先做，
    // 否则异常客户端发送无换行长串时会覆盖固定缓冲区。
    if (buffer == NULL || data == NULL || length > sizeof(buffer->data) - buffer->length)
        return -1;

    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    return 0;
}

int line_buffer_take_line(LineBuffer *buffer, char *output, size_t output_size)
{
    size_t index;
    size_t line_length;
    size_t remaining;

    if (buffer == NULL || output == NULL || output_size == 0)
        return -1;

    for (index = 0; index < buffer->length; ++index) {
        if (buffer->data[index] == '\n')
            break;
    }
    if (index == buffer->length)
        return 0; /* TCP 本次只收到了半包，继续等待下一次 recv。 */

    line_length = index;
    if (line_length > 0 && buffer->data[line_length - 1] == '\r')
        --line_length; /* 兼容 \r\n 结尾。 */
    if (line_length + 1 > output_size)
        return -1;

    memcpy(output, buffer->data, line_length);
    output[line_length] = '\0';

    // 取走第一行后把剩余字节前移；剩余区可能含下一条完整消息（粘包），
    // 也可能只是下一条消息的开头（半包），调用者会循环提取直到返回 0。
    remaining = buffer->length - (index + 1);
    memmove(buffer->data, buffer->data + index + 1, remaining);
    buffer->length = remaining;
    return 1;
}

int status_is_stale(int has_status, uint64_t last_valid_ms,
                    uint64_t now_ms, uint64_t timeout_ms)
{
    if (!has_status || now_ms < last_valid_ms)
        return 0;
    return now_ms - last_valid_ms >= timeout_ms;
}
