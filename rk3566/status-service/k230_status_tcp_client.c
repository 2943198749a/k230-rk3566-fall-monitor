/*
 * 交付版注释
 * 文件：rk3566/status-service/k230_status_tcp_client.c
 * 职责：可在普通 Linux 运行的 K230 状态模拟客户端。
 * 设计原因：在没有开发板时生成确定性 SAFE/FALL 数据，用于验证协议、断线和服务端状态机。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

/*
 * K230 -> RK3566 的 TCP 状态客户端（纯 C 学习版）。
 *
 * 在真实 K230 工程中，DetectionStatus 来自 DataFIFO；此学习版用
 * make_demo_status() 生成样例，让你能先在普通 Linux/WSL 上跑通网络。
 * 把该函数换成 DataFIFO 读取逻辑，就能接到原工程的检测结果。
 */
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signal_number)
{
    (void)signal_number;
    g_stop = 1;
}

static int connect_server(const char *ip, unsigned short port)
{
    int fd;
    struct sockaddr_in server;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port); /* 主机字节序端口号 -> 网络字节序。 */
    if (inet_pton(AF_INET, ip, &server.sin_addr) != 1) {
        fprintf(stderr, "无效 IPv4 地址: %s\n", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "连接 %s:%u 失败: %s\n", ip, port, strerror(errno));
        close(fd);
        return -1;
    }

    printf("已连接 RK3566：%s:%u\n", ip, port);
    return fd;
}

/* send() 可能只发送一部分字节，所以必须循环至全部内容发送完毕。 */
static int send_all(int fd, const char *data, size_t length)
{
    size_t offset = 0;

    while (offset < length) {
        ssize_t sent = send(fd, data + offset, length - offset, MSG_NOSIGNAL);
        if (sent <= 0)
            return -1;
        offset += (size_t)sent;
    }
    return 0;
}

static DetectionStatus make_demo_status(unsigned int sequence)
{
    DetectionStatus status;

    status.sequence = sequence;
    status.total = 2;
    status.fallen = (sequence % 10 >= 6) ? 1 : 0;
    status.standing = status.total - status.fallen;
    status.is_fall = status.fallen > 0;
    return status;
}

int main(int argc, char *argv[])
{
    const char *server_ip;
    unsigned long parsed_port;
    unsigned int sequence = 1;
    int tcp_fd = -1;

    if (argc != 3) {
        fprintf(stderr, "用法：%s <RK3566_IP> <端口>\n", argv[0]);
        return 1;
    }
    server_ip = argv[1];
    parsed_port = strtoul(argv[2], NULL, 10);
    if (parsed_port == 0 || parsed_port > 65535) {
        fprintf(stderr, "端口必须在 1 到 65535 之间\n");
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    while (!g_stop) {
        DetectionStatus status = make_demo_status(sequence++);
        char message[128];
        int message_length = format_status_line(&status, message, sizeof(message));

        if (message_length < 0) {
            fprintf(stderr, "状态格式化失败\n");
            break;
        }
        if (tcp_fd < 0)
            tcp_fd = connect_server(server_ip, (unsigned short)parsed_port);
        if (tcp_fd >= 0 && send_all(tcp_fd, message, (size_t)message_length) == 0) {
            printf("已发送：%s", message);
            sleep(1);
            continue;
        }

        if (tcp_fd >= 0)
            close(tcp_fd);
        tcp_fd = -1;
        fprintf(stderr, "发送失败，2 秒后重连……\n");
        sleep(2);
    }

    if (tcp_fd >= 0)
        close(tcp_fd);
    return 0;
}
