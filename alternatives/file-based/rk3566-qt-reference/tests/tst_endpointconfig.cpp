#include <QtTest>
#include "endpointconfig.h"

class EndpointConfigTest : public QObject {
    Q_OBJECT
private slots:
    void acceptsDefaults();
    void rejectsInvalidValues();
};
void EndpointConfigTest::acceptsDefaults() {
    QString error;
    QVERIFY(EndpointConfig::isValid("192.168.10.3", 9000,
                                    "rtsp://192.168.10.2:8554/BackChannelTest", &error));
}
void EndpointConfigTest::rejectsInvalidValues() {
    QString error;
    QVERIFY(!EndpointConfig::isValid("not-an-ip", 9000, "rtsp://host/live", &error));
    QVERIFY(!EndpointConfig::isValid("192.168.10.3", 0, "rtsp://host/live", &error));
    QVERIFY(!EndpointConfig::isValid("192.168.10.3", 9000, "http://host/live", &error));
}
QTEST_APPLESS_MAIN(EndpointConfigTest)
#include "tst_endpointconfig.moc"
