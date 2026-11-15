# 交付包验证报告

验证日期：2026-11-02（Asia/Shanghai）

## 独立最终审查

全新上下文的只读审阅者复验了目录、ZIP、81 条内部哈希、DataFIFO 布局和关键所有权顺序，未发现 Critical。它提出的 Important 已在最终包中处理：状态过期改按最后合法消息计时；数字解析拒绝符号与溢出；Qt 手动重连先停止旧线程；模型脚本改用真实交付文件名；预编译二进制的历史路径/RPATH 改为明确披露而非错误宣称不存在。

## 已通过

### 交付结构、注释和文档

- `scripts/verify-package.ps1`：通过。
- 必需入口、KModel、四个板端运行文件和七类核心文档均存在。
- 55 个源码/构建文件均包含“交付版注释”文件头。
- `docs/02-逐文件详解.md` 覆盖全部源码文件。
- 源码、脚本、清单和说明文档未发现私人绝对路径；原样保留的预编译二进制属于下述明确例外。
- `source-map.csv` 共 67 个原始文件映射，目标无重复，来源使用逻辑根目录。

### 源码保持性

对来源与交付副本的 C/C++ 文件去除注释和空白后比较：36 个未改逻辑文件的可执行 token 一致；9 个功能修复/测试文件明确排除并在变更说明中逐项记录：

- K230 RTSP `main.cpp`：线程函数显式返回；
- RK3566 `protocol.c/.h`、状态服务端和协议测试：严格数字解析与按最后合法状态计时；
- Qt `player_controller.cpp/.h`、`rtsp_decoder.cpp` 和控制器测试：安全停止/手动重连生命周期。

CMake 和 Shell 的目录/SDK 参数调整见 `docs/07-整理版变更说明.md`。

### RK3566 纯 C 状态服务

在 WSL Ubuntu 执行：

```bash
make clean && make && make test
```

结果：三个目标均在 `-std=c11 -Wall -Wextra -Werror -pedantic` 下编译；`test_protocol: all tests passed`。

红绿验证：

1. 原解析器拒绝合法 FALL 行；GDB 显示 `sscanf fields=5`，但代码要求 6。
2. 新增尾随内容测试后，旧代码错误接受并触发断言。
3. 新增负号、正号、超过 `unsigned int` 范围和最后合法状态超时测试；旧代码先因缺少严格接口/行为失败。
4. 改用纯十进制 token + `strtoul` 范围检查，并按单调时钟记录最后合法状态后，全部协议测试通过。
5. 集成测试先发送合法 SAFE，再每 0.6 秒持续发送无换行非法字节；服务端仍在 3 秒后输出 UNKNOWN，证明非法可读事件不会刷新状态寿命。

### K230 大核交叉编译

SDK：历史验证路径对应的 `k230-v3-work/k230_sdk`，使用其中 musl RISC-V 工具链和 AI POC toolchain file。

结果：`falldown_rtsp_big.elf` 构建成功。

```text
ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI,
statically linked, stripped
SHA-256 5909749fed1befa036f3ef3c7e96cc53070471f76e5c483f31323296f1666a7f
```

编译器产生既有 SDK/工具代码警告，包括 nncase 多字符常量、`build_schedule()` 返回值未消费、若干数值窄化和 `void*` 算术。它们没有阻止构建，但已列为后续清理项；本次交付没有在缺少板端回归的情况下批量改动媒体/AI 代码。

### K230 小核 RTSP 交叉编译

使用 SDK 内 Xuantie glibc RISC-V 工具链和匹配 SDK 库构建。首次编译发现 `read_send()` 缺少返回值；使用 `-Werror=return-type` 确认失败，补 `return nullptr` 后同一严格构建通过。

```text
ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI,
dynamically linked, GNU/Linux 4.15.0, stripped
SHA-256 b0128eeb56c1b3da2723ef38f03190a9b248bb1ec44d4113e3665e8bd1810497
```

### K230 小核状态客户端交叉编译

结果：`k230_status_tcp_client` 构建成功。

```text
ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI,
dynamically linked, GNU/Linux 4.15.0
SHA-256 3a2517a00e984372f72e2f028b4d5fae23744991dcd1acf3b08492964d87c398
```

### 脚本与缺失 SDK 诊断

- 三个 Shell 示例通过 `bash -n`。
- 不提供 K230 SDK 参数时，大核、RTSP 和状态客户端 CMake 均按预期在配置阶段给出明确的 `K230_SDK_ROOT/K230_SDK_ASSET_ROOT` 错误，而不是继续生成错误构建。
- Qt `PlayerController` 和相应测试已使用 Qt 5.15 头文件执行 `g++ -fsyntax-only`，语法检查通过。

## 未通过/未执行

### RK3566 Qt/FFmpeg 构建

WSL 有 Qt 5.15.18，但缺少 `libavformat/libavcodec/libavutil/libswscale` 开发包，CMake 在 `pkg_check_modules()` 阶段按预期停止。因此本机没有链接 Qt 应用，也没有执行 Qt 四组运行时测试；仅完成上述控制器语法检查。安装 `libav*-dev` 后应按 `docs/03-编译部署运行.md` 重新构建和运行 `ctest`。

### 板端运行验收

本次没有连接真实 K230/RK3566 板卡，以下项目仍需按 `docs/05-故障排查与验收.md` 执行：摄像头/VICP、KModel/NPU、H.264 硬件编码、两个 DataFIFO、RTSP 两分钟播放、SAFE→FALL→SAFE、断线 UNKNOWN 和双通道故障隔离。

## 预编译文件说明

`k230/bin` 保留工作区中已有、用于板端复验的运行文件；上面列出的“新交叉编译 SHA-256”是源码可构建性的证据，不自动替换已有板端基线。重新部署新构建物前仍需真实板端回归。

逐字节扫描显示，大核基线 ELF 和独立推理 ELF 包含历史源码/SDK 绝对路径；预编译状态客户端还含历史 SDK 库目录的 DT_RPATH。由于本次目标是保留板端基线，没有直接改写 ELF。公开发布前应通过重新构建消除路径并再次板端验证。

