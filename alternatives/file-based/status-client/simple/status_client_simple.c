/*
 * K230 人数和跌倒状态客户端
 *
 * YOLO 程序 -> /tmp/yolo_fall_boxes -> 本程序 -> RK3566 Qt 服务端
 * 视频由另一个 RTSP 服务传输，本程序只发送人数和状态。
 *
 * 编译：gcc -std=c11 -Wall -Wextra status_client_simple.c -o status_client -lm
 * 运行：./status_client
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* 需要改变地址时，修改这里并重新编译。 */
#ifndef SERVER_IP
#define SERVER_IP "192.168.10.3"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT 9000
#endif
#ifndef BOX_FILE
#define BOX_FILE "/tmp/yolo_fall_boxes"
#endif
#define MAX_AGE_SECONDS 2  /* 检测文件超过 2 秒未更新，就不再发送旧结果。 */

typedef struct {
    unsigned int standing;  /* NoFall 检测框数量 */
    unsigned int fallen;    /* Fall 检测框数量 */
} PersonCounts;

static volatile sig_atomic_t running = 1;

static void stop_program(int signal_number)
{
    (void)signal_number;
    running = 0;
}

/* 读取一帧检测结果。读取失败或数据过期时返回 false。 */
static bool read_counts(PersonCounts *counts)
{
    FILE *file = fopen(BOX_FILE, "r");
    if (file == NULL) return false;

    struct stat info;
    time_t now = time(NULL);
    if (fstat(fileno(file), &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size > 65536 || now == (time_t)-1 ||
        info.st_mtime > now + 1 || now - info.st_mtime > MAX_AGE_SECONDS) {
        fclose(file);
        return false;
    }

    counts->standing = 0;
    counts->fallen = 0;
    char line[512];
    bool valid = true;
    while (fgets(line, sizeof(line), file) != NULL) {
        int label, used = 0;
        double score, left, top, right, bottom;
        /* 每一行格式：类别 置信度 左 上 右 下 */
        if (strchr(line, '\n') == NULL ||
            sscanf(line, " %d %lf %lf %lf %lf %lf %n", &label, &score,
                   &left, &top, &right, &bottom, &used) != 6 ||
            line[used] != '\0' || (label != 0 && label != 1) ||
            !isfinite(score) || score < 0 || score > 1 ||
            !isfinite(left) || !isfinite(top) || !isfinite(right) || !isfinite(bottom) ||
            left < 0 || top < 0 || right < left || bottom < top ||
            right > 1920 || bottom > 1080 || counts->standing + counts->fallen >= 10000) {
            valid = false;
            break;
        }
        if (label == 0) counts->fallen++;
        else counts->standing++;
    }
    if (ferror(file)) valid = false;
    fclose(file);
    return valid;
}

/* 与 RK3566 建立 TCP 连接。失败返回 -1。 */
static int connect_server(void)
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;

    struct timeval timeout = {2, 0};
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(socket_fd);
        return -1;
    }
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) != 1 ||
        connect(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

/* 把整条状态发出去，TCP 一次 send 不一定发送全部字节。 */
static bool send_message(int socket_fd, const char *message)
{
    size_t length = strlen(message);
    size_t sent = 0;
    while (sent < length && running) {
        ssize_t n = send(socket_fd, message + sent, length - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return sent == length;
}

/* 接收 Qt 的一行回复，并检查 ACK 序号是否与发送的序号一致。 */
static bool receive_ack(int socket_fd, uint32_t sequence)
{
    char reply[64];
    size_t length = 0;
    while (running && length < sizeof(reply) - 1) {
        char ch;
        if (recv(socket_fd, &ch, 1, 0) != 1) return false;
        if (ch == '\n') {
            reply[length] = '\0';
            char expected[64];
            snprintf(expected, sizeof(expected), "ACK seq=%u", sequence);
            return strcmp(reply, expected) == 0;
        }
        reply[length++] = ch;
    }
    return false;
}

int main(void)
{
    signal(SIGINT, stop_program);   /* 按 Ctrl+C 退出 */
    signal(SIGTERM, stop_program);
    int socket_fd = -1;            /* -1 表示没有连接 */
    uint32_t sequence = 1;         /* 每条消息的序号 */

    printf("连接 RK3566：%s:%d\n", SERVER_IP, SERVER_PORT);
    while (running) {
        PersonCounts counts;
        if (!read_counts(&counts)) {
            /* 检测中断时关闭连接，避免把旧人数当作当前结果。 */
            if (socket_fd >= 0) close(socket_fd);
            socket_fd = -1;
            struct timespec delay = {0, 500000000};
            nanosleep(&delay, NULL);
            continue;
        }

        if (socket_fd < 0) {
            socket_fd = connect_server();
            if (socket_fd < 0) {
                fprintf(stderr, "连接失败，1 秒后重试\n");
                sleep(1);
                continue;
            }
            printf("已连接 RK3566\n");
            /* 连接期间可能等待了几秒，需要重新读取最新结果。 */
            if (!read_counts(&counts)) {
                close(socket_fd);
                socket_fd = -1;
                continue;
            }
        }

        uint32_t current_sequence = sequence++;
        const char *state = counts.fallen > 0 ? "FALL" : "SAFE";
        char message[160];
        snprintf(message, sizeof(message),
                 "STATUS seq=%u total=%u standing=%u fallen=%u state=%s\n",
                 current_sequence, counts.standing + counts.fallen,
                 counts.standing, counts.fallen, state);
        if (!send_message(socket_fd, message) || !receive_ack(socket_fd, current_sequence)) {
            fprintf(stderr, "发送失败或 ACK 错误，准备重连\n");
            close(socket_fd);
            socket_fd = -1;
            sleep(1);
            continue;
        }
        printf("%s", message);
        fflush(stdout);
        struct timespec delay = {0, 500000000};
        nanosleep(&delay, NULL);  /* 每 500 毫秒发送一次 */
    }
    if (socket_fd >= 0) close(socket_fd);
    return 0;
}
