/*
 * 交付版注释
 * 文件：rk3566/qt-viewer/tests/test_retry_policy.cpp
 * 职责：验证重连次数与退避上限。
 * 设计原因：防止错误路径形成忙循环或等待时间失控。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <QtTest>

#include "retry_policy.h"

class RetryPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void stopsAfterConfiguredMaximum();
    void usesBoundedBackoffDelays();
};

void RetryPolicyTest::stopsAfterConfiguredMaximum()
{
    RetryPolicy policy;

    QVERIFY(policy.mayRetry(1));
    QVERIFY(policy.mayRetry(3));
    QVERIFY(!policy.mayRetry(4));
}

void RetryPolicyTest::usesBoundedBackoffDelays()
{
    RetryPolicy policy;

    QCOMPARE(policy.delayMsForAttempt(1), 500);
    QCOMPARE(policy.delayMsForAttempt(2), 1000);
    QCOMPARE(policy.delayMsForAttempt(3), 2000);
}

QTEST_MAIN(RetryPolicyTest)
#include "test_retry_policy.moc"
