#pragma once
// Theme — 全局主题 token(v1.0.149 主题设计: 科技蓝/岩土橙)
// 用法: QSS 字符串中 " + Theme::pri + " 拼接; QColor(Theme::pri) 构造。
// QSS 里的色值即"主题 token", 换主题只改这里一套映射。
#include <QString>
#include <QSettings>

namespace Theme {

enum Id { TechBlue = 0, GeotechOrange = 1 };

// 7 个语义 token(默认=岩土橙)
inline QString pri;       // 主色      蓝#0048af / 橙#ff2000
inline QString priDark;   // 按下/深   蓝#00419e / 橙#cc1a00
inline QString priMid;    // 选中实底(按钮checked/激活) 蓝#1e60d5 / 橙#ff5c33
inline QString hover;     // 悬停浅底  蓝#dee9fc / 橙#ffe2db
inline QString light;     // 浅底(分隔带/容器) 蓝#e6eeff / 橙#ffe9e3
inline QString panel;     // 面板/卡片头条 蓝#eff4ff / 橙#fff0ec
inline QString status;    // 状态栏/表格头 蓝#d9e3f6 / 橙#ffd6cc
inline QString onMid;     // 选中实底上的前景字 蓝#dee5ff / 橙#fff0ec

inline Id current = GeotechOrange;

inline void apply(Id id)
{
    current = id;
    if (id == TechBlue) {
        pri = QStringLiteral("#0048af"); priDark = QStringLiteral("#00419e");
        priMid = QStringLiteral("#1e60d5"); hover = QStringLiteral("#dee9fc");
        light = QStringLiteral("#e6eeff"); panel = QStringLiteral("#eff4ff");
        status = QStringLiteral("#d9e3f6");
        onMid = QStringLiteral("#dee5ff");
    } else {
        pri = QStringLiteral("#ff2000"); priDark = QStringLiteral("#cc1a00");
        priMid = QStringLiteral("#ff5c33"); hover = QStringLiteral("#ffe2db");
        light = QStringLiteral("#ffe9e3"); panel = QStringLiteral("#fff0ec");
        status = QStringLiteral("#ffd6cc");
        onMid = QStringLiteral("#fff0ec");
    }
}

// 启动时调用(在创建任何窗口之前): QSettings("Diting","depro") theme=blue|orange, 默认岩土橙
inline void load()
{
    QSettings st(QStringLiteral("Diting"), QStringLiteral("depro"));
    apply(st.value(QStringLiteral("theme"), QStringLiteral("orange")).toString()
              == QLatin1String("blue") ? TechBlue : GeotechOrange);
}

inline void save(Id id)
{
    QSettings st(QStringLiteral("Diting"), QStringLiteral("depro"));
    st.setValue(QStringLiteral("theme"),
                id == TechBlue ? QStringLiteral("blue") : QStringLiteral("orange"));
    apply(id);
}

} // namespace Theme
