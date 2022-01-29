TEMPLATE = app

TARGET = indicator

CONFIG += console c++11 c++14 c++17
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += $$_PRO_FILE_PWD_/ $$_PRO_FILE_PWD_/../
LIBS += -L$$_PRO_FILE_PWD_/ -L$$_PRO_FILE_PWD_/../libs/ -lapi -ldevice
DEPENDPATH += $$PWD/

SOURCES += \
        cindicator.cpp \
        testing_program.cpp

HEADERS += \
    cindicator.h \
    iindicator.h

DESTDIR = $$_PRO_FILE_PWD_/../
target.path = $$DESTDIR
!isEmpty(target.path): INSTALLS += target
