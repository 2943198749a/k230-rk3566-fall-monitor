# K230 Linux：YOLO 跌倒检测 + 1080p RTSP 复刻包

本包根据当前实际运行版本整理。摄像头从两个 ISP 通道分别供 YOLO 推理与视频编码使用；推理结果通过临时文件传给 RTSP 进程，叠加到视频后由硬件 H.264 编码，提供 8556/yolo。源码中的“中文复刻注释”只解释数据流，不改变代码行为。

## 1. 你将得到什么

- OV5647 原厂 ISP 配置 + YOLOv5n 跌倒检测（Fall / NoFall）。
- 1080p、H.264、5 Mbps、四区域 OSD，地址 `rtsp://192.168.10.2:8556/yolo`。
- 镜像 SDK 对应 commit：`02d4630a639247146ec3660d4ae9f83f017913c6`；CanMV-K230 V3P0 Linux v1.4，nncase 2.11.0。
- 现成可执行文件、对应模型、注释源码、编译脚本和板上前台运行脚本。软件开发工具及完整 SDK 未打包。

目录：

```text
source/ai_demo/falldown_publish/  推理和结果发布源码
source/ai_demo/common/            官方共享推理、图像与摄像头工具代码
source/rtsp/                      采集、OSD、硬件编码和 RTSP 代码
source/librtsp_server/            官方 Live555 RTSP 服务库源码（用于阅读）
runtime/                          板上可执行文件、模型、示例图片、启动脚本
scripts/build.sh                  在配套 SDK 环境交叉编译
scripts/S32ov5647                 OV5647 初始化脚本（不自动安装）
docs/工作原理与排错.md            调用链、设计取舍及排错
SHA256SUMS.txt                    包内文件 SHA-256
```

## 2. 依赖和版本

1. K230 板：CanMV-K230 LP4 V3.0，OV5647；当前镜像 `CanMV-K230_V3P0_linux_v1.4_nncase_v2.11.0_02d4630a_sdcard.img.gz`。镜像须包含板端 nncase、OpenCV、FFmpeg、V4L2、nonai2d 和 RTSP 等运行库。
2. 电脑：Ubuntu 22.04 / 24.04 或 WSL2 + Ubuntu 22.04 Docker。该机实际在 Ubuntu 22.04 容器中构建；Ubuntu 26.04/GCC 15 曾导致主机依赖构建失败。路径不能包含空格。
3. SDK：`https://github.com/kendryte/k230_linux_sdk`，检出 commit `02d4630a639247146ec3660d4ae9f83f017913c6`；已完成 `k230_canmv_v3_defconfig` 构建、生成 `output/k230_canmv_v3_defconfig/staging`，其中须有与板子匹配的头文件、OpenCV、FFmpeg、nncase、librtsp_server 和 Live555 库。
4. 交叉工具链：`Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2`，默认 `/opt/toolchain/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2`。
5. 以太网直连示例：Windows 电脑 `192.168.10.1/24`、K230 `192.168.10.2/24`。不要与局域网已有地址冲突。

官方文档：https://www.kendryte.com/k230_linux/zh/main/app_develop_guide/ai_develop/aidemo_doc.html

## 3. 先使用现成程序

### 3.1 把 runtime 目录传到 K230

在 Windows PowerShell 中进入解压后的本包目录，然后执行（按实际路径替换）：

```powershell
cd C:\你的解压目录\k230-linux-rtsp-yolo-reproduction
ssh root@192.168.10.2 'mkdir -p /root/k230-yolo-repro'
scp -O runtime/* root@192.168.10.2:/root/k230-yolo-repro/
```

首次 SSH 按实际环境核对主机身份；需要密码时输入板子的密码。也可通过其他方式复制 `runtime` 目录内容。

### 3.2 确认 OV5647 和地址

在 K230 的 Linux 串口（本机曾为 COM6）中：

```sh
ip -4 addr show eth0
cat /proc/vsi/isp_subdev0
ls -l /dev/video1 /dev/video2 /dev/video3
```

应确认 eth0 是 `192.168.10.2/24`、sensor 是 `ov5647`。临时设置示例（不要重复添加已存在的地址）：

```sh
ip addr add 192.168.10.2/24 dev eth0
```

如果传感器显示 `gc2093`，本包 `scripts/S32ov5647` 记录了之前板上验证的 OV5647 初始化步骤。先检查脚本路径与自己的镜像匹配，再以 root 执行其中的 `start` 操作；本包不会自动修改你的开机配置。当前板上的同名启动脚本已验证开机后选择 OV5647。原厂 auto JSON 的 AE setPoint=30，之前试验的 32 已撤回。

### 3.3 启动

先确保旧的摄像头推流／推理程序已经停止，8556、`/dev/video2` 和 `/dev/video3` 没有被占用；不要同时启动两套本程序。当前环境可能已在运行，因此不应直接重复启动。

```sh
cd /root/k230-yolo-repro
chmod +x falldown_publish.elf camera_rtsp_yolo_1080 run_foreground.sh
./run_foreground.sh
```

在 VLC 的“媒体 → 打开网络串流”中输入：

```text
rtsp://192.168.10.2:8556/yolo
```

用 Ctrl+C 请求停止。官方 RTSP 示例的退出有时会等待线程；如果未退出，先排查，不要反复强制终止 ISP 进程。也可参考 `docs/工作原理与排错.md` 中分终端启动方式。

## 4. 从源码复刻编译

以下命令在 Ubuntu/WSL 的 Linux 终端或 Ubuntu 22.04 Docker 容器中执行，**不是在 K230 上执行**。

```bash
# 将解压包复制到 Linux 文件系统，以下示例路径按自己实际目录修改
cp -a /mnt/c/你的解压目录/k230-linux-rtsp-yolo-reproduction ~/k230-yolo-repro
cd ~/k230-yolo-repro
export SDK_ROOT="$HOME/k230_linux_sdk_02d4630a"
export TOOLCHAIN_ROOT=/opt/toolchain/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2
sh scripts/build.sh
ls -lh build/bin
```

若使用容器，需同时挂载包、SDK 和 `/opt/toolchain`；容器内 `SDK_ROOT` 路径应与挂载路径一致。脚本先编译 YOLO 程序，再交叉编译 RTSP 程序，不覆盖 SDK 源码和原产物。`source/librtsp_server` 是对应 SDK 的服务库源码，供阅读；脚本链接 SDK staging 中已构建的库，不会重编该库。生成的文件：

```text
build/bin/falldown_publish.elf
build/bin/camera_rtsp_yolo_1080
```

把这两个文件与 `runtime/yolov5n-falldown.kmodel` 和 `runtime/run_foreground.sh` 放在板上同一目录，再按第 3 节运行。二进制逐字节哈希可能因编译路径和编译时间变化；应以架构、运行及功能验证为准。

## 5. 验证与已知限制

- 用 `sha256sum -c SHA256SUMS.txt` 检查包内文件；校验清单不包括自身。
- 该版本在原板和镜像上已验证 VLC 可解码 1080p，用户确认画面和检测框正常；用户估计延迟约 1–3 秒，未做精确的端到端测量或长期稳定性测试。
- 部分日志有 `capture POLLERR`；不能仅凭此判断每次都失败，应结合实际画面和进程状态检查。
- `/tmp/yolo_fall_boxes` 仅供本机绘框，格式为 `label score x1 y1 x2 y2`，没有消息确认、追踪、状态去抖或持久化；旧结果超过 3 秒不显示。它不是可靠的报警系统。
- 模型 `0=Fall`、`1=NoFall`；检测框及输出不代表经过临床或安全认证的跌倒报警。模型效果受光照、姿态和拍摄方向影响。
- 本包不含 RK3566 状态上报，不自动启动 RTSP，不提供开机自愈；原厂无叠框流是独立程序，并非本包 8556 服务。
- 摄像头朝向要让 **未旋转的原始视频**正常；单独在 VLC 旋转只改变显示，不会旋转 YOLO 输入。
- `scripts/S32ov5647` 和 `runtime/run_foreground.sh` 是辅助脚本；其中后者是为复刻包新写的，尚未在板上做完整启动／退出验证。
