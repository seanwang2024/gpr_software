#pragma once
// MatIcon — Material Symbols Outlined 矢量图标工具
// 设计稿(specs/软件需求20260817/.../主页-文件头.png)中的所有图标均为该字体字形,
// 内嵌同款字体后按码点渲染, 任意缩放/高分屏下与设计稿完全一致, 且可任意着色。
// 用法: main.cpp 里 QApplication 之后调一次 MatIcon::init(), 之后任意位置
//       btn->setIcon(MatIcon::icon("folder_open", QColor("#0048af")));

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace MatIcon {

// 进程内调用一次: 注册内嵌字体 + 解析码点表(":/fonts/MaterialSymbolsOutlined.codepoints")
// 失败仅返回 false(qWarning 已打印), 不阻断启动; ready() 为 false 时调用方应回退旧 PNG 图标
bool init();
bool ready();

QString family();      // "Material Symbols Outlined"
QString monoFamily();  // "JetBrains Mono"(数值/坐标等宽字体)

// 字形名 → 码点; 未知名返回 0
quint32 codepoint(const QString &glyphName);

// 单个字形渲染为 pixmap(QPainter 居中绘制, 抗锯齿)
// sizePx 为逻辑像素; dpr 由调用方传入(通常 qGuiApp->devicePixelRatio())
// fill: FILL 变量轴 0..1(0=轮廓, 1=实心; 需 Qt 6.7+, 6.8.3 可用)
QPixmap pixmap(const QString &glyphName, const QColor &color,
               int sizePx, qreal fill = 0.0, qreal devicePixelRatio = 1.0);

// 多状态 QIcon:
//   Normal/Off = normalColor;  Active(悬停)/Off = hoverColor(无效则 normalColor)
//   Checked/On = checkedColor(无效则不添加 On 态, 由样式表着色兜底)
QIcon icon(const QString &glyphName, const QColor &normalColor,
           const QColor &checkedColor = QColor(), const QColor &hoverColor = QColor(),
           int sizePx = 24, qreal fill = 0.0);

// 字体本身(文本内嵌小图标用)与等宽数值字体
QFont font(int pixelSize, qreal fill = 0.0, qreal wght = 400.0);
QFont monoFont(int pixelSize);

} // namespace MatIcon
