#include <QtTest>

#include "statusprotocol.h"

class StatusProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesSafeStatus();
    void rejectsInconsistentStatus_data();
    void rejectsInconsistentStatus();
};

void StatusProtocolTest::parsesSafeStatus()
{
    StatusMessage message;
    QString reason;

    QVERIFY2(StatusProtocol::parseLine(
                  "STATUS seq=7 total=3 standing=3 fallen=0 state=SAFE",
                  &message, &reason),
             qPrintable(reason));
    QCOMPARE(message.sequence, quint32(7));
    QCOMPARE(message.total, quint32(3));
    QCOMPARE(message.standing, quint32(3));
    QCOMPARE(message.fallen, quint32(0));
    QCOMPARE(message.state, QStringLiteral("SAFE"));
}

void StatusProtocolTest::rejectsInconsistentStatus_data()
{
    QTest::addColumn<QByteArray>("line");
    QTest::newRow("wrong total")
        << QByteArray("STATUS seq=1 total=4 standing=3 fallen=0 state=SAFE");
    QTest::newRow("wrong safe state")
        << QByteArray("STATUS seq=1 total=1 standing=0 fallen=1 state=SAFE");
    QTest::newRow("wrong fall state")
        << QByteArray("STATUS seq=1 total=1 standing=1 fallen=0 state=FALL");
    QTest::newRow("missing field")
        << QByteArray("STATUS seq=1 total=1 standing=1 state=SAFE");
}

void StatusProtocolTest::rejectsInconsistentStatus()
{
    QFETCH(QByteArray, line);
    StatusMessage message;
    QString reason;

    QVERIFY(!StatusProtocol::parseLine(line, &message, &reason));
    QVERIFY(!reason.isEmpty());
}

QTEST_APPLESS_MAIN(StatusProtocolTest)

#include "tst_statusprotocol.moc"
