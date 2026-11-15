/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/main_window.cpp
 * 职责：组装地址输入、连接控制、状态文字和视频控件。
 * 设计原因：窗口只响应控制器信号，不直接持有 FFmpeg 资源。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include "main_window.h"
#include "app_config.h"
#include "video_widget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
MainWindow::MainWindow() {
 auto *central=new QWidget(this); auto *layout=new QVBoxLayout(central); auto *bar=new QHBoxLayout; url_=new QLineEdit(kDefaultRtspUrl,central); auto *connectButton=new QPushButton(QStringLiteral("连接"),central); auto *stopButton=new QPushButton(QStringLiteral("停止"),central); auto *reconnectButton=new QPushButton(QStringLiteral("重连"),central); bar->addWidget(url_); bar->addWidget(connectButton); bar->addWidget(stopButton); bar->addWidget(reconnectButton); layout->addLayout(bar); video_=new VideoWidget(central); layout->addWidget(video_,1); status_=new QLabel(QStringLiteral("已停止"),central); stats_=new QLabel(QStringLiteral("-- fps"),central); layout->addWidget(status_); layout->addWidget(stats_); setCentralWidget(central); setWindowTitle(QStringLiteral("K230 实时视频")); resize(1000,700);
 connect(connectButton,&QPushButton::clicked,this,[this]{controller_.start(url_->text());}); connect(stopButton,&QPushButton::clicked,&controller_,&PlayerController::stop); connect(reconnectButton,&QPushButton::clicked,&controller_,&PlayerController::reconnect); connect(&controller_,&PlayerController::stateChanged,this,[this](PlayerState, const QString &m){status_->setText(m);}); connect(&controller_,&PlayerController::frameReady,video_,&VideoWidget::setFrame); connect(&controller_,&PlayerController::statsReady,this,[this](const PlayerStats &s){ stats_->setText(QStringLiteral("%1 fps  %2×%3").arg(s.fps,0,'f',1).arg(s.frameSize.width()).arg(s.frameSize.height())); status_->setText(QStringLiteral("播放中")); });
 controller_.start(kDefaultRtspUrl);
}
QString MainWindow::rtspUrlForTest() const { return url_->text(); }
