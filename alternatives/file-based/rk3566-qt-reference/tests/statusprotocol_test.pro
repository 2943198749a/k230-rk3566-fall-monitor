QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle

SOURCES += \
    tst_statusprotocol.cpp \
    ../statusprotocol.cpp

HEADERS += \
    ../statusprotocol.h

INCLUDEPATH += ..
