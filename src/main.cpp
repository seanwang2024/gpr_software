#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include "MainWindow.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

// 终端诊断输出:同时写 UTF-8 到控制台(stderr) 和日志文件 dzx_diag.log(便于复制)。
// 供 MainWindow 的 DZX 诊断调用——终端只显示这些内容。
void diagPrint(const QString &msg)
{
    QByteArray ba = msg.toUtf8();
    // 控制台
    fputs(ba.constData(), stderr);
    fputc('\n', stderr);
    fflush(stderr);
    // 日志文件(每次运行覆盖,位于 exe 同级目录)
    static QFile logFile;
    static bool inited = false;
    if (!inited) {
        inited = true;
        logFile.setFileName(QCoreApplication::applicationDirPath() + "/dzx_diag.log");
        logFile.open(QIODevice::WriteOnly | QIODevice::Text);  // 覆盖写,本次运行内追加
    }
    if (logFile.isOpen()) {
        logFile.write(ba);
        logFile.write("\n");
        logFile.flush();
    }
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
        // 启用快速编辑(QuickEdit):鼠标可选中终端文字,选中后按 回车/右键 复制
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            mode |= ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hIn, mode);
        }
    }
#endif
    qInstallMessageHandler(silentMessageHandler); // 屏蔽其他 qDebug

    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    if (hasConsole) {
        diagPrint("==== 劳雷AI数据处理 [诊断测试版] ====");
        diagPrint("本终端只显示 DZX 处理信息。");
        diagPrint("复制方法:①终端内鼠标选中文字 -> 按 回车 或 右键 复制;");
        diagPrint("          ②或直接用记事本打开下面的日志文件,Ctrl+C/Ctrl+A 正常复制:");
        diagPrint("          " + QCoreApplication::applicationDirPath() + "/dzx_diag.log");
        diagPrint("请打开带 DZX 的 DZT 文件,下方会打印该文件的处理步骤,再与 RADAN 逐项对照。");
        diagPrint("=====================================");
    }
#endif

    MainWindow window;
    window.show();
    return app.exec();
}
