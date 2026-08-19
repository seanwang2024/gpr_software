#include "TopBar.h"
#include "MatIcon.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QWindow>
#include <QWidgetAction>

#include <functional>

namespace {

// 品牌下拉菜单自绘行: [20px图标] 标题(+副标题10px), 参考 地听-下拉框.html 设计
class BrandMenuRow : public QWidget
{
public:
    BrandMenuRow(const QString &glyph, const QString &title, const QString &sub,
                 bool enabled, std::function<void()> onClick, QWidget *parent = nullptr)
        : QWidget(parent), m_enabled(enabled), m_onClick(std::move(onClick))
    {
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
        setStyleSheet("background: transparent;");
        if (!enabled)
            setToolTip(QStringLiteral("后续版本提供"));

        QHBoxLayout *lay = new QHBoxLayout(this);
        lay->setContentsMargins(12, 7, 12, 7);
        lay->setSpacing(12);

        QLabel *icon = new QLabel;
        icon->setFixedSize(20, 20);
        icon->setStyleSheet("border: none; background: transparent;");
        const QColor iconColor = enabled ? QColor(0x42, 0x46, 0x54) : QColor(0xb0, 0xb4, 0xc0);
        if (MatIcon::ready())
            icon->setPixmap(MatIcon::pixmap(glyph, iconColor, 20, 0.0, devicePixelRatioF()));
        lay->addWidget(icon);

        QVBoxLayout *txt = new QVBoxLayout;
        txt->setSpacing(0);
        QLabel *t = new QLabel(title);
        t->setStyleSheet(QString("border: none; background: transparent; font-size: 14px; color: %1;")
                             .arg(enabled ? QStringLiteral("#121c2a") : QStringLiteral("#b0b4c0")));
        txt->addWidget(t);
        if (!sub.isEmpty()) {
            QLabel *s = new QLabel(sub);
            s->setStyleSheet("border: none; background: transparent; font-size: 10px; color: #424654;");
            txt->addWidget(s);
        }
        lay->addLayout(txt);
        lay->addStretch(1);
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::HoverEnter && m_enabled)
            setStyleSheet("background: #dee9fc; border-radius: 2px;");
        else if (e->type() == QEvent::HoverLeave)
            setStyleSheet("background: transparent;");
        return QWidget::event(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && m_enabled && m_onClick) {
            if (auto *m = qobject_cast<QMenu *>(window()))
                m->close();
            m_onClick();
        }
    }

private:
    bool m_enabled;
    std::function<void()> m_onClick;
};

const char *kMenuSS =
    "QMenu { background: #ffffff; border: 1px solid #c3c6d6; border-radius: 4px; padding: 4px 0; }"
    "QMenu::item { padding: 7px 24px 7px 16px; color: #121c2a; font-size: 14px; }"
    "QMenu::item:selected { background: #dee9fc; }"
    "QMenu::separator { height: 1px; background: #c3c6d6; margin: 4px 8px; }";

} // namespace

TopBar::TopBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(40);   // 设计稿 toolbar-height
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: #f8f9ff; border-bottom: 1px solid #c3c6d6;");

    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 0, 0);
    lay->setSpacing(24);   // 设计稿 space-x-6=24px: 品牌↔模块标签

    // ---- 左: 品牌按钮 "劳雷" + ▾ (space-x-1=4px, 独立容器) ----
    QWidget *brandBox = new QWidget(this);
    QHBoxLayout *brandLay = new QHBoxLayout(brandBox);
    brandLay->setContentsMargins(0, 0, 0, 0);
    brandLay->setSpacing(2);

    QPushButton *brand = new QPushButton(QStringLiteral("劳雷"), brandBox);
    brand->setCursor(Qt::PointingHandCursor);
    brand->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #0048af;"
        " font-size: 18px; font-weight: bold; padding: 0 4px 0 6px; }"
        "QPushButton:hover { background: #dee9fc; border-radius: 2px; }");
    brandLay->addWidget(brand);

    QToolButton *arrow = new QToolButton(brandBox);
    arrow->setCursor(Qt::PointingHandCursor);
    if (MatIcon::ready())
        arrow->setIcon(MatIcon::icon(QStringLiteral("arrow_drop_down"), QColor(0x73, 0x77, 0x85)));
    arrow->setIconSize(QSize(20, 20));
    arrow->setFixedSize(22, 24);
    arrow->setStyleSheet(
        "QToolButton { border: none; background: transparent; }"
        "QToolButton:hover { background: #dee9fc; border-radius: 2px; }");
    brandLay->addWidget(arrow);
    lay->addWidget(brandBox);

    // 品牌下拉菜单(200px): 打开/关闭/保存 + 数据组装/工作路径/格式转换(占位)
    QMenu *brandMenu = new QMenu(this);
    brandMenu->setFixedWidth(200);
    brandMenu->setStyleSheet(kMenuSS);
    auto addRow = [&](const QString &glyph, const QString &title, const QString &sub,
                      bool enabled, std::function<void()> fn) {
        auto *row = new BrandMenuRow(glyph, title, sub, enabled, std::move(fn));
        auto *wa = new QWidgetAction(brandMenu);
        wa->setDefaultWidget(row);
        brandMenu->addAction(wa);
    };
    addRow(QStringLiteral("folder_open"), QStringLiteral("打开"), QString(), true,
           [this] { emit openFileRequested(); });
    addRow(QStringLiteral("close"), QStringLiteral("关闭"), QString(), true,
           [this] { emit closeFileRequested(); });
    addRow(QStringLiteral("save"), QStringLiteral("保存"), QString(), true,
           [this] { emit saveFileRequested(); });
    brandMenu->addSeparator();
    addRow(QStringLiteral("account_tree"), QStringLiteral("数据组装"),
           QStringLiteral("批量处理多个文件"), false, nullptr);
    addRow(QStringLiteral("folder_shared"), QStringLiteral("工作路径"), QString(), false, nullptr);
    addRow(QStringLiteral("transform"), QStringLiteral("格式转换"),
           QStringLiteral("DZX/DZT 转原生格式"), false, nullptr);

    auto popupBrandMenu = [this, brandMenu]() {
        brandMenu->popup(mapToGlobal(QPoint(8, height())));
    };
    connect(brand, &QPushButton::clicked, this, popupBrandMenu);
    connect(arrow, &QToolButton::clicked, this, popupBrandMenu);

    // ---- 中: 5 个模块标签(互斥, 顶栏即模块切换) ----
    QHBoxLayout *tabs = new QHBoxLayout;
    tabs->setSpacing(16);   // 设计稿 space-x-4=16px: 标签间距
    lay->addLayout(tabs, 1);

    m_moduleGroup = new QButtonGroup(this);
    m_moduleGroup->setExclusive(true);
    const QString moduleNames[5] = {
        QStringLiteral("主页"), QStringLiteral("编辑"), QStringLiteral("数据处理"),
        QStringLiteral("数据解译"), QStringLiteral("AI分析")};
    for (int i = 0; i < 5; ++i) {
        QPushButton *b = new QPushButton(moduleNames[i], this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(40);
        b->setStyleSheet(
            "QPushButton { border: none; border-bottom: 2px solid transparent;"
            " border-top-left-radius: 2px; border-top-right-radius: 2px;"
            " padding: 10px 8px 4px 8px; background: transparent; color: #424654; font-size: 14px; }"
            "QPushButton:hover { background: #dee9fc; }"
            "QPushButton:checked { color: #0048af; font-weight: bold;"
            " border-bottom: 2px solid #0048af; }");
        m_moduleGroup->addButton(b, i);
        tabs->addWidget(b);
    }
    m_moduleGroup->button(0)->setChecked(true);
    connect(m_moduleGroup, &QButtonGroup::idClicked, this,
            [this](int id) { emit moduleChanged(id); });

    // ---- 右: ⚙设置 / ?帮助 / ◯账号 ----
    auto iconBtn = [this](const QString &glyph) -> QToolButton * {
        QToolButton *b = new QToolButton(this);
        if (MatIcon::ready())
            b->setIcon(MatIcon::icon(glyph, QColor(0x73, 0x77, 0x85), QColor(),
                                     QColor(0x12, 0x1c, 0x2a), 22));
        b->setIconSize(QSize(22, 22));
        b->setFixedSize(30, 30);
        b->setStyleSheet(
            "QToolButton { border: none; border-radius: 4px; background: transparent; }"
            "QToolButton:hover { background: #dee9fc; }");
        return b;
    };

    // 三个角标 space-x-2=8px, 独立容器(不受外层 24px spacing 影响)
    QHBoxLayout *corner = new QHBoxLayout;
    corner->setSpacing(8);
    auto popupRight = [](QToolButton *btn, QMenu *menu) {
        menu->show();
        menu->move(btn->mapToGlobal(QPoint(btn->width() - menu->width(), btn->height())));
    };

    // ⚙ 设置: 关于 + 检查升级
    m_settingsBtn = iconBtn(QStringLiteral("settings"));
    m_settingsMenu = new QMenu(this);
    m_settingsMenu->setStyleSheet(kMenuSS);
    QAction *aAbout = m_settingsMenu->addAction(QStringLiteral("关于"));
    connect(aAbout, &QAction::triggered, this, [this] { emit aboutRequested(); });
    QAction *aUpgrade = m_settingsMenu->addAction(QStringLiteral("检查升级"));
    connect(aUpgrade, &QAction::triggered, this, [this] { emit upgradeRequested(); });
    connect(m_settingsBtn, &QToolButton::clicked, this, [this, &popupRight]() {
        popupRight(m_settingsBtn, m_settingsMenu);
    });
    corner->addWidget(m_settingsBtn);

    // ? 帮助: 帮助文档
    QToolButton *helpBtn = iconBtn(QStringLiteral("help"));
    QMenu *helpMenu = new QMenu(this);
    helpMenu->setStyleSheet(kMenuSS);
    QAction *aHelpDoc = helpMenu->addAction(QStringLiteral("帮助文档"));
    connect(aHelpDoc, &QAction::triggered, this, [this] { emit helpRequested(); });
    connect(helpBtn, &QToolButton::clicked, this, [helpBtn, helpMenu, &popupRight]() {
        popupRight(helpBtn, helpMenu);
    });
    corner->addWidget(helpBtn);

    // ◯ 账号: 账号信息
    QToolButton *accountBtn = iconBtn(QStringLiteral("account_circle"));
    QMenu *accountMenu = new QMenu(this);
    accountMenu->setStyleSheet(kMenuSS);
    QAction *aAccount = accountMenu->addAction(QStringLiteral("账号信息"));
    connect(aAccount, &QAction::triggered, this, [this] { emit accountRequested(); });
    connect(accountBtn, &QToolButton::clicked, this, [accountBtn, accountMenu, &popupRight]() {
        popupRight(accountBtn, accountMenu);
    });
    corner->addWidget(accountBtn);

    lay->addLayout(corner);
    lay->addSpacing(12);

    // ---- 最右: 窗口控制 —□× ----
    auto winBtn = [this](const QString &text) -> QPushButton * {
        QPushButton *btn = new QPushButton(text, this);
        btn->setFixedSize(46, 40);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; color: #444444; font-size: 14px; }"
            "QPushButton:hover { background: #e5e5e5; color: #444444; }"
            "QPushButton:pressed { background: #d0d0d0; }");
        return btn;
    };
    QPushButton *btnMin = winBtn(QString::fromUtf8("\xE2\x80\x94"));    // —
    QPushButton *btnMax = winBtn(QString::fromUtf8("\xE2\x96\xA1"));    // □
    QPushButton *btnClose = winBtn(QString::fromUtf8("\xC3\x97"));      // ×
    QWidget *winBox = new QWidget(this);
    QHBoxLayout *winLay = new QHBoxLayout(winBox);
    winLay->setContentsMargins(0, 0, 0, 0);
    winLay->setSpacing(0);
    winLay->addWidget(btnMin);
    winLay->addWidget(btnMax);
    winLay->addWidget(btnClose);
    lay->addWidget(winBox);

    connect(btnMin, &QPushButton::clicked, this, [this]() {
        if (auto *w = window()) w->showMinimized();
    });
    connect(btnMax, &QPushButton::clicked, this, [this]() {
        if (auto *w = window()) {
            if (w->windowState() & Qt::WindowMaximized) w->showNormal();
            else w->showMaximized();
        }
    });
    connect(btnClose, &QPushButton::clicked, this, [this]() {
        if (auto *w = window()) w->close();
    });
}

void TopBar::setModuleIndex(int index)
{
    if (QAbstractButton *b = m_moduleGroup->button(index))
        b->setChecked(true);
}

int TopBar::moduleIndex() const
{
    return m_moduleGroup->checkedId();
}

void TopBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (auto *w = window()) {
            if (QWindow *wh = w->windowHandle())
                wh->startSystemMove();
        }
    }
    QWidget::mousePressEvent(event);
}

void TopBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (auto *w = window()) {
            if (w->windowState() & Qt::WindowMaximized) w->showNormal();
            else w->showMaximized();
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}
