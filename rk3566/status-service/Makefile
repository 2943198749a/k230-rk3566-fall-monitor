# 交付版注释
# 文件：rk3566/status-service/Makefile
# 职责：构建协议测试、模拟客户端和 RK3566 服务端。
# 设计原因：只依赖标准 C/POSIX，便于在 WSL、PC 或 RK3566 上快速验证。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic

.PHONY: all test clean

all: k230_status_tcp_client rk3566_status_tcp_server

k230_status_tcp_client: k230_status_tcp_client.c protocol.c protocol.h
	$(CC) $(CFLAGS) -o $@ k230_status_tcp_client.c protocol.c

rk3566_status_tcp_server: rk3566_status_tcp_server.c protocol.c protocol.h
	$(CC) $(CFLAGS) -o $@ rk3566_status_tcp_server.c protocol.c

test: tests/test_protocol
	./tests/test_protocol

tests/test_protocol: tests/test_protocol.c protocol.c protocol.h
	$(CC) $(CFLAGS) -I. -o $@ tests/test_protocol.c protocol.c

clean:
	rm -f k230_status_tcp_client rk3566_status_tcp_server tests/test_protocol
