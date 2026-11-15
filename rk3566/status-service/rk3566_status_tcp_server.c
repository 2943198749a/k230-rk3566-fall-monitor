/*
 * 交付版注释
 * 文件：rk3566/status-service/rk3566_status_tcp_server.c
 * 职责：RK3566 TCP 服务端：接收并校验 K230 状态，维护 SAFE/FALL/UNKNOWN。
 * 设计原因：RK3566 作为固定地址服务端便于多次重连；超时转 UNKNOWN 避免把陈旧 SAFE 当作当前状态。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

/*
 * RK3566 TCP 状态服务端（纯 C 学习版）。
 * 接收 K230 发来的 STATUS 文本，演示 TCP 服务端、半包/粘包和超时处理。
 */
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signal_number)
{
    (void)signal_number;
    g_stop = 1;
}

static int create_listener(unsigned short port)
{
    int fd;
    int enabled = 1;
    struct sockaddr_in address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    /* 允许服务重启后立即重新绑定同一端口。 */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY); /* 监听所有网卡。 */
    address.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 4) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static void print_status(const DetectionStatus *status)
{
    printf("K230 状态：seq=%u，总人数=%u，站立=%u，跌倒=%u，状态=%s\n",
           status->sequence, status->total, status->standing, status->fallen,
           status->is_fall ? "FALL（有人跌倒）" : "SAFE（无人跌倒）");
}

static uint64_t monotonic_millis(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static int process_received_bytes(LineBuffer *buffer, const char *data,
                                  size_t length, int *has_current_status)
{
    char line[256];
    int take_result;
    int saw_valid_status = 0;

    if (line_buffer_append(buffer, data, length) < 0) {
        fprintf(stderr, "接收缓存溢出，丢弃本次连接\n");
        buffer->length = 0;
        return 0;
    }

    /* 一次 recv() 可能没有一条完整消息，也可能包含很多条消息。 */
    while ((take_result = line_buffer_take_line(buffer, line, sizeof(line))) == 1) {
        DetectionStatus status;

        if (parse_status_line(line, &status) == 0) {
            print_status(&status);
            *has_current_status = 1;
            saw_valid_status = 1;
        } else {
            fprintf(stderr, "丢弃非法消息：%s\n", line);
        }
    }
    if (take_result < 0) {
        fprintf(stderr, "单条消息过长，清空接收缓存\n");
        buffer->length = 0;
    }
    return saw_valid_status;
}

int main(int argc, char *argv[])
{
    unsigned long parsed_port;
    int listener_fd;
    int client_fd = -1;
    LineBuffer buffer = {{0}, 0};
    int has_current_status = 0;
    uint64_t last_valid_ms = 0;

    if (argc != 2) {
        fprintf(stderr, "用法：%s <监听端口>\n", argv[0]);
        return 1;
    }
    parsed_port = strtoul(argv[1], NULL, 10);
    if (parsed_port == 0 || parsed_port > 65535) {
        fprintf(stderr, "端口必须在 1 到 65535 之间\n");
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    listener_fd = create_listener((unsigned short)parsed_port);
    if (listener_fd < 0)
        return 1;
    printf("RK3566 正在监听 TCP 端口 %lu，按 Ctrl+C 退出。\n", parsed_port);

    while (!g_stop) {
        fd_set read_set;
        struct timeval timeout;
        int max_fd = listener_fd;
        int ready;

        const uint64_t now_ms = monotonic_millis();
        if (status_is_stale(has_current_status, last_valid_ms, now_ms, 3000u)) {
            puts("K230 状态：UNKNOWN（连续 3 秒未收到合法状态）");
            has_current_status = 0;
        }

        FD_ZERO(&read_set);
        FD_SET(listener_fd, &read_set);
        if (client_fd >= 0) {
            FD_SET(client_fd, &read_set);
            if (client_fd > max_fd)
                max_fd = client_fd;
        }
        /*
         * 有当前状态时只等待到它的剩余寿命；非法行或半包导致 socket 可读，
         * 下一轮仍会用 last_valid_ms 重新计算，不能替旧 SAFE/FALL 续期。
         */
        if (has_current_status && now_ms >= last_valid_ms) {
            uint64_t elapsed = now_ms - last_valid_ms;
            uint64_t remaining = elapsed >= 3000u ? 0u : 3000u - elapsed;
            timeout.tv_sec = (time_t)(remaining / 1000u);
            timeout.tv_usec = (suseconds_t)((remaining % 1000u) * 1000u);
        } else {
            timeout.tv_sec = 3;
            timeout.tv_usec = 0;
        }
        ready = select(max_fd + 1, &read_set, NULL, NULL, &timeout);

        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        if (ready == 0) {
            continue;
        }

        if (FD_ISSET(listener_fd, &read_set)) {
            struct sockaddr_in peer;
            socklen_t peer_length = sizeof(peer);
            int new_client = accept(listener_fd, (struct sockaddr *)&peer,
                                    &peer_length);
            if (new_client >= 0) {
                char peer_ip[INET_ADDRSTRLEN];
                if (client_fd >= 0)
                    close(client_fd); /* 一次只接受一块 K230，新的替换旧的。 */
                client_fd = new_client;
                buffer.length = 0;
                inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
                printf("K230 已连接：%s:%u\n", peer_ip, ntohs(peer.sin_port));
            }
        }

        if (client_fd >= 0 && FD_ISSET(client_fd, &read_set)) {
            char received[512];
            ssize_t received_length = recv(client_fd, received, sizeof(received), 0);

            if (received_length > 0) {
                if (process_received_bytes(&buffer, received,
                                           (size_t)received_length,
                                           &has_current_status))
                    last_valid_ms = monotonic_millis();
            } else {
                puts("K230 已断开连接");
                close(client_fd);
                client_fd = -1;
                buffer.length = 0;
            }
        }
    }

    if (client_fd >= 0)
        close(client_fd);
    close(listener_fd);
    return 0;
}
