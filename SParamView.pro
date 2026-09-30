QT += widgets concurrent
CONFIG += release c++20
win32:CONFIG -= entrypoint
TEMPLATE = app
TARGET = SParamView
INCLUDEPATH += include src
SOURCES += \
    src/core.cpp \
    src/workers.cpp \
    src/main.cpp \
    src/window.cpp \
    src/plot.cpp \
    src/report.cpp \
    src/theme.cpp \
    src/navigation.cpp
RESOURCES += assets/SParamView.qrc
unix:QMAKE_RPATHDIR += $$[QT_INSTALL_LIBS]
win32 {
    RC_FILE = assets/SParamView.rc
}
