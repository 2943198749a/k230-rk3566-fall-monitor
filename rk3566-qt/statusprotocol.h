#ifndef STATUSPROTOCOL_H
#define STATUSPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/**
 * @brief 一条由 K230 发送的跌倒识别状态。
 */
struct StatusMessage
{
    /** K230 单调递增的消息序号，用于 ACK 对应与现场排障。 */
    quint32 sequence = 0;
    /** 识别区域内的总人数，必须等于 standing + fallen。 */
    quint32 total = 0;
    /** 当前未跌倒人数。 */
    quint32 standing = 0;
    /** 当前跌倒人数；非零时界面进入告警状态。 */
    quint32 fallen = 0;
    /** K230 给出的摘要状态，只允许 SAFE 或 FALL。 */
    QString state;
};

/**
 * @brief K230 STATUS 文本协议的无状态解析工具。
 */
class StatusProtocol
{
public:
    /**
     * @brief 解析并校验一条不带换行符的 STATUS 消息。
     * @param line 从 TCP 缓冲区提取出的完整 UTF-8 行。
     * @param message 成功时接收已验证的状态数据，不能为 nullptr。
     * @param reason 失败时接收中文错误原因，可为 nullptr。
     * @return 格式及人数/state 一致时返回 true，否则返回 false。
     */
    static bool parseLine(const QByteArray &line, StatusMessage *message,
                          QString *reason = nullptr);
};

#endif // STATUSPROTOCOL_H
