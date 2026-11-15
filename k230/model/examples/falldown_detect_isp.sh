# 交付版注释
# 文件：k230/model/examples/falldown_detect_isp.sh
# 职责：演示独立 YOLO ELF 的图片或摄像头启动参数。
# 设计原因：保留最短可运行命令，便于先验证模型再接入 RTSP 全链路。
# 修改原则：仅补充说明；运行行为与来源文件保持一致。

set -eu

FALLDOWN_ELF="${FALLDOWN_ELF:-./falldown_detect_standalone.elf}"
FALLDOWN_MODEL="${FALLDOWN_MODEL:-./yolov5n-falldown.kmodel}"

exec "${FALLDOWN_ELF}" "${FALLDOWN_MODEL}" 0.3 0.45 None 0
