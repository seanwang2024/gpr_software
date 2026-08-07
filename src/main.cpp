#include <QApplication>
#include "MainWindow.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 若从 cmd/PowerShell 终端启动,附加到父控制台,使 qDebug 诊断输出显示在终端
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
