#include "widget.h"

#include <QApplication>

/**
 * Qt Widgets 进程入口。
 * 该函数只负责建立 QApplication、显示根窗口并把控制权交给 Qt 的事件
 * 循环；网络、视频和用户交互均由 Widget 及其槽函数异步处理。
 */
int main(int argc, char *argv[])
{
    // QApplication 管理窗口系统、字体、事件循环等所有 Qt Widgets 资源。
    QApplication a(argc, argv);
    Widget w;
    // RK3566 使用 1024x600 七寸屏，服务端作为现场终端时始终全屏显示。
    w.showFullScreen();
    // 进入事件循环；所有 TCP、按钮和视频帧信号在此循环中分发。
    return a.exec();
}
