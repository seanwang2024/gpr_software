#pragma once
// TopBar — 40px 顶部导航栏(严格按 specs/软件需求20260817/.../主页-文件头.png 设计)
//   [地听▾ 品牌下拉] [主页|编辑|数据处理|数据解译|AI分析] ...stretch... [⚙设置][?帮助][◯账号] [—□×]
// 图标全部来自内嵌 Material Symbols Outlined 字体(MatIcon)。

#include <QWidget>

class QPushButton;
class QToolButton;
class QButtonGroup;
class QMenu;

class TopBar : public QWidget
{
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);

    // 程序化切换模块(与 ribbonTab::currentChanged 反向同步;程序化 setChecked 不发 moduleChanged,无环)
    void setModuleIndex(int index);
    int moduleIndex() const;

signals:
    void moduleChanged(int index);   // 5 个模块标签
    void openFileRequested();        // 品牌下拉-打开
    void closeFileRequested();       // 品牌下拉-关闭
    void saveFileRequested();        // 品牌下拉-保存
    void assembleRequested();        // 品牌下拉-数据组装
    void workPathRequested();        // 品牌下拉-工作路径
    void convertRequested();         // 品牌下拉-格式转换
    void aboutRequested();           // 齿轮菜单-关于
    void upgradeRequested();         // 齿轮菜单-检查升级
    void helpRequested();            // 帮助-帮助文档
    void accountRequested();         // 账号-账号信息

protected:
    void mousePressEvent(QMouseEvent *event) override;        // 拖动移动窗口
    void mouseDoubleClickEvent(QMouseEvent *event) override;  // 双击最大化/还原

private:
    QButtonGroup *m_moduleGroup = nullptr;
    QToolButton *m_settingsBtn = nullptr;
    QMenu *m_settingsMenu = nullptr;
};
