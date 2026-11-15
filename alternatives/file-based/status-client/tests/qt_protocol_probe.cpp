// 将客户端测试发出的 STATUS 行交给用户提供的 Qt 原始解析器验证。
#include <QCoreApplication>
#include <QTextStream>
#include "statusprotocol.h"

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (argc != 2) return 2;
    StatusMessage message;
    QString reason;
    if (!StatusProtocol::parseLine(QByteArray(argv[1]), &message, &reason)) {
        QTextStream(stderr) << reason << '\n';
        return 1;
    }
    QTextStream(stdout) << "accepted seq=" << message.sequence << '\n';
    return 0;
}
