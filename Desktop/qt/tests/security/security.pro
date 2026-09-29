QT += core gui widgets network sql testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = security_tests
INCLUDEPATH += ../../Server
SOURCES += security_test.cpp \
    ../../Server/security.cpp \
    ../../Server/passwordhash.cpp \
    ../../Server/captchacode.cpp \
    ../../Server/protocol.cpp \
    ../../Server/operatedb.cpp \
    ../../Server/presencestore.cpp \
    ../../Server/msghandler.cpp \
    ../../Server/mytcpsocket.cpp \
    ../../Server/mytcpserver.cpp
HEADERS += ../../Server/server.h \
    ../../Server/operatedb.h \
    ../../Server/mytcpsocket.h \
    ../../Server/mytcpserver.h \
    ../../Server/presencestore.h
