/*
 * 交付版注释
 * 文件：rk3566/status-service/tests/test_protocol.c
 * 职责：覆盖协议解析、格式化、半包和粘包的纯 C 单元测试。
 * 设计原因：网络问题常被误判为模型问题；先固定协议行为能缩小板端联调范围。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../protocol.h"

static void test_reject_trailing_characters(void)
{
    DetectionStatus status;
    int result = parse_status_line(
        "STATUS seq=7 total=3 standing=2 fallen=1 state=FALL extra", &status);

    assert(result == -1);
}

static void test_reject_negative_and_overflowing_unsigned_fields(void)
{
    DetectionStatus status;

    assert(parse_status_line(
               "STATUS seq=-1 total=0 standing=0 fallen=0 state=SAFE",
               &status) == -1);
    assert(parse_status_line(
               "STATUS seq=1 total=4294967296 standing=0 fallen=0 state=SAFE",
               &status) == -1);
    assert(parse_status_line(
               "STATUS seq=+1 total=0 standing=0 fallen=0 state=SAFE",
               &status) == -1);
}

static void test_staleness_uses_last_valid_status_time(void)
{
    /* 无论 socket 是否持续可读，只看最后一条合法状态距今多久。 */
    assert(status_is_stale(1, 1000u, 3999u, 3000u) == 0);
    assert(status_is_stale(1, 1000u, 4000u, 3000u) == 1);
    assert(status_is_stale(0, 1000u, 9000u, 3000u) == 0);
    assert(status_is_stale(1, 5000u, 4000u, 3000u) == 0);
}

static void test_parse_valid_fall_line(void)
{
    DetectionStatus status;
    int result = parse_status_line(
        "STATUS seq=7 total=3 standing=2 fallen=1 state=FALL", &status);

    assert(result == 0);
    assert(status.sequence == 7);
    assert(status.total == 3);
    assert(status.standing == 2);
    assert(status.fallen == 1);
    assert(status.is_fall == 1);
}

static void test_reject_inconsistent_counts(void)
{
    DetectionStatus status;
    int result = parse_status_line(
        "STATUS seq=7 total=3 standing=2 fallen=2 state=FALL", &status);

    assert(result == -1);
}

static void test_reassemble_split_tcp_message(void)
{
    LineBuffer buffer = {{0}, 0};
    char line[256];

    assert(line_buffer_append(&buffer, "STATUS seq=8 total=1 ", 21) == 0);
    assert(line_buffer_take_line(&buffer, line, sizeof(line)) == 0);
    assert(line_buffer_append(&buffer,
                              "standing=1 fallen=0 state=SAFE\n", 31) == 0);
    assert(line_buffer_take_line(&buffer, line, sizeof(line)) == 1);
    assert(strcmp(line,
                  "STATUS seq=8 total=1 standing=1 fallen=0 state=SAFE") == 0);
}

static void test_split_sticky_packets_into_two_lines(void)
{
    LineBuffer buffer = {{0}, 0};
    char line[256];

    assert(line_buffer_append(
               &buffer,
               "STATUS seq=1 total=0 standing=0 fallen=0 state=SAFE\n"
               "STATUS seq=2 total=1 standing=0 fallen=1 state=FALL\n",
               strlen("STATUS seq=1 total=0 standing=0 fallen=0 state=SAFE\n"
                      "STATUS seq=2 total=1 standing=0 fallen=1 state=FALL\n")) == 0);
    assert(line_buffer_take_line(&buffer, line, sizeof(line)) == 1);
    assert(strstr(line, "seq=1") != NULL);
    assert(line_buffer_take_line(&buffer, line, sizeof(line)) == 1);
    assert(strstr(line, "seq=2") != NULL);
}

int main(void)
{
    test_reject_trailing_characters();
    test_reject_negative_and_overflowing_unsigned_fields();
    test_staleness_uses_last_valid_status_time();
    test_parse_valid_fall_line();
    test_reject_inconsistent_counts();
    test_reassemble_split_tcp_message();
    test_split_sticky_packets_into_two_lines();
    puts("test_protocol: all tests passed");
    return 0;
}
