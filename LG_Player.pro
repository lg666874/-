#-------------------------------------------------
#
# Project created by QtCreator 2025-11-23T23:28:04
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = LG_Player
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwind.cpp \
    ctrlbar.cpp \
    title.cpp \
    playlistwind.cpp \
    displaywind.cpp \
    ffmsg_queue.cpp \
    ijkmediaplayer.cpp \
    ff_ffplay.cpp \
    ff_ffplay_def.cpp \
    screenshot.cpp \
    sonic.cpp

HEADERS += \
        mainwind.h \
    ctrlbar.h \
    title.h \
    playlistwind.h \
    displaywind.h \
    ffmsg_queue.h \
    ff_ffmsg.h \
    ijkmediaplayer.h \
    ff_ffplay.h \
    ff_ffplay_def.h \
    screenshot.h \
    sonic.h

FORMS += \
        mainwind.ui \
    ctrlbar.ui \
    title.ui \
    playlistwind.ui \
    displaywind.ui

RESOURCES += \
    icon.qrc
win32 {
INCLUDEPATH += $$PWD/ffmpeg-4.2.1-win32-dev/include
INCLUDEPATH += $$PWD/SDL2-2.0.10/include

LIBS += $$PWD/ffmpeg-4.2.1-win32-dev/lib/avformat.lib   \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/avcodec.lib    \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/avdevice.lib   \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/avfilter.lib   \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/avutil.lib     \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/postproc.lib   \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/swresample.lib \
        $$PWD/ffmpeg-4.2.1-win32-dev/lib/swscale.lib \
        $$PWD/SDL2-2.0.10\lib\x86/SDL2.lib \
        "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x86\Ole32.Lib"
}

DISTFILES +=
