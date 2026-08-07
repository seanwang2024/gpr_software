#include <QApplication>
#include "MainWindow.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

// 统一消息处理:强制 UTF-8 输出到 stderr,配合控制台 CP_UTF8 使中文不乱码
static void diagMessageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    QByteArray ba = msg.toUtf8();
    fputs(ba.constData(), stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 【诊断测试版】启动即弹出终端窗口,供测试人员查看 DZX 处理信息并与 RADAN 对照
    bool hasConsole = AllocConsole();
    if (hasConsole) {
        SetConsoleOutputCP(CP_UTF8);              // 控制台按 UTF-8 显示,中文不乱码
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        SetConsoleTitleW(L"DZX Processing Diagnostic");
    }
#endif
    qInstallMessageHandler(diagMessageHandler);   // qDebug 统一走 UTF-8

    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    if (hasConsole) {
        qDebug().noquote() << "==== 劳雷AI数据处理 [诊断测试版] ====";
        qDebug().noquote() << "本终端用于显示 DZX 处理信息。";
        qDebug().noquote() << "请打开带 DZX 的 DZT 文件,下方会打印该文件的处理步骤,";
        qDebug().noquote() << "再与 RADAN 主机的处理设置逐项对照。";
        qDebug().noquote() << "=====================================";
    }
#endif

    MainWindow window;
    window.show();
    return app.exec();
}
