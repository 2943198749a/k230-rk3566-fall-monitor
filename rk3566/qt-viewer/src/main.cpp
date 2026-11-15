/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/src/main.cpp
 * 职责：创建 QApplication 和主窗口。
 * 设计原因：保持进程入口最小，把业务生命周期交给窗口与控制器。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <QApplication>
#include "main_window.h"
int main(int argc,char *argv[]) { QApplication app(argc,argv); MainWindow window; window.show(); return app.exec(); }
