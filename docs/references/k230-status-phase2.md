# K230 Status TCP Phase 2 Runbook

## Prerequisite

Phase 1 must already show normal H.264 RTSP video for two minutes. Keep that
video pipeline running while this procedure tests status reporting. The status
client has a separate DataFIFO and must never be pointed at video address
`1628c000`.

## Build the separate Linux client on WSL

```sh
SDK=/home/user/work/k230_sdk
APP=/home/user/work/k230-rk3566-yolo-falldown/k230
export PATH="$SDK/toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin:$PATH"

TOOL="$SDK/toolchain/Xuantie-900-gcc-linux-5.10.4-glibc-x86_64-V2.6.0/bin"
cmake -S "$APP/little/status_tcp_client" -B /tmp/falldown-rtsp-tcp-statusclient-v3 \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_CXX_COMPILER="$TOOL/riscv64-unknown-linux-gnu-g++" \
  -DK230_SDK_ASSET_ROOT="$SDK"
cmake --build /tmp/falldown-rtsp-tcp-statusclient-v3 --target k230_status_tcp_client -j2
file /tmp/falldown-rtsp-tcp-statusclient-v3/k230_status_tcp_client
sha256sum /tmp/falldown-rtsp-tcp-statusclient-v3/k230_status_tcp_client
```

The existing `k230_status_tcp_client` in
`/sharefs/app/falldown_rtsp_tcp/` is the paired Linux client.  It must be
started only after the safe big-core ELF has printed `STATUS_FIFO_PHY_ADDR`.

## RK3566 TCP receiver

Use a line-oriented TCP receiver on `192.168.10.3:9000`; it must print every
received newline-terminated record. The expected exact schema is:

```text
STATUS seq=<n> total=<n> standing=<n> fallen=<n> state=<SAFE|FALL>
```

## K230 COM6 status client

Copy the value after `STATUS_FIFO_PHY_ADDR=` from the experimental COM7 log.
It is hexadecimal and is different from `1628c000`.

```sh
cd /sharefs/app/falldown_rtsp_tcp
./k230_status_tcp_client <STATUS_FIFO_PHY_ADDR without 0x> 192.168.10.3 9000
```

The client may show connection retries while the RK3566 service is stopped;
that must not stop the COM7 encoder or normal RTSP image.

## Validation matrix

| Check | Expected result | Result |
| --- | --- | --- |
| Valid record, no fall | `state=SAFE` | Pending board run |
| Fall record | `state=FALL` | Pending board run |
| Later no-fall record | returns to `state=SAFE` | Pending board run |
| Stop RK3566 service | status client retries; RTSP remains normal | Pending board run |
| Restart RK3566 service | status client reconnects and resumes lines; RTSP remains normal | Pending board run |

## Stop and rollback

Stop only `k230_status_tcp_client_v3` and the temporary RK3566 TCP receiver.
The Phase 1 video process remains independent. If the experimental video needs
rollback, use the known-good `/sharefs/app/falldown_rtsp/` commands documented
in `k230-h264-phase1.md`.
