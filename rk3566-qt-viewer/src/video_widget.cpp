/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/video_widget.cpp
 * 职责：在 Qt 控件中按比例绘制最新视频帧。
 * 设计原因：绘制层只关心 QImage，避免依赖 FFmpeg 类型并保持界面线程安全。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "video_widget.h"
#include <QPainter>
VideoWidget::VideoWidget(QWidget *parent): QWidget(parent) { setMinimumSize(640, 360); }
void VideoWidget::setFrame(const QImage &frame) { frame_ = frame; update(); }
void VideoWidget::paintEvent(QPaintEvent *) { QPainter p(this); p.fillRect(rect(), Qt::black); if (!frame_.isNull()) { QSize s=frame_.size(); s.scale(size(), Qt::KeepAspectRatio); p.drawImage(QRect((width()-s.width())/2,(height()-s.height())/2,s.width(),s.height()),frame_); } }
