# 模型目录

`yolov5n-falldown.kmodel` 是本工程的 K230 部署模型，类别顺序为 `Fall`、`NoFall`。`test-data/falldown_elder.jpg` 用于最小化图片推理验证，`examples` 保存独立 ELF 的启动示例。

本目录不包含训练/转换源资产。替换 KModel 前必须核对输入布局、尺寸、anchors、输出尺度、类别顺序、量化方式和 nncase/K230 SDK 版本，详见 `docs/00-从零开始.md` 与 `docs/06-版本依赖与限制.md`。

