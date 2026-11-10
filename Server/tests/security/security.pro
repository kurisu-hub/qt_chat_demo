QT += core gui widgets network sql testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = security_tests
INCLUDEPATH += ../..
SOURCES += security_test.cpp \
    ../../security.cpp \
    ../../passwordhash.cpp \
    ../../captchacode.cpp \
    ../../protocol.cpp \
    ../../operatedb.cpp \
    ../../presencestore.cpp \
    ../../msghandler.cpp \
    ../../mytcpsocket.cpp \
    ../../mytcpserver.cpp
HEADERS += ../../server.h \
    ../../operatedb.h \
    ../../mytcpsocket.h \
    ../../mytcpserver.h \
    ../../presencestore.h
