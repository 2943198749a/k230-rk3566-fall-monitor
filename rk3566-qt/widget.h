#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QHostAddress>
#include <QUrl>
#include "statusprotocol.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTcpServer;
class QTcpSocket;
class RtspDecoder;

namespace Ui {
class Widget;
}

/**
 * @brief RK3566 跌倒监控台主窗口。
 *
 * 同时承担三项彼此独立的工作：监听 K230 STATUS 文本协议、显示
 * FFmpeg 解码后的视频帧、向操作员展示人数/告警与通信日志。
 * 视频故障不得中断 TCP 服务；反之 TCP 客户端断开也不得关闭视频。
 */
class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private slots:
    /** @brief 启动 TCP 监听。 */
    void startListening();
    /** @brief 停止 TCP 监听和当前连接。 */
    void stopListening();
    /** @brief 接收新的 K230 TCP 客户端。 */
    void handleNewConnection();
    /** @brief 读取 TCP 数据并按换行拆分 STATUS 帧。 */
    void readClientData();
    /** @brief 处理当前客户端的断开事件。 */
    void handleClientDisconnected();
    /** @brief 重新连接 K230 RTSP 视频流。 */
    void restartVideo();
    /** @brief 在 RTSP 后端报错时显示错误。 */
    void handleMediaError();
    /** @brief 校验并应用顶部输入的监听端点和 RTSP 地址。 */
    void applyConfiguration();

private:
    /** @brief 创建 1024x600 七寸屏使用的全屏紧凑布局及配置输入框。 */
    void buildInterface();
    /** @brief 统一设置工业上位机风格的 Qt 样式表。 */
    void applyTheme();
    /** @brief 在底部日志追加带本地时间的事件，日志最多保留 300 行。 */
    void appendLog(const QString &text);
    /** @brief 更新监听状态文本、颜色以及开始/停止按钮可用性。 */
    void setServerState(const QString &text, bool running);
    /** @brief 更新当前 K230 客户端连接状态。 */
    void setClientState(const QString &text, bool connected);
    /** @brief 解析一帧完整 STATUS 行并更新界面、发送回应。 */
    void processLine(const QByteArray &line);
    void updateDashboard(const StatusMessage &message);
    /** @brief 向当前客户端发送 ACK 或 ERR 协议回应。 */
    void sendReply(const QByteArray &reply);

    /** TCP 监听器由 Widget 所有；关闭窗口时 Qt 父子对象机制自动销毁它。 */
    QTcpServer *m_server;

    /** 当前唯一的 K230 客户端；新连接到达时会替换旧连接。 */
    QTcpSocket *m_clientSocket = nullptr;

    /** 在独立线程解码 RTSP，绝不直接从该线程操作界面控件。 */
    RtspDecoder *m_decoder;

    /** Qt Designer 管理控件树，Widget 管理业务行为。 */
    Ui::Widget *m_ui;

    /** 显示已转换 RGB 视频帧的主画面标签。 */
    QLabel *m_videoLabel;

    /** 已生效的 TCP 绑定地址、端口和下一次 RTSP 重连地址。 */
    QHostAddress m_listenAddress;
    quint16 m_listenPort = 9000;
    QUrl m_rtspUrl;

    /** TCP 收包缓存：只保留尚未出现换行符的半条协议帧。 */
    QByteArray m_receiveBuffer;

    /** 服务、统计与告警区的只读状态标签。 */
    QLabel *m_serverValue;
    QLabel *m_clientValue;
    QLabel *m_updatedValue;
    QLabel *m_totalValue;
    QLabel *m_standingValue;
    QLabel *m_fallenValue;
    QLabel *m_alertBanner;
    QLabel *m_videoHint;

    /** 有上限的运行日志以及控制监听状态的按钮。 */
    QPlainTextEdit *m_logView;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;

    /** 操作员编辑的候选端点；仅在校验成功后写入已生效配置。 */
    QLineEdit *m_ipEdit;
    QLineEdit *m_rtspEdit;
    QSpinBox *m_portEdit;
};
#endif
