#include "statusprotocol.h"

#include <QRegularExpression>

bool StatusProtocol::parseLine(const QByteArray &line, StatusMessage *message,
                               QString *reason)
{
    // 所有失败出口统一填写原因，调用方据此记录日志并回复 ERR。
    const auto fail = [reason](const QString &text) {
        if (reason) {
            *reason = text;
        }
        return false;
    };

    if (!message) {
        return fail(QStringLiteral("状态输出参数为空"));
    }

    // 必须整行匹配，避免将半包、粘包残片或多余字段误当作有效状态。
    static const QRegularExpression expression(
        QStringLiteral("^STATUS\\s+seq=(\\d+)\\s+total=(\\d+)\\s+"
                       "standing=(\\d+)\\s+fallen=(\\d+)\\s+"
                       "state=(SAFE|FALL)$"));
    const QRegularExpressionMatch match = expression.match(
        QString::fromUtf8(line).trimmed());
    if (!match.hasMatch()) {
        return fail(QStringLiteral("STATUS 格式错误"));
    }

    bool converted = false;
    const quint32 sequence = match.captured(1).toUInt(&converted);
    if (!converted) {
        return fail(QStringLiteral("seq 超出范围"));
    }
    const quint32 total = match.captured(2).toUInt(&converted);
    if (!converted) {
        return fail(QStringLiteral("total 超出范围"));
    }
    const quint32 standing = match.captured(3).toUInt(&converted);
    if (!converted) {
        return fail(QStringLiteral("standing 超出范围"));
    }
    const quint32 fallen = match.captured(4).toUInt(&converted);
    if (!converted) {
        return fail(QStringLiteral("fallen 超出范围"));
    }
    // 先防止无符号加法溢出，再验证总人数与两个分类人数一致。
    if (standing > UINT_MAX - fallen || total != standing + fallen) {
        return fail(QStringLiteral("total 与人数不一致"));
    }

    const QString state = match.captured(5);
    // K230 文本状态与人数必须相互印证，防止错误告警或漏报。
    if ((state == QStringLiteral("SAFE") && fallen != 0)
        || (state == QStringLiteral("FALL") && fallen == 0)) {
        return fail(QStringLiteral("state 与跌倒人数不一致"));
    }

    message->sequence = sequence;
    message->total = total;
    message->standing = standing;
    message->fallen = fallen;
    message->state = state;
    if (reason) {
        reason->clear();
    }
    return true;
}
