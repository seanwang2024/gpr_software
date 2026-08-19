#include "MatIcon.h"

#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QTextStream>

namespace {
bool s_ready = false;
QString s_family;
QString s_monoFamily;
QHash<QString, quint32> s_codepoints;
} // namespace

namespace MatIcon {

bool init()
{
    if (s_ready)
        return true;

    QFontDatabase db;

    int id = db.addApplicationFont(QStringLiteral(":/fonts/MaterialSymbolsOutlined.ttf"));
    if (id < 0) {
        qWarning("MatIcon: MaterialSymbolsOutlined.ttf 加载失败, UI 将回退 PNG 图标");
        return false;
    }
    s_family = QFontDatabase::applicationFontFamilies(id).value(0);

    id = db.addApplicationFont(QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"));
    if (id < 0) {
        qWarning("MatIcon: JetBrainsMono-Regular.ttf 加载失败, 等宽数值将用系统字体");
    } else {
        s_monoFamily = QFontDatabase::applicationFontFamilies(id).value(0);
    }

    QFile f(QStringLiteral(":/fonts/MaterialSymbolsOutlined.codepoints"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("MatIcon: codepoints 表打开失败");
        return false;
    }
    QTextStream ts(&f);
    int n = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        const int sp = line.indexOf(QLatin1Char(' '));
        if (sp <= 0 || sp + 1 >= line.size())
            continue;
        bool ok = false;
        const quint32 cp = line.mid(sp + 1).toUInt(&ok, 16);
        if (ok && cp > 0) {
            s_codepoints.insert(line.left(sp), cp);
            ++n;
        }
    }
    s_ready = true;
    qDebug("MatIcon: 就绪, family=%ls, mono=%ls, %d 字形",
           qUtf16Printable(s_family), qUtf16Printable(s_monoFamily), n);
    return true;
}

bool ready() { return s_ready; }

QString family() { return s_family; }

QString monoFamily() { return s_monoFamily; }

quint32 codepoint(const QString &glyphName)
{
    return s_codepoints.value(glyphName, 0);
}

QPixmap pixmap(const QString &glyphName, const QColor &color,
               int sizePx, qreal fill, qreal devicePixelRatio)
{
    if (!s_ready || sizePx <= 0)
        return QPixmap();
    const quint32 cp = codepoint(glyphName);
    if (!cp)
        return QPixmap();

    const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
    const char32_t ucs4 = cp;
    const QString ch = QString::fromUcs4(&ucs4, 1);

    QFont f(s_family);
    f.setPixelSize(sizePx);
    if (fill > 0.0)
        f.setVariableAxis(QFont::Tag("FILL"), float(fill));

    QPixmap pm(qRound(sizePx * dpr), qRound(sizePx * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(0, 0, sizePx, sizePx), Qt::AlignCenter, ch);
    p.end();
    return pm;
}

QIcon icon(const QString &glyphName, const QColor &normalColor,
           const QColor &checkedColor, const QColor &hoverColor,
           int sizePx, qreal fill)
{
    QIcon ic;
    if (!s_ready)
        return ic;
    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    const QColor hover = hoverColor.isValid() ? hoverColor : normalColor;

    ic.addPixmap(pixmap(glyphName, normalColor, sizePx, fill, dpr), QIcon::Normal, QIcon::Off);
    ic.addPixmap(pixmap(glyphName, hover, sizePx, fill, dpr), QIcon::Active, QIcon::Off);
    if (checkedColor.isValid()) {
        ic.addPixmap(pixmap(glyphName, checkedColor, sizePx, fill, dpr), QIcon::Normal, QIcon::On);
        ic.addPixmap(pixmap(glyphName, checkedColor, sizePx, fill, dpr), QIcon::Active, QIcon::On);
    }
    return ic;
}

QFont font(int pixelSize, qreal fill, qreal wght)
{
    QFont f(s_family);
    f.setPixelSize(pixelSize);
    if (fill > 0.0)
        f.setVariableAxis(QFont::Tag("FILL"), float(fill));
    if (wght != 400.0)
        f.setVariableAxis(QFont::Tag("wght"), float(wght));
    return f;
}

QFont monoFont(int pixelSize)
{
    QFont f(s_monoFamily.isEmpty() ? QStringLiteral("Consolas") : s_monoFamily);
    f.setPixelSize(pixelSize);
    return f;
}

} // namespace MatIcon
