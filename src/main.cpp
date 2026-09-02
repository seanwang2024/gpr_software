#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QScreen>
#include "MainWindow.h"
#include "License.h"
#include "MatIcon.h"
#include "Theme.h"
#include "version.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

// 终端诊断输出:同时写 UTF-8 到控制台(stderr) 和日志文件 dzx_diag.log(便于复制)。
// 供 MainWindow 的 DZX 诊断调用——终端只显示这些内容。
// v1.0.171 Win11 兼容: Win11 24H2 默认终端=Windows Terminal 时 freopen 可能失败,
// 失败后 FILE* 不可再写(否则崩溃), g_consoleOk=false 只落日志文件。
static bool g_consoleOk = false;
void diagPrint(const QString &msg)
{
    QByteArray ba = msg.toUtf8();
    // 控制台
    if (g_consoleOk) {
        fputs(ba.constData(), stderr);
        fputc('\n', stderr);
        fflush(stderr);
    }
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
    // License 自检: 验签通路/篡改检测/凭证往返/机器指纹(通过=0, 失败=2) — 早于控制台初始化, 输出直通管道
    if (qEnvironmentVariableIsSet("GPR_LICENSE_SELFTEST")) {
        const bool ok = License::selfTest();
        fprintf(stderr, "GPR_LICENSE_SELFTEST %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 2;
    }

#ifdef Q_OS_WIN
    // 【诊断版】启动即弹出终端窗口,显示 DZX 处理信息(供与 RADAN 对照)
    bool hasConsole = AllocConsole();
    if (hasConsole) {
        SetConsoleOutputCP(CP_UTF8);              // 控制台按 UTF-8 显示,中文不乱码
        // v1.0.171: freopen 判空防崩(Win11 24H2 Windows Terminal 下可能失败)
        FILE *fo = freopen("CONOUT$", "w", stdout);
        FILE *fe = freopen("CONOUT$", "w", stderr);
        g_consoleOk = (fo != nullptr && fe != nullptr);
        if (!g_consoleOk && fo == nullptr && fe == nullptr)
            hasConsole = false;   // 双双向失败: 纯日志模式
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
    Theme::load();   // v1.0.149 主题(默认岩土橙)须在创建窗口前
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/diting_logo.png")));   // v1.0.148 地听logo

    // v1.0.171 环境自检: Win10/Win11 兼容问题排查必备信息(控制台+日志文件双写)
    {
        QScreen *scr = app.primaryScreen();
        diagPrint(QString("环境: %1 | %2 | 内核 %3 | DPI缩放 %4% | 主屏 %5x%6 | 设备ID %7")
            .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(),
                 QSysInfo::kernelVersion())
            .arg(scr ? qRound(scr->devicePixelRatio() * 100) : 100)
            .arg(scr ? scr->size().width() : 0)
            .arg(scr ? scr->size().height() : 0)
            .arg(License::deviceIdDisplay()));
    }

    // Material Symbols 矢量图标字体(设计稿同款) — 失败仅告警, UI 回退 PNG
    if (MatIcon::init()) {
        if (hasConsole) {
            diagPrint(QString("MatIcon OK: family=%1 mono=%2").arg(MatIcon::family(), MatIcon::monoFamily()));
        }
        // 渲染自检: 输出测试 PNG(验证字体加载/码点渲染, 排查图标不显示问题)
        QPixmap pm = MatIcon::pixmap(QStringLiteral("settings"), QColor("#0048af"), 64);
        pm.save(QCoreApplication::applicationDirPath() + "/maticon_test.png");
    } else if (hasConsole) {
        diagPrint("MatIcon FAIL: 字体未加载, 所有 UI 图标将为空!");
    }

#ifdef Q_OS_WIN
    if (hasConsole) {
        diagPrint("==== 地听AI数据处理 [诊断版 " APP_VERSION "] ====");
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
