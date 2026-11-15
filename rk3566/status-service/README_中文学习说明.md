# K230 与 RK3566：纯 C TCP 网络编程学习包

这个目录把原工程中最重要的状态上报流程提炼为纯 C：K230 是 TCP 客户端，RK3566 是 TCP 服务端。

```text
K230（检测到人数和跌倒结果）
  -> k230_status_tcp_client
  -> TCP/IP 网络
  -> rk3566_status_tcp_server
  -> 在终端显示 SAFE / FALL / UNKNOWN
```

## 先在一台 Linux/WSL 电脑测试

打开第一个终端：

```bash
make
./rk3566_status_tcp_server 9000
```

打开第二个终端：

```bash
./k230_status_tcp_client 127.0.0.1 9000
```

客户端每秒发送一条样例状态；第 6 至第 9 条会是 `FALL`，其余是 `SAFE`。停止客户端，服务端三秒后显示 `UNKNOWN`。

## 必须掌握的函数

| 角色 | 函数 | 一句话说明 |
|---|---|---|
| K230 客户端 | `socket()` | 创建网络文件描述符 |
| K230 客户端 | `connect()` | 主动连接 RK3566 IP 和端口 |
| K230 客户端 | `send()` | 发送 TCP 字节流 |
| RK3566 服务端 | `bind()` | 把端口绑定到本机 |
| RK3566 服务端 | `listen()` | 进入监听状态 |
| RK3566 服务端 | `accept()` | 接收一个 K230 连接 |
| RK3566 服务端 | `recv()` | 接收 TCP 字节流 |
| 两端 | `close()` | 关闭 socket |

## 为什么服务端不能把一次 `recv()` 当作一条消息？

TCP 是**字节流**，没有“报文边界”。例如一条消息：

```text
STATUS seq=8 total=1 standing=1 fallen=0 state=SAFE\n
```

可能分两次 `recv()` 才到，也可能与下一条消息一起到。`LineBuffer` 使用换行符 `\n` 做应用层边界：不完整时继续缓存，完整时取出一行，多条则逐条取出。这就是半包和粘包的处理方式。

## 如何接回原 K230 工程

学习版的 `k230_status_tcp_client.c` 用 `make_demo_status()` 产生样例。实际工程可在这里替换为从 K230 DataFIFO 读取 `DetectionStatus` 的代码。可直接对照原工程中的：

- `tmp/falldown_rtsp_status_safe/little/status_tcp_client/status_tcp_client.cpp`：实际 DataFIFO 读取、`socket`、`connect`、`send_all` 和重连。
- `tmp/falldown_rtsp_status_safe/common/status_protocol.h`：原工程 K230 大核和小核之间的二进制结构体。
- `tmp/falldown_rtsp_status_safe/common/status_text.h`：原工程的 `STATUS` 文本格式。

学习版的 `protocol.h` 只保留网络传输所需要的字段，故意未引入 K230 SDK 头文件；这样你可以在 WSL、RK3566 或普通 Linux 上先把网络部分练熟。

## 编译命令

```bash
make          # 编译客户端和服务端
make test     # 运行协议、半包和粘包的测试
make clean    # 删除编译生成的可执行文件
```
