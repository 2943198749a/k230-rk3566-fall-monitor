/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/main_window.h
 * 职责：组装地址输入、连接控制、状态文字和视频控件。
 * 设计原因：窗口只响应控制器信号，不直接持有 FFmpeg 资源。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once
#include <QMainWindow>
#include "player_controller.h"
class QLineEdit; class QLabel; class QPushButton; class VideoWidget;
class MainWindow final : public QMainWindow { public: MainWindow(); QString rtspUrlForTest() const; private: QLineEdit *url_; QLabel *status_; QLabel *stats_; VideoWidget *video_; PlayerController controller_; };
