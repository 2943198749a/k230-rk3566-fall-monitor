/*
 * 交付版注释
 * 文件：k230/little-core/status-client/status_tcp_client.cpp
 * 职责：读取状态 DataFIFO，校验记录并通过可重连 TCP 发送到 RK3566。
 * 设计原因：状态传输独立于视频 RTSP；RK3566 断线只触发重连，不能阻塞推理或视频。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "k_datafifo.h"
#include "status_protocol.h"
#include "status_text.h"

namespace {
// Must match the big core's FIFO cache-line size.  DetectionStatus occupies
// the first 64 bytes; the rest of each 1024-byte slot is zero padding.
constexpr k_s32 kStatusBlockLength = 1024;
k_datafifo_handle g_fifo = (k_datafifo_handle)K_DATAFIFO_INVALID_HANDLE;
volatile sig_atomic_t g_stop = 0;

void on_signal(int) { g_stop = 1; }

bool send_all(int fd, const std::string &message)
{
    // TCP send() 允许只发送一部分字节。循环直到整条、以换行结尾的 STATUS
    // 写完，RK3566 的行缓冲才能把它识别为一个完整应用层消息。
    size_t offset = 0;
    while (offset < message.size())
    {
        const ssize_t sent = send(fd, message.data() + offset,
                                  message.size() - offset, MSG_NOSIGNAL);
        if (sent <= 0)
            return false;
        offset += static_cast<size_t>(sent);
    }
    return true;
}

int connect_server(const char *ip, unsigned short port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        return -1;
    }
    sockaddr_in server = {};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server.sin_addr) != 1) {
        fprintf(stderr, "invalid RK3566 IPv4 address: %s\n", ip);
        close(fd);
        return -1;
    }
    if (connect(fd, reinterpret_cast<sockaddr *>(&server), sizeof(server)) < 0) {
        fprintf(stderr, "connect to RK3566 %s:%u failed: %s\n",
                ip, port, strerror(errno));
        close(fd);
        return -1;
    }
    printf("connected to RK3566 %s:%u\n", ip, port);
    return fd;
}
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <status_fifo_phy_hex> <rk3566_ip> <port>\n", argv[0]);
        return 1;
    }

    unsigned long long phy = 0;
    unsigned int port = 0;
    if (sscanf(argv[1], "%llx", &phy) != 1 ||
        sscanf(argv[3], "%u", &port) != 1 || port == 0 || port > 65535)
    {
        fprintf(stderr, "invalid FIFO physical address or TCP port\n");
        return 1;
    }

    k_datafifo_params_s params = {32, kStatusBlockLength, K_TRUE, DATAFIFO_READER};
    if (kd_datafifo_open_by_addr(&g_fifo, &params, static_cast<k_u64>(phy)) != K_SUCCESS)
    {
        fprintf(stderr, "cannot open status DataFIFO at 0x%llx\n", phy);
        return 1;
    }
    printf("status FIFO opened: 0x%llx, item_size=%d\n", phy, kStatusBlockLength);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    int tcp_fd = -1;
    unsigned int empty_polls = 0;
    while (!g_stop)
    {
        k_u32 available = 0;
        const k_s32 cmd_ret = kd_datafifo_cmd(
            g_fifo, DATAFIFO_CMD_GET_AVAIL_READ_LEN, &available);
        if (cmd_ret != K_SUCCESS) {
            fprintf(stderr, "get status FIFO readable length failed: 0x%x\n", cmd_ret);
            break;
        }
        if (available == 0)
        {
            if (++empty_polls % 100 == 0)
                printf("waiting for status FIFO data...\n");
            usleep(10000);
            continue;
        }
        empty_polls = 0;

        void *raw = nullptr;
        const k_s32 read_ret = kd_datafifo_read(g_fifo, &raw);
        if (read_ret != K_SUCCESS || raw == nullptr) {
            fprintf(stderr, "read status FIFO failed: 0x%x\n", read_ret);
            continue;
        }
        // raw 的所有权仍属于 DataFIFO。先复制到本地，再 READ_DONE；确认之后
        // 大核即可复用共享槽位，因此后续校验和网络发送只能使用本地副本。
        DetectionStatus status{};
        memcpy(&status, raw, sizeof(status));
        const k_s32 done_ret = kd_datafifo_cmd(g_fifo, DATAFIFO_CMD_READ_DONE, raw);
        if (done_ret != K_SUCCESS) {
            fprintf(stderr, "complete status FIFO read failed: 0x%x\n", done_ret);
            continue;
        }
        if (!is_valid_detection_status(status)) {
            fprintf(stderr,
                    "invalid status: magic=0x%x version=%u seq=%u total=%u standing=%u fallen=%u\n",
                    status.magic, status.version, status.sequence, status.total,
                    status.standing, status.fallen);
            continue;
        }

        if (status.sequence == 1 || status.sequence % 30 == 0)
            printf("received status: seq=%u total=%u standing=%u fallen=%u\n",
                   status.sequence, status.total, status.standing, status.fallen);

        if (tcp_fd < 0)
            tcp_fd = connect_server(argv[2], static_cast<unsigned short>(port));
        if (tcp_fd < 0 || !send_all(tcp_fd, format_status_line(status)))
        {
            if (tcp_fd >= 0)
                close(tcp_fd);
            tcp_fd = -1;
            usleep(1000000);
        }
    }
    if (tcp_fd >= 0)
        close(tcp_fd);
    kd_datafifo_close(g_fifo);
    return 0;
}
