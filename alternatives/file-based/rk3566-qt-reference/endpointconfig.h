#ifndef ENDPOINTCONFIG_H
#define ENDPOINTCONFIG_H

#include <QString>

/**
 * @brief 校验操作员在界面中输入的网络端点。
 *
 * 校验与实际重启网络服务分离，因此错误输入不会先关闭已经工作的
 * TCP 监听或 RTSP 视频流。
 */
class EndpointConfig {
public:
    /**
     * @brief 检查 IPv4 监听地址、TCP 端口与 RTSP URL。
     * @param ip 需要绑定的 RK3566 IPv4 地址。
     * @param port 监听端口，合法范围为 1 到 65535。
     * @param rtsp K230 视频流地址，必须包含 rtsp 协议和主机地址。
     * @param error 失败时返回可直接显示给操作员的中文原因。
     * @return 三项均合法时返回 true。
     */
    //static 在调用时不需要new一个类的对象
    static bool isValid(const QString &ip, int port, const QString &rtsp, QString *error = nullptr);
};
#endif
