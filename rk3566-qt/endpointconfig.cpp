//解析IP地址，端口号 URL是否正确，不正确返回false 正确返回true他


#include "endpointconfig.h"
#include <QHostAddress>
#include <QUrl>

bool EndpointConfig::isValid(const QString &ip, int port, const QString &rtsp, QString *error) {
    // QHostAddress 同时会接受 IPv6；本项目协议固定为 IPv4，因此显式限制协议族。
    QHostAddress address;
    if (!address.setAddress(ip) || address.protocol() != QAbstractSocket::IPv4Protocol) {
        if (error) *error = QStringLiteral("监听 IP 必须是有效 IPv4 地址");
        return false;
    }
    // quint16 的 0 常用于“系统自动分配端口”，这里不允许，避免和 K230 固定协议冲突。
    if (port < 1 || port > 65535) { if (error) *error = QStringLiteral("端口范围为 1 至 65535");
        return false; }
    // 只验证 URL 的协议和主机；具体连通性由 FFmpeg 解码线程在后台报告。
    const QUrl url(rtsp);
    if (!url.isValid() || url.scheme().toLower() != QStringLiteral("rtsp") || url.host().isEmpty()) {
        if (error) *error = QStringLiteral("RTSP 地址必须以 rtsp:// 开头");
        return false;
    }
    if (error) error->clear();
    return true;
}
