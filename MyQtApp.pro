QT       += core gui charts quick quickwidgets quick3d printsupport network

# Windows 应用图标(劳雷 logo)
RC_ICONS += installer/laurel_logo.ico
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Add C++17 support
CONFIG += c++17

# Add debug information
CONFIG(debug, debug|release) {
    DEFINES += QT_DEBUG
    QMAKE_CXXFLAGS += -g -O0
} else {
    DEFINES += QT_RELEASE
    QMAKE_CXXFLAGS += -O2
}

# Windows specific settings
win32 {
    DEFINES += WIN32_LEAN_AND_MEAN
    LIBS += -L$$QT_HOST_PREFIX/lib
}

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp

INCLUDEPATH += $$PWD/include

# OpenCV 4.11.0 (static, world module with dnn)
INCLUDEPATH += D:/opencv/install/include
LIBS += -LD:/opencv/install/x64/mingw/staticlib -lopencv_world4110
LIBS += -LD:/opencv/build/3rdparty/lib
LIBS += -llibprotobuf -llibwebp -lIlmImf -llibopenjp2 -llibtiff -llibjpeg-turbo -llibpng -lzlib
#HEADERS += \
#    include/MainWindow.h

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    include/MainWindow.h

RESOURCES += \
    resources.qrc