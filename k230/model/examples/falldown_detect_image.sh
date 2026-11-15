# 交付版注释
# 文件：k230/model/examples/falldown_detect_image.sh
# 职责：演示独立 YOLO ELF 的图片或摄像头启动参数。
# 设计原因：保留最短可运行命令，便于先验证模型再接入 RTSP 全链路。
# 修改原则：仅补充说明；运行行为与来源文件保持一致。

set -eu

# 默认按“ELF、模型、图片复制到板端同一目录”的部署方式运行；也允许通过
# 环境变量覆盖路径，方便保留交付包目录结构或自动化部署。
FALLDOWN_ELF="${FALLDOWN_ELF:-./falldown_detect_standalone.elf}"
FALLDOWN_MODEL="${FALLDOWN_MODEL:-./yolov5n-falldown.kmodel}"
FALLDOWN_IMAGE="${FALLDOWN_IMAGE:-./falldown_elder.jpg}"

exec "${FALLDOWN_ELF}" "${FALLDOWN_MODEL}" 0.5 0.45 "${FALLDOWN_IMAGE}" 0
