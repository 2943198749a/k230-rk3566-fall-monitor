/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/video_widget.h
 * 职责：在 Qt 控件中按比例绘制最新视频帧。
 * 设计原因：绘制层只关心 QImage，避免依赖 FFmpeg 类型并保持界面线程安全。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once
#include <QImage>
#include <QWidget>
class VideoWidget final : public QWidget { public: explicit VideoWidget(QWidget *parent=nullptr); void setFrame(const QImage &frame); protected: void paintEvent(QPaintEvent *event) override; private: QImage frame_; };
