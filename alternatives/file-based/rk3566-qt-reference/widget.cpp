#include "widget.h"

#include "endpointconfig.h"
#include "rtspdecoder.h"
#include "ui_widget.h"

#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

/**
 * 创建主窗口并启动两个彼此独立的服务：
 *
 * - TCP 服务：接收 K230 发来的 STATUS 行协议；
 * - RTSP 解码线程：接收并显示摄像头视频帧。
 *
 * 解码器在工作线程发出图像，因此三个解码器信号均显式使用
 * QueuedConnection，避免在非 GUI 线程直接操作 QLabel。
 */
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , m_server(new QTcpServer(this))
    , m_decoder(new RtspDecoder(this))
    , m_ui(new Ui::Widget)
    , m_listenAddress(QStringLiteral("192.168.10.3"))
    , m_rtspUrl(QStringLiteral("rtsp://192.168.10.2:8554/BackChannelTest"))
{
    buildInterface();
    applyTheme();

    connect(m_server, &QTcpServer::newConnection,
            this, &Widget::handleNewConnection);
    connect(m_decoder, &RtspDecoder::frameReady, this,
            [this](const QImage &frame) {
                const QPixmap pixmap = QPixmap::fromImage(frame).scaled(
                    m_videoLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
                m_videoLabel->setPixmap(pixmap);
            },
            Qt::QueuedConnection);
    connect(m_decoder, &RtspDecoder::streamOpened, this,
            [this] {
                m_videoHint->setText(QStringLiteral("视频流已连接（FFmpeg TCP）"));
            },
            Qt::QueuedConnection);
    connect(m_decoder, &RtspDecoder::errorOccurred, this,
            [this](const QString &error) {
                m_videoHint->setText(QStringLiteral("视频不可用：") + error);
                appendLog(error);
            },
            Qt::QueuedConnection);

    restartVideo();
    startListening();
}

/**
 * Ui::Widget 不继承 QObject，不能由 Qt 父对象机制自动释放，
 * 因而需要在窗口销毁时显式删除。
 */
Widget::~Widget()
{
    delete m_ui;
}

/**
 * 从 widget.ui 创建所有可视控件，并保存运行期频繁访问的控件指针。
 *
 * widget.ui 只描述固定的界面层级和默认外观；网络状态、人数统计及
 * RTSP 帧均由本类在运行期写入。将信号连接集中于此，可使 Designer
 * 文件不包含任何业务逻辑。
 */
void Widget::buildInterface()
{
    m_ui->setupUi(this);
    setMinimumSize(800, 480);

    m_videoLabel = m_ui->videoLabel;
    m_videoHint = m_ui->videoHint;
    m_serverValue = m_ui->serverValue;
    m_clientValue = m_ui->clientValue;
    m_updatedValue = m_ui->updatedValue;
    m_totalValue = m_ui->totalValue;
    m_standingValue = m_ui->standingValue;
    m_fallenValue = m_ui->fallenValue;
    m_alertBanner = m_ui->alertBanner;
    m_logView = m_ui->logView;
    m_ipEdit = m_ui->ipEdit;
    m_portEdit = m_ui->portEdit;
    m_rtspEdit = m_ui->rtspEdit;
    m_startButton = m_ui->startButton;
    m_stopButton = m_ui->stopButton;

    m_ipEdit->setText(m_listenAddress.toString());
    m_portEdit->setValue(m_listenPort);
    m_rtspEdit->setText(m_rtspUrl.toString());
    m_logView->setMaximumBlockCount(300);

    connect(m_startButton, &QPushButton::clicked,
            this, &Widget::startListening);
    connect(m_stopButton, &QPushButton::clicked,
            this, &Widget::stopListening);
    connect(m_ui->retryButton, &QPushButton::clicked,
            this, &Widget::restartVideo);
    connect(m_ui->applyButton, &QPushButton::clicked,
            this, &Widget::applyConfiguration);

    setServerState(QStringLiteral("未监听"), false);
    setClientState(QStringLiteral("未连接"), false);
}

/**
 * 应用窗口级 QSS。告警标签会在正常/告警状态间改写 objectName，
 * 因此 updateDashboard() 之后会再次调用本函数，让 Qt 重新匹配
 * QLabel#safe 或 QLabel#fall 选择器。
 */
void Widget::applyTheme()
{
    setStyleSheet(R"(
        QWidget {
            background: #ededed;
            color: #202020;
            font-family: "Noto Sans CJK SC", "Microsoft YaHei";
            font-size: 12px;
        }
        QLabel#titleLabel {
            color: #1b365d;
            font-size: 16px;
            font-weight: 600;
            padding: 2px 8px 2px 0;
        }
        QLabel#ipLabel, QLabel#portLabel, QLabel#rtspLabel,
        QLabel#serverLabel, QLabel#clientLabel, QLabel#updatedLabel,
        QLabel#totalLabel, QLabel#standingLabel, QLabel#fallenLabel {
            color: #3b3b3b;
        }
        QGroupBox {
            background: #f8f8f8;
            border: 1px solid #b8b8b8;
            border-radius: 2px;
            font-weight: 600;
            margin-top: 8px;
            padding: 8px 6px 6px 6px;
        }
        QGroupBox::title {
            color: #17365d;
            left: 7px;
            padding: 0 3px;
            subcontrol-origin: margin;
        }
        QLabel#serverValue, QLabel#clientValue, QLabel#updatedValue,
        QLabel#totalValue, QLabel#standingValue, QLabel#fallenValue {
            background: #ffffff;
            border: 1px solid #c9c9c9;
            color: #202020;
            min-height: 18px;
            padding: 1px 4px;
        }
        QLabel#videoHint {
            background: #ffffff;
            border: 1px solid #c9c9c9;
            color: #555;
            padding: 3px;
        }
        QLabel#alertBanner {
            color: #555;
            padding: 3px;
        }
        QLabel#safe {
            color: #16743a;
            font-weight: 600;
            padding: 3px;
        }
        QLabel#fall {
            background: #fff2f2;
            border: 1px solid #e3aaaa;
            color: #b00020;
            font-weight: 700;
            padding: 3px;
        }
        QPlainTextEdit {
            background: #fff;
            border: 1px solid #bcbcbc;
            color: #202020;
            font-family: monospace;
            font-size: 11px;
        }
        QLineEdit, QSpinBox {
            background: #fff;
            border: 1px solid #a9a9a9;
            min-height: 22px;
            padding: 1px;
        }
        QPushButton {
            background: #f6f6f6;
            border: 1px solid #9f9f9f;
            border-radius: 2px;
            min-width: 62px;
            padding: 4px 8px;
        }
        QPushButton:hover { background: #e0eaf5; }
        QPushButton:pressed { background: #ccdced; }
        QPushButton:disabled {
            background: #ececec;
            border-color: #c5c5c5;
            color: #999;
        }
    )");
}

/**
 * 使用“已生效”而非编辑框中的地址开始监听。
 *
 * 编辑框的内容只是候选配置；只有 applyConfiguration() 校验成功后
 * 才会写入 m_listenAddress 和 m_listenPort，避免用户输入到一半就
 * 影响正在运行的 TCP 服务。
 */
void Widget::startListening()
{
    if (m_server->isListening()) {
        return;
    }

    if (!m_server->listen(m_listenAddress, m_listenPort)) {
        const QString error = m_server->errorString();
        setServerState(QStringLiteral("监听失败：%1").arg(error), false);
        appendLog(QStringLiteral("监听失败：%1").arg(error));
        return;
    }

    const QString endpoint = QStringLiteral("%1:%2")
                                 .arg(m_listenAddress.toString())
                                 .arg(m_listenPort);
    setServerState(QStringLiteral("监听中  %1").arg(endpoint), true);
    appendLog(QStringLiteral("TCP 服务已监听 %1").arg(endpoint));
}

/**
 * 停止监听并主动断开当前唯一的数据源。
 *
 * 协议设计只接受一个 K230 客户端；断开操作是异步的，最终的状态清理
 * 仍由 handleClientDisconnected() 在 disconnected 信号中完成。
 */
void Widget::stopListening()
{
    if (m_clientSocket) {
        m_clientSocket->disconnectFromHost();
    }
    m_server->close();
    setServerState(QStringLiteral("已停止"), false);
    appendLog(QStringLiteral("TCP 服务已停止"));
}

/**
 * 接受新 TCP 客户端。
 *
 * K230 是单数据源设备，所以新的连接优先：存在旧连接时先请求旧连接
 * 断开，再将新 socket 设为当前 socket。readClientData() 会再次比较
 * sender()，从而避免旧 socket 迟到的数据覆盖新连接状态。
 */
void Widget::handleNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }

    if (m_clientSocket) {
        appendLog(QStringLiteral("新客户端接入，关闭旧客户端"));
        m_clientSocket->disconnectFromHost();
    }

    m_clientSocket = socket;
    m_receiveBuffer.clear();
    connect(socket, &QTcpSocket::readyRead, this, &Widget::readClientData);
    connect(socket, &QTcpSocket::disconnected,
            this, &Widget::handleClientDisconnected);

    const QString endpoint = QStringLiteral("%1:%2")
                                 .arg(socket->peerAddress().toString())
                                 .arg(socket->peerPort());
    setClientState(QStringLiteral("已连接  ") + endpoint, true);
    appendLog(QStringLiteral("K230 客户端已连接：") + endpoint);
}

/**
 * 从 TCP 字节流中还原以换行结尾的 STATUS 消息。
 *
 * TCP 没有消息边界：一次 readyRead() 可能只有半条消息，也可能包含
 * 多条消息。因此先把 readAll() 追加到 m_receiveBuffer，循环取出每
 * 个完整的 '\n' 前内容；最后不足一行的尾部保留到下次回调。
 */
void Widget::readClientData()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || socket != m_clientSocket) {
        return;
    }

    m_receiveBuffer += socket->readAll();
    int separator = 0;
    while ((separator = m_receiveBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_receiveBuffer.left(separator).trimmed();
        m_receiveBuffer.remove(0, separator + 1);
        if (!line.isEmpty()) {
            processLine(line);
        }
    }
}

/**
 * 校验一条完整的 STATUS 数据并作出协议回应。
 *
 * StatusProtocol 负责字段、人数守恒和 SAFE/FALL 语义校验；本窗口
 * 只负责将有效值投射到界面。无效行不影响后续行解析，并回复 ERR，
 * 有效行则回复包含序号的 ACK，供 K230 确认传输结果。
 */
void Widget::processLine(const QByteArray &line)
{
    StatusMessage message;
    QString reason;
    if (!StatusProtocol::parseLine(line, &message, &reason)) {
        appendLog(QStringLiteral("收到非法 STATUS：%1（%2）")
                      .arg(QString::fromUtf8(line), reason));
        sendReply("ERR reason=invalid_status\n");
        return;
    }

    updateDashboard(message);
    sendReply(QStringLiteral("ACK seq=%1\n").arg(message.sequence).toUtf8());
    appendLog(QStringLiteral("STATUS #%1：总数 %2，未跌倒 %3，跌倒 %4")
                  .arg(message.sequence)
                  .arg(message.total)
                  .arg(message.standing)
                  .arg(message.fallen));
}

/**
 * 向当前连接发送 ACK/ERR。
 *
 * 写入前检查连接状态，避免客户端刚好断开时对已关闭 socket 写数据。
 * QTcpSocket::write() 是异步的，数据将由 Qt 事件循环实际发送。
 */
void Widget::sendReply(const QByteArray &reply)
{
    if (m_clientSocket
        && m_clientSocket->state() == QAbstractSocket::ConnectedState) {
        m_clientSocket->write(reply);
    }
}

/**
 * 将已校验的协议数据显示在仪表盘。
 *
 * fallen 大于零时切换为红色告警，否则显示绿色安全状态。时间使用本机
 * 接收时间，而不是设备时间，便于操作员判断最后一次成功通信的时刻。
 */
void Widget::updateDashboard(const StatusMessage &message)
{
    m_totalValue->setText(QString::number(message.total));
    m_standingValue->setText(QString::number(message.standing));
    m_fallenValue->setText(QString::number(message.fallen));
    m_updatedValue->setText(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    if (message.fallen) {
        m_alertBanner->setObjectName(QStringLiteral("fall"));
        m_alertBanner->setText(
            QStringLiteral("● 告警：检测到 %1 人跌倒").arg(message.fallen));
    } else {
        m_alertBanner->setObjectName(QStringLiteral("safe"));
        m_alertBanner->setText(QStringLiteral("● 正常：未检测到跌倒人员"));
    }
    applyTheme();
}

/**
 * 处理 socket 断开。
 *
 * 被新连接替代的旧 socket 也会发出 disconnected。只有当前 socket
 * 可以清空界面连接状态；旧 socket 仅延迟释放，避免误将新连接显示为
 * “未连接”。
 */
void Widget::handleClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket != m_clientSocket) {
        if (socket) {
            socket->deleteLater();
        }
        return;
    }

    appendLog(QStringLiteral("K230 客户端已断开"));
    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;
    m_receiveBuffer.clear();
    setClientState(QStringLiteral("未连接"), false);
}

/**
 * 按当前 RTSP 地址重启解码线程。
 *
 * stop() 先请求中断并等待旧线程退出，保证 setUrl() 和 start() 不会
 * 与仍在运行的 FFmpeg 读取逻辑并发访问 URL。
 */
void Widget::restartVideo()
{
    m_videoHint->setText(
        QStringLiteral("连接视频流（FFmpeg TCP）：") + m_rtspUrl.toString());
    m_decoder->stop();
    m_decoder->setUrl(m_rtspUrl);
    m_decoder->start();
    appendLog(QStringLiteral("请求 FFmpeg RTSP/TCP：") + m_rtspUrl.toString());
}

/**
 * 保留该槽位作为媒体错误扩展点。
 *
 * 当前错误由构造函数中连接的 lambda 直接显示并写日志；保留此函数
 * 可以兼容后续改为命名槽函数的实现。
 */
void Widget::handleMediaError()
{
}

/**
 * 原子化地应用操作员输入的监听端点和 RTSP URL。
 *
 * 所有输入先通过 EndpointConfig 校验。若 TCP 原来处于监听状态，则
 * 暂停监听、更新配置后再恢复；这能避免同时打开两个地址或端口。视频
 * 解码器同样先停止，以免旧 URL 的工作线程与新 URL 混用。
 */
void Widget::applyConfiguration()
{
    const QString ip = m_ipEdit->text().trimmed();
    const int port = m_portEdit->value();
    const QString rtsp = m_rtspEdit->text().trimmed();
    QString error;

    if (!EndpointConfig::isValid(ip, port, rtsp, &error)) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), error);
        return;
    }

    const bool wasListening = m_server->isListening();
    if (wasListening) {
        stopListening();
    }
    m_decoder->stop();

    m_listenAddress = QHostAddress(ip);
    m_listenPort = static_cast<quint16>(port);
    m_rtspUrl = QUrl(rtsp);
    appendLog(QStringLiteral("已应用配置：%1:%2，%3").arg(ip).arg(port).arg(rtsp));

    if (wasListening) {
        startListening();
    }
    restartVideo();
}

/**
 * 在通信日志中追加一条带本地时间的事件。
 *
 * 日志控件的最大块数已在 buildInterface() 设为 300，长期运行时旧
 * 记录会自动被丢弃，避免日志无限增长占用内存。
 */
void Widget::appendLog(const QString &text)
{
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, text));
}

/**
 * 同步服务端状态文本、颜色和互斥按钮。
 *
 * 监听成功使用绿色，并禁用“开始监听”；停止或失败使用红色，并启用
 * “开始监听”。这样用户无法重复调用 listen()。
 */
void Widget::setServerState(const QString &text, bool running)
{
    m_serverValue->setText(text);
    m_serverValue->setStyleSheet(
        running ? QStringLiteral("color:#16743a;font-weight:600;")
                : QStringLiteral("color:#b00020;font-weight:600;"));
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
}

/**
 * 更新当前 K230 客户端的连接状态。
 *
 * TCP 服务器和客户端状态独立：即使没有 K230 连接，监听服务也可以
 * 继续正常运行，因此此函数不改变开始/停止监听按钮。
 */
void Widget::setClientState(const QString &text, bool connected)
{
    m_clientValue->setText(text);
    m_clientValue->setStyleSheet(
        connected ? QStringLiteral("color:#16743a;font-weight:600;")
                  : QStringLiteral("color:#555;"));
}
