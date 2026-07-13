QT += core gui widgets network sql

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    filesender.cpp

HEADERS += \
    mainwindow.h \
    filesender.h

FORMS += \
    mainwindow.ui

DISTFILES += \
    .gitignore