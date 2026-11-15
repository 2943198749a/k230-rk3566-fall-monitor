# RK3566 智慧跌倒监控台

Qt Widgets 服务端，固定监听 `192.168.10.3:9000`，接收 K230 统计，并使用 FFmpeg 通过 RTSP/TCP 播放：

`rtsp://192.168.10.2:8554/BackChannelTest`

## K230 TCP 协议

K230 每条消息以换行结束：

```text
STATUS seq=7 total=3 standing=2 fallen=1 state=FALL
```

服务端校验 `total = standing + fallen`，且 `SAFE` 只能对应零跌倒、`FALL` 必须对应正跌倒。

- 成功：`ACK seq=7\n`
- 无效：`ERR reason=invalid_status\n`

TCP 是字节流，服务端会缓存半包并逐行处理粘包中的多条消息。

## 构建与运行

```bash
# FFmpeg 是可选的视频后端；未安装时仍可构建并运行监控/TCP 界面，
# 但视频区会提示 RTSP 解码不可用。
# 安装后会在 qmake 阶段自动启用 RTSP/TCP 解码。
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
qmake rk.pro -o build/Makefile
make -C build -j2
./build/rk
```

若运行机器没有 `192.168.10.3` 这个网卡地址，监听会在日志中明确提示失败；请在 RK3566 上配置该地址后运行。

## Qt Creator

在 Qt Creator 中打开 `rk.pro`，选择 Qt 5.15+ Kit 后先点击“运行 qmake”，再构建运行即可。双击 `widget.ui` 会显示完整可编辑的监控台；它现在是运行界面的唯一布局来源，`widget.cpp` 只负责 TCP/RTSP 行为和状态更新。视频能否显示还取决于 RK3566 上 FFmpeg 是否支持 K230 RTSP 流使用的编码。
