#-------------------------------------------------
#
# Project created by QtCreator 2014-05-11T19:13:52
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = os3
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++0x

SOURCES += main.cpp \
    fs.cpp

HEADERS += \
    fs.h
