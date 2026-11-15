/*
 * K230 Linux 跌倒状态客户端（C，POSIX TCP）
 *
 * 此程序只发送人数和状态，不负责传输视频。视频由独立的 RTSP 服务提供，
 * RK3566 Qt 程序作为 RTSP 客户端拉流；本程序作为 TCP 客户端连接 Qt 9000。
 *
 * 生产者 falldown_publish.elf 使用临时文件 + rename 原子发布检测快照：
 *     label score x1 y1 x2 y2
 * 其中 label=0 是 Fall，label=1 是 NoFall；空文件表示这一帧没有目标。
 * 检测框数量不是去重后的人数，结果没有经过跌倒状态去抖或安全认证。
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* 一份新鲜检测快照：字段是当前帧的检测框数量，不是累加计数。 */
typedef struct DetectionCounts {
    unsigned int standing_count; /* 模型 NoFall 框数：协议 standing */
    unsigned int fallen_count;   /* 模型 Fall 框数：协议 fallen */
} DetectionCounts;

/* 配置对象保存启动时输入的参数；不会隐含指定未知 RK3566 地址。 */
typedef struct ClientConfig {
    struct sockaddr_in server_address; /* RK3566 IPv4 地址和 TCP 端口 */
    const char *snapshot_path;         /* YOLO 原子发布的快照文件路径 */
    long send_interval_ms;             /* 两次状态发送的间隔 */
    long snapshot_max_age_ms;          /* 快照允许的最大年龄 */
} ClientConfig;

/* 信号处理函数只更新标志；正常循环负责关闭 socket 等资源。 */
static volatile sig_atomic_t stop_requested = 0;
static void handle_stop_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

/* 将用户输入转为有范围限制的整数；-1 表示输入无效。 */
static long parse_integer(const char *text, long minimum, long maximum)
{
    char *end_pointer = NULL;
    errno = 0;
    long parsed_value = strtol(text, &end_pointer, 10);
    if (errno || end_pointer == text || *end_pointer != '\0' ||
        parsed_value < minimum || parsed_value > maximum) return -1;
    return parsed_value;
}

/* 不中断退出响应的等待；重连时使用，避免连接失败后无限高速重试。 */
static void sleep_ms(long milliseconds)
{
    struct timespec remaining = { milliseconds / 1000, (milliseconds % 1000) * 1000000L };
    while (!stop_requested && nanosleep(&remaining, &remaining) < 0 && errno == EINTR) {}
}

/* 返回单调时间，专用于网络超时；不受设备系统时间调整影响。 */
static long long monotonic_ms(void)
{
    struct timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) != 0) return -1;
    return (long long)current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
}

/*
 * 读取一份 YOLO 快照。open 后的文件描述符指向同一份已发布文件，
 * 即使 YOLO 同时 rename 新快照，本次读取也不会读到一半新、一半旧。
 * 成功时 result 指向的 DetectionCounts 对象被填充；失败时绝不假装零人。
 */
static bool read_detection_snapshot(const ClientConfig *config, DetectionCounts *result)
{
    int file_descriptor = open(config->snapshot_path, O_RDONLY | O_CLOEXEC);
    if (file_descriptor < 0) return false;

    struct stat file_info;       /* 文件的大小、修改时间等元数据 */
    struct timespec wall_time;   /* 与文件 mtime 相同的系统时钟 */
    if (fstat(file_descriptor, &file_info) != 0 || !S_ISREG(file_info.st_mode) ||
        file_info.st_size > 65536 || clock_gettime(CLOCK_REALTIME, &wall_time) != 0) {
        close(file_descriptor);
        return false;
    }
    long long age_ms = (long long)(wall_time.tv_sec - file_info.st_mtim.tv_sec) * 1000 +
                       (wall_time.tv_nsec - file_info.st_mtim.tv_nsec) / 1000000;
    /* 允许 1 秒时钟误差，未来过多或过期的快照都视为不可用。 */
    if (age_ms < -1000 || age_ms > config->snapshot_max_age_ms) {
        close(file_descriptor);
        return false;
    }

    FILE *snapshot_file = fdopen(file_descriptor, "r");
    if (!snapshot_file) { close(file_descriptor); return false; }
    char text_line[512];                   /* 当前正在解析的一行文本 */
    DetectionCounts counts = {0, 0};       /* 当前帧的临时统计对象 */
    bool valid = true;
    while (fgets(text_line, sizeof(text_line), snapshot_file)) {
        int label = -1;                    /* 检测类别：0 跌倒，1 未跌倒 */
        int parsed_characters = 0;         /* sscanf 消耗的位置，检查多余字段 */
        double confidence, left, top, right, bottom;
        if (!strchr(text_line, '\n') ||
            sscanf(text_line, " %d %lf %lf %lf %lf %lf %n", &label, &confidence,
                   &left, &top, &right, &bottom, &parsed_characters) != 6) {
            valid = false; break;
        }
        for (const char *cursor = text_line + parsed_characters; *cursor; ++cursor)
            if (!isspace((unsigned char)*cursor)) valid = false;
        if (!valid || (label != 0 && label != 1) || !isfinite(confidence) ||
            confidence < 0 || confidence > 1 || !isfinite(left) || !isfinite(top) ||
            !isfinite(right) || !isfinite(bottom) || left < 0 || top < 0 ||
            right < left || bottom < top || right > 1920 || bottom > 1080 ||
            counts.standing_count + counts.fallen_count >= 10000) {
            valid = false; break;
        }
        if (label == 0) ++counts.fallen_count;
        else ++counts.standing_count;
    }
    if (ferror(snapshot_file)) valid = false;
    fclose(snapshot_file); /* 同时关闭由 fdopen 接管的文件描述符 */
    if (!valid) return false;
    *result = counts;      /* 空文件会得到合法的 0 人快照 */
    return true;
}

/* 创建 TCP 连接：非阻塞 connect + poll，最多等待 2 秒。 */
static int connect_server(const ClientConfig *config)
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;
    int original_flags = fcntl(socket_fd, F_GETFL, 0);
    if (original_flags < 0 || fcntl(socket_fd, F_SETFL, original_flags | O_NONBLOCK) < 0) {
        close(socket_fd); return -1;
    }
    int result = connect(socket_fd, (const struct sockaddr *)&config->server_address,
                         sizeof(config->server_address));
    if (result < 0 && errno == EINPROGRESS) {
        struct pollfd wait_socket = { socket_fd, POLLOUT, 0 };
        result = poll(&wait_socket, 1, 2000);
        int socket_error = 0;
        socklen_t error_length = sizeof(socket_error);
        if (result <= 0 || getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                                      &socket_error, &error_length) < 0 || socket_error != 0) {
            close(socket_fd); return -1;
        }
    } else if (result < 0) {
        close(socket_fd); return -1;
    }
    if (fcntl(socket_fd, F_SETFL, original_flags) < 0) { close(socket_fd); return -1; }
    /* 阻塞 send 的最长等待时间，避免网络异常让客户端永久卡住。 */
    struct timeval send_timeout = {2, 0};
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0) {
        close(socket_fd); return -1;
    }
    return socket_fd;
}

/* TCP 可能一次只发送部分字节，循环发送直到整行发送完毕。 */
static bool send_all(int socket_fd, const char *text, size_t text_length)
{
    size_t sent_length = 0;
    while (sent_length < text_length && !stop_requested) {
        ssize_t sent_now = send(socket_fd, text + sent_length, text_length - sent_length, MSG_NOSIGNAL);
        if (sent_now < 0 && errno == EINTR) continue;
        if (sent_now <= 0) return false;
        sent_length += (size_t)sent_now;
    }
    return sent_length == text_length;
}

/* 按字节接收一行 ACK。TCP 可能拆包，必须等到换行；限制长度和总时间。 */
static bool receive_matching_ack(int socket_fd, uint32_t expected_sequence)
{
    char reply_line[128];
    size_t used = 0;
    long long started_ms = monotonic_ms();
    if (started_ms < 0) return false;
    while (!stop_requested && used + 1 < sizeof(reply_line)) {
        long long elapsed = monotonic_ms() - started_ms;
        if (elapsed < 0 || elapsed >= 2000) return false;
        struct pollfd wait_socket = { socket_fd, POLLIN, 0 };
        int ready = poll(&wait_socket, 1, (int)(2000 - elapsed));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) return false;
        char next_character;
        ssize_t received = recv(socket_fd, &next_character, 1, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received != 1) return false;
        if (next_character == '\n') {
            reply_line[used] = '\0';
            char expected_line[64];
            snprintf(expected_line, sizeof(expected_line), "ACK seq=%u", expected_sequence);
            return strcmp(reply_line, expected_line) == 0;
        }
        reply_line[used++] = next_character;
    }
    return false;
}

int main(int argc, char **argv)
{
    if (argc > 6) {
        fprintf(stderr, "Usage: %s [RK3566_IPV4=192.168.10.3] [PORT=9000] [BOXES=/tmp/yolo_fall_boxes] [INTERVAL_MS=500] [MAX_AGE_MS=1500]\n", argv[0]);
        return 2;
    }
    /* 用户未输入参数时使用两块板当前约定的固定地址，也允许以后手动覆盖。 */
    const char *server_ip = argc > 1 ? argv[1] : "192.168.10.3";
    long port = argc > 2 ? parse_integer(argv[2], 1, 65535) : 9000;
    long interval = argc > 4 ? parse_integer(argv[4], 50, 60000) : 500;
    long max_age = argc > 5 ? parse_integer(argv[5], 100, 60000) : 1500;
    ClientConfig config = {0};
    config.server_address.sin_family = AF_INET;
    if (port < 0 || interval < 0 || max_age < interval ||
        inet_pton(AF_INET, server_ip, &config.server_address.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP, port, interval, or maximum age\n"); return 2;
    }
    config.server_address.sin_port = htons((unsigned short)port);
    config.snapshot_path = argc > 3 ? argv[3] : "/tmp/yolo_fall_boxes";
    config.send_interval_ms = interval;
    config.snapshot_max_age_ms = max_age;
    struct sigaction action = {0}; action.sa_handler = handle_stop_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL); sigaction(SIGTERM, &action, NULL);

    int socket_fd = -1;              /* -1 表示当前未连接 Qt 服务端 */
    uint32_t sequence = 1;           /* 本次进程中的 STATUS 消息序号 */
    bool waiting_for_snapshot = false; /* 避免每 500 毫秒重复输出同一条提示 */
    fprintf(stderr, "目标 RK3566: %s:%ld；检测文件: %s\n", server_ip, port, config.snapshot_path);
    while (!stop_requested) {
        DetectionCounts counts;
        if (!read_detection_snapshot(&config, &counts)) {
            /* 中断连接而不是发送 SAFE；注意 Qt 当前版本仍需单独清除旧显示。 */
            if (socket_fd >= 0) { close(socket_fd); socket_fd = -1; }
            if (!waiting_for_snapshot) {
                fprintf(stderr, "等待新鲜的 YOLO 检测数据（文件缺失、过期或格式错误）\n");
                waiting_for_snapshot = true;
            }
            sleep_ms(config.send_interval_ms);
            continue;
        }
        waiting_for_snapshot = false;
        if (socket_fd < 0) {
            socket_fd = connect_server(&config);
            if (socket_fd < 0) { fprintf(stderr, "connect failed; retrying\n"); sleep_ms(1000); continue; }
            fprintf(stderr, "connected to Qt status server\n");
            /* connect 期间快照可能变旧，重新检查后才能发送。 */
            if (!read_detection_snapshot(&config, &counts)) { close(socket_fd); socket_fd = -1; continue; }
        }
        unsigned int total_count = counts.standing_count + counts.fallen_count;
        const char *state_text = counts.fallen_count ? "FALL" : "SAFE";
        char status_line[160];
        uint32_t sent_sequence = sequence++;
        int line_length = snprintf(status_line, sizeof(status_line),
            "STATUS seq=%u total=%u standing=%u fallen=%u state=%s\n",
            sent_sequence, total_count, counts.standing_count, counts.fallen_count, state_text);
        if (line_length <= 0 || (size_t)line_length >= sizeof(status_line) ||
            !send_all(socket_fd, status_line, (size_t)line_length) ||
            !receive_matching_ack(socket_fd, sent_sequence)) {
            fprintf(stderr, "send/ACK failed; reconnecting\n");
            close(socket_fd); socket_fd = -1; sleep_ms(1000); continue;
        }
        printf("%s", status_line); fflush(stdout);
        sleep_ms(config.send_interval_ms);
    }
    if (socket_fd >= 0) close(socket_fd);
    return 0;
}
