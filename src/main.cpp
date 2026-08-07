#include <QApplication>
#include "MainWindow.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

// 终端诊断输出:直接写 UTF-8 到 stderr(控制台),绕过 Qt 消息系统。
// 供 MainWindow 的 DZX 诊断调用——终端只显示这些内容。
void diagPrint(const QString &msg)
{
    QByteArray ba = msg.toUtf8();
    fputs(ba.constData(), stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

// 消息处理:抑制所有 qDebug/qWarning,避免其他调试信息刷屏(终端只留 DZX 诊断)
static void silentMessageHandler(QtMsgType, const QMessageLogContext &, const QString &) {}

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
    qInstallMessageHandler(silentMessageHandler); // 屏蔽其他 qDebug

    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    if (hasConsole) {
        diagPrint("==== 劳雷AI数据处理 [诊断测试版] ====");
        diagPrint("本终端只显示 DZX 处理信息。");
        diagPrint("请打开带 DZX 的 DZT 文件,下方会打印该文件的处理步骤,再与 RADAN 逐项对照。");
        diagPrint("=====================================");
    }
#endif

    MainWindow window;
    window.show();
    return app.exec();
}
