QT += widgets printsupport
QT += core gui widgets

RESOURCES +=

QMAKE_CXXFLAGS += -Wa,-mbig-obj

CONFIG += c++17

INCLUDEPATH += $$PWD
DEPENDPATH  += E:\University\CPP\4032\Project\FFTW_PRJ\FFTW_PRJ

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    qcustomplot.cpp

HEADERS += \
    mainwindow.h \
    qcustomplot.h

DEFINES -= QCUSTOMPLOT_USE_LIBRARY
DEFINES -= QCUSTOMPLOT_COMPILE_LIBRARY

INCLUDEPATH += E:\University\CPP\4032\Project\FFTW_PRJ\FFTW_PRJ
LIBS += -L$$PWD -lfftw3-3

FORMS += \
    mainwindow.ui

RESOURCES +=
