#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QScrollArea>
#include <QPoint>
#include <QMouseEvent>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <QPainter>
#include <QPen>
#include <QSlider>
#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QTreeWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QToolButton>
#include <QStackedWidget>
#include <QCheckBox>
#include <QSplitter>
#include "TopBar.h"
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
class QChart;
class QProgressBar;
class QScrollBar;
class QGridLayout;
class QTabBar;
class QButtonGroup;
QT_END_NAMESPACE

class HRulerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HRulerWidget(QWidget *parent = nullptr);
    void setDataRange(int dataWidth);
    void setOffset(int offset);
    void setZoom(float zoom);   // 水平缩放因子(像素/道),道号标尺据此换算像素↔道号
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    int m_offset = 0;       // 滚动条偏移(显示像素)
    int m_dataWidth = 0;    // 总道数
    float m_hZoom = 1.0f;   // 每道占多少显示像素
};

class VRulerWidget : public QWidget
{
    Q_OBJECT
public:
    enum Direction { Left, Right };
    explicit VRulerWidget(Direction dir, QWidget *parent = nullptr);
    void setRange(double minVal, double maxVal);
    void setLabel(const QString &label);
    void setImageHeight(int height);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    Direction m_direction;
    double m_minVal = 0;
    double m_maxVal = 100;
    QString m_label;
    int m_imageHeight = 512;
};

class CustomChartView : public QChartView
{
    Q_OBJECT

public:
    CustomChartView(QWidget *parent = nullptr);
    void setLineSeries(QLineSeries *series);
    void setLineCount(int count);
    int lineCount() const;
    void setHandleX(int idx, float val);
    float handleX(int idx) const;
    void setGainRange(float minVal, float maxVal);
    float gainMin() const;
    float gainMax() const;
    float interpolatedGain(float y) const;
    void setGainVisible(bool visible);
    void setYScale(float scale);
    void setSampleCount(int n);   // 采样点数(决定增益手柄 Y 跨度 0..n-1,随文件头 nsamp)
    float yScale() const;
    void setZeroOffset(float offset);

signals:
    void gainChanged(int idx, float val);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_lineCount;
    QVector<qreal> m_handleX;
    QVector<float> m_lineY;
    int m_sampleCount = 512;   // 增益手柄 Y 跨度上限(= nsamp,随文件头)
    int m_draggingIdx;
    QLineSeries *m_series;
    float m_gainMin = -6.0f;
    float m_gainMax = 6.0f;
    bool m_gainVisible = false;
    float m_yScale = 1.0f;
    float m_zeroOffset = 0.0f;  // time-zero offset for negative-Y shading
    qreal mapGainToWidgetX(float gainVal);
    float mapWidgetToGainX(qreal widgetX);
    qreal mapChartToWidgetY(float y);
};

// v1.0.107 数据块(多块模型): state 0=未标记 1=保留 2=删除
struct EditBlk {
    QRectF rectT;      // (t0,s0,t1,s1) trace/sample 域
    int state = 0;
};

// v1.0.108 数据解译: 层位曲线 (trace,sample) 域
struct HorizonLayer {
    QString name;
    QColor color;
    bool visible = true;
    int lineWidth = 2;      // 1-5
    bool dashed = false;
    QVector<QPointF> points;   // (trace, sample) 按 trace 升序
};

// v1.0.108 数据解译: 异常标注 (trace,sample) 域
struct AnomalyMark {
    int shape = 0;             // 0圆 1矩 2闭合多边形 3文本
    QString name;
    QString remark;
    QColor color;
    QString fontFamily = QStringLiteral("Microsoft YaHei");
    int fontSize = 12;
    QRectF rect;               // 圆(外接框)/矩/文本框
    QVector<QPointF> poly;     // 多边形顶点
};

class ImageLabel : public QLabel
{
    Q_OBJECT

public:
    ImageLabel(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    void setHyperbolaTracking(bool on);
    void setCrosshairDark(bool dark);   // 浅色背景(堆积图)用黑色十字,深色背景(B-SCAN)用白色
    void setHyperbolaParams(double firstWave, double velocity, int width,
                            double traceSpacing, double timePerSample);

    // v1.0.98 编辑模块覆盖层: 标记红虚线 + 数据块矩形框 (状态存 trace/sample 域, 抗缩放/刷新)
    void setMarkerOverlay(bool on, const QVector<int> &traces);
    // v1.0.107 多数据块: blocks + 活动块(显示手柄/按钮); 活动块由内部交互维护, 可查询
    void setEditBlocks(const QVector<EditBlk> &blocks, int activeIdx);
    void setEditBlocksVisible(bool on);
    bool editBlocksVisible() const { return m_blocksVisible; }
    int editActiveBlock() const { return m_activeBlock; }
    void setGeometryForMapping(int traceCount, int drawRows, float pxPerTrace, int wiggleStep);
    // v1.0.108 数据解译叠加: 层位曲线/异常标注/追踪种子 (mPerSample 供深度标签)
    void setInterpOverlays(const QVector<HorizonLayer> &horizons,
                           const QVector<AnomalyMark> &anomalies,
                           const QVector<QPointF> &seeds,
                           double mPerSample, int selectedAnomaly = -1);
    void setRadanLayers(const QVector<HorizonLayer> &layers);   // RADAN原生层位点(彩色圆点)
    bool hasInterpOverlay() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

signals:
    void imageClicked(const QPoint &pos);
    void gainSelected(float gain);
    void imageChanged();
    void transformSelected(int mode);
    void editRectChanged(int idx, const QRectF &rectTS);   // 块拖动/调整结束
    void editMarkKeepRequested(int idx);                    // 块上[保留]标记
    void editMarkDeleteRequested(int idx);                  // 块上[删除]标记

private:
    QImage m_image;
    QPoint m_crosshairPos;
    bool m_showCrosshair;
    bool m_crosshairDark = false;   // 十字颜色:true=黑(浅底),false=白(深底)
    float m_currentGainDb;
    int m_transformMode;
    QSize m_originalSize;

    // Hyperbola overlay (Kirchhoff interactive fitting)
    bool m_hyperbolaTracking = false;
    bool m_showHyperbola = false;
    QPoint m_hyperbolaApex;
    double m_hypFirstWave = 27.0;
    double m_hypVelocity = 0.106;      // m/ns
    int m_hypWidth = 60;               // aperture (traces)
    double m_hypTraceSpacing = 0.01;   // meters
    double m_hypTimePerSample = 0.039; // ns

    // ---- v1.0.98/107 编辑模块覆盖层 ----
    bool m_showMarkerOverlay = false;
    QVector<int> m_markerTraces;
    bool m_blocksVisible = false;
    QVector<EditBlk> m_blocks;         // 多数据块
    int m_activeBlock = -1;            // 活动块(手柄+按钮只画在它上面)
    // 映射参数(外部注入, 不自己算)
    int m_mapTraceCount = 0;
    int m_mapDrawRows = 0;
    float m_mapPxPerTrace = 1.0f;
    int m_mapWiggleStep = 0;           // 0=普通 2=wiggle(每槽2道×32px)
    enum DragMode { DragNone, DragMove, DragTL, DragT, DragTR,
                    DragR, DragBR, DragB, DragBL, DragL };
    DragMode m_dragMode = DragNone;
    int m_dragIdx = -1;                // 正在拖动的块索引
    QPointF m_dragAnchor;
    QRectF m_dragRectStart;

    int traceToWidgetX(int t) const;
    int widgetXToTrace(int x) const;
    int sampleToWidgetY(int s) const;
    int widgetYToSample(int y) const;
    QRect rectFromRectT(const QRectF &rT) const;   // trace/sample 域 → widget 像素
    QRect keepButtonRect(const QRect &r) const;
    QRect deleteButtonRect(const QRect &r) const;
    int hitBlock(const QPoint &pos) const;                      // 命中块索引(活动块优先)
    DragMode hitTest(const QPoint &pos, int idx, bool *onKeep, bool *onDel) const;
    QRectF clampNoOverlap(const QRectF &r, int selfIdx) const;  // 防重叠实时夹取
    void drawEditOverlay();
    // v1.0.108 解译叠加
    QVector<HorizonLayer> m_horizons;
    QVector<AnomalyMark> m_anomalies;
    QVector<QPointF> m_seeds;         // 追踪参考点(选中层)
    QVector<HorizonLayer> m_radanLayers;   // RADAN原生层位点(彩色圆点)
    double m_interpMPerSample = 0.0;  // 深度标签换算
    int m_interpSelectedAnomaly = -1;
    void drawInterpOverlay();
};

// (v1.0.87 旧 CustomTitleBar 已删除, 由 TopBar.h 的 TopBar 替代 — 严格按 主页-文件头.png)

// v1.0.98 雷达缩略图(编辑标记面板): 底图 + 红标记线 + 蓝视口指示框, 点击跳转
class MarkerThumbWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MarkerThumbWidget(QWidget *parent = nullptr);
    void setSource(const QImage &img);
    void setMarkers(const QVector<int> &traces);
    void setTraceCount(int n);
    void setSampleCount(int n);                            // 数据块 y 向映射
    void setBlocks(const QVector<QRectF> &blocksTS);       // 数据块(trace/sample域)蓝色框
    void setViewportRange(double x0Frac, double x1Frac);   // 0..1

signals:
    void viewportJumpRequested(double traceFrac);          // 点击空白处跳转(0..1)
    void viewportDragRequested(double centerFrac);         // 按住视口框拖动 → 主图视窗跟随

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage m_thumb;
    QVector<int> m_markers;
    int m_traceCount = 0;
    int m_sampleCount = 0;
    QVector<QRectF> m_blocks;      // 数据块(trace/sample域)
    double m_vpX0 = 0.0, m_vpX1 = 1.0;
    bool m_vpDrag = false;
    double m_vpGrabOffset = 0.0;   // 按下点相对视口中心的偏移(0..1)
};

// Per-file data and widgets for each open tab
struct TabData {
    QString filePath;
    QByteArray rawData;
    QByteArray originalRawData;
    qint64 dataOffset = 0;
    int pixelsPerRow = 512;
    float gain = 1.0f;
    int transformMode = 0;
    int traceCount = 0;
    double timeRange = 20.0;
    double depthRange = 1.25;
    bool gainApplied = false;
    bool zeroApplied = false;
    int zeroSkipRows = 0;
    float signalPosition = 0.0f;  // rhf_position from offset 22
    float hZoom = 1.0f;           // 水平缩放因子(像素/道,1.0=原始)
    bool wiggleMode = false;      // 堆积图(wiggle)显示模式
    QByteArray header;            // 前 1024 字节原始文件头(保留所有文件头信息)
    int nsamp = 512;              // 采样点数/扫描 (offset 4)
    float headerRange = 20.0f;    // 记录长度 ns (offset 26)
    float epsr = 1.0f;            // 介电常数 (offset 54)
    QVector<int> markers;         // 编辑标记(升序道号), 持久化到同名 DZX <MarkGroup>
    int dataRev = 0;              // 数据内容版本号(裁剪等改变 rawData 时++)
    QVector<EditBlk> editBlocks;  // 数据块(多块, 会话内不持久化)
    int activeEditBlock = -1;     // 活动数据块索引
    QVector<HorizonLayer> horizons;   // 层位曲线(默认2层, 持久化到 DZX <InterpGroup>)
    QVector<AnomalyMark> anomalies;   // 异常标注
    QVector<QPointF> trackSeeds;      // 追踪参考点(会话内)
    QVector<HorizonLayer> radanLayers;  // RADAN原生LayerGroup层位点(只读展示, 不进面板)

    QWidget *page = nullptr;
    QScrollArea *scrollArea = nullptr;
    QGridLayout *imageGrid = nullptr;
    ImageLabel *imageLabel = nullptr;
    CustomChartView *chartView = nullptr;
    QLineSeries *chartSeries = nullptr;
    HRulerWidget *topRuler = nullptr;
    VRulerWidget *leftRuler = nullptr;
    VRulerWidget *rightRuler = nullptr;
    QScrollBar *extHScrollBar = nullptr;
    QWidget *topLeftCorner = nullptr;
    QWidget *topRightCorner = nullptr;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void showAbout();
    void showUpgrade();
    void onImageClicked(const QPoint &pos);

private:
    QImage loadDZTFile(const QString &filePath);
    QImage renderWiggleImage(int traceCount, int drawRows, int skipRows);  // 堆积图(wiggle)渲染
    void openDztFile(const QString &filePath);
    void updateCoordinateLabel(int x, int y);
    qint32 getPixelValue(int x, int y);
    void updateChart(int xValue);
    void refreshImage();
    void createMenuBar();
    void updateRulers();
    void resizeImageLabel();
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void updateWelcomePixmap();
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void updateWindowTitle();
    bool requireOpenFile();  // 检查是否已打开文件,未打开则弹提示
    // DZX 自动处理
    struct DzxProcess {
        int typeId = 0;          // offset 0x08 的类型 ID
        QByteArray rawData;      // 解码后的完整 blob
    };
    bool parseDzxProcesses(const QString &dzxPath, QList<DzxProcess> &processes);
    void applyDzxProcessing(const QString &dzxPath);
    int m_dzxTimeZeroSkip = 0;            // DZX时间零点跳过的采样点数
    float m_dzxOriginalSignalPos = 0.0f;  // 处理前信号位置(写入操作记录)
    void saveProcessedWithDzx(const QString &origDztPath, const QList<DzxProcess> &processes);

    // Tab management
    TabData* createTab(const QString &filePath, const QImage &image);
    void closeTab(int index);
    void switchToTab(int index);
    void applyGain();
    void resetGainPanel();   // 打开增益面板时重置为默认值(0dB/线性1.0),清除上次残留
    void saveProcessedFile();
    void showWelcome();
    void hideWelcome();
    void showFileHeader();
    void createHeaderPanel();                  // v1.0.87 右侧350px文件头属性栏
    void setHeaderPanelVisible(bool visible); // 开关右栏+同步主页按钮+重定位悬浮切换按钮
    void refreshHeaderPanel();                // 解析当前DZT头填充8字段
    void syncAscanVisibility();               // A-SCAN波形列显隐同步(线扫描+波形模式 || 零点/增益编辑中)
    void createEditPanel();                   // v1.0.98 右侧256px编辑属性面板(数据块/缩放两页)
    void setHZoom(float zoom);                // 横向缩放(保持视口中心道)
    void syncRightRail();                     // 编辑面板与文件头右栏互斥
    void syncEditUiState();                   // 编辑模块状态总闸(面板显隐/页切换/控件回填)
    void createInterpPanel();                 // v1.0.108 右侧320px解译与管理面板
    void syncInterpUiState();                 // 数据解译状态总闸(面板显隐/按钮复位/列表刷新)
    void refreshHorizonList();                // 层位列表刷新
    void refreshAnomalyList();                // 异常列表刷新
    void syncInterpOverlays();                // tab解译数据 → 主图叠加
    int selectedHorizon() const;              // 层位列表当前选中层索引
    double interpMPerSample() const;          // 采样点→米(深度标签/自动追踪)
    void autoTrackHorizon(int layerIdx);      // 峰值跟随自动追踪(从种子向两侧)
    void clearTrackSeeds();                   // 停止: 清参考点
    void createMarkerPanel();                 // 底部标记面板(标记表+缩略图, 编辑标记开关)
    void refreshMarkerPanel();                // 标记表/主图覆盖层/缩略图 刷新
    void updateMarkerThumb();                 // 缩略图重建(带缓存) + 视口框
    QImage buildBscanThumbnail(int w, int h); // 从 rawData 抽样过调色板(与主图同色系)
    void insertMarkerRow();                   // 插入标记(两行间取均值)
    void deleteMarkerRow();                   // 删除选中标记
    void commitMarkers();                     // 标记提交: 排序+刷新+持久化(S4接DZX)
    double markerSpacingM();                  // 米/道(spm 优先, DZX unitsPerScan 兜底)
    static double readDzxUnitsPerScan(const QString &dztPath);
    static QVector<int> readDzxMarkers(const QString &dztPath);      // 读 DZX Profile/WayPt (RADAN原生)
    bool writeDzxMarkers(const QString &dztPath, const QVector<int> &markers);  // 写回 WayPt 格式
    static bool syncDzxMtimeToDzt(const QString &dztPath, const QString &dzxPath);
    void flushMarkersToDzx(TabData *tab);   // RADAN规律: 关闭/切换文件时一次性写入
    // v1.0.108 InterpGroup(层位+异常) DZX 读写
    static bool readDzxInterp(const QString &dztPath,
                              QVector<HorizonLayer> &horizons, QVector<AnomalyMark> &anomalies);
    static bool readDzxDLayers(const QString &dztPath, QVector<HorizonLayer> &radanLayers);
    bool writeDzxInterp(const QString &dztPath,
                        const QVector<HorizonLayer> &horizons,
                        const QVector<AnomalyMark> &anomalies);
    void commitInterp();                                             // 解译数据内存提交(排序+刷新, 不写DZX)
    void flushInterpToDzx(TabData *tab);                             // RADAN规律: 关闭/切换文件时一次性写 DZX
    void refreshSelectionInfo();            // 选区几何4字段刷新(道号/时间/尺寸)
    void clearEditBlocks();                 // 删除/重置: 清空全部数据块
    void createNewEditBlock();              // 新建数据块(自动找不重叠位置; 进块模式默认建一个)
    void markEditBlockKeep(int idx);        // 标记保留(唯一, 重复标记弹提示)
    void markEditBlockDelete(int idx);      // 标记删除
    void syncEditBlocksToView();            // tab->editBlocks → 主图覆盖层+缩略图
    void performCropSelection();            // 确认裁剪: 整幅数据裁剪为"保留"块(数据手术)
    void patchDztHeaderForTab(QByteArray &header);  // 保存路径头补丁(nsamp/ntraces/range按tab实际值)

    struct DztHeaderInfo {                     // 右栏8字段所需的DZT头子集
        QString fileName, createDate, antName;
        int nsamp = 0;
        float range = 0.f, epsr = 0.f, spm = 0.f;
    };
    bool readDztHeaderInfo(DztHeaderInfo &out);
    void showDigitalFilter();
    void showMovingAverage();
    void applyMovingAverage();
    void updateTraceRange();

    // Shared/global widgets
    QPushButton *openButton;
    QProgressBar *m_progressBar = nullptr;
    QNetworkAccessManager *m_net = nullptr;  // 升级用网络管理器(成员级,生命周期不随对话框)
    bool m_upgradeRestart = false;  // 升级下载完成:exec()返回后据此退出应用(模态内 quit 不可靠)
    QString m_pendingUpgradeNewPath;   // "下次启动再用":待应用的临时下载文件路径
    QString m_pendingUpgradeAppPath;   // 对应的应用 exe 路径
    QLabel *coordinateLabel;
    QTreeWidget *gainTree;
    QTreeWidgetItem *m_gainSampleEndItem = nullptr;  // 增益面板"采样点数/结束"(随 nsamp 更新)
    QWidget *m_headerPanel = nullptr;                // v1.0.87 右侧350px文件头属性栏
    QVector<QLabel*> m_headerValueLabels;            // 8个值单元格(文件名/天线频率/采样点数/总道数/时窗/介电常数/采集日期/道间距)
    QDialog *m_leftPanel;
    QStackedWidget *m_leftStack;
    QWidget *m_gainPage;
    QWidget *m_zeroPage;
    QComboBox *m_gainTypeCombo;
    QPushButton *m_btnApply;
    QPushButton *m_btnOK;
    QPushButton *m_btnCancel;
    QTabWidget *ribbonTab;
    QLabel *welcomeLabel;
    QPixmap m_welcomePix;          // welcome 原图(按比例铺满时缩放源)
    QList<QWidget*> m_welcomeHotspots;  // welcome 底部 4 个功能图标热区(悬停显示说明)
    QLabel *m_welcomeZoom = nullptr;    // 悬停时放大的图标(圆形)
    QLabel *m_welcomeTip = nullptr;     // 右上角功能说明文字
    QStringList m_welcomeTips;          // 4 个功能说明(悬停显示在右上角)
    QList<QPixmap> m_welcomeIconPix;    // 预切的 4 个图标(放大用)
    QTabWidget *m_docTabWidget;
    TopBar *m_topBar = nullptr;              // v1.0.87 顶栏(劳雷▾+5模块标签+齿轮/帮助/账号+窗口控制)
    QToolButton *m_btnHeaderToggle = nullptr; // 主页"文件头"按钮(与右侧文件头栏开合联动)
    bool m_showAscan = false;                 // 线扫描+波形模式(默认false=线扫描,仅B-SCAN)
    QButtonGroup *m_displayGroup = nullptr;   // 图像显示三按钮互斥组(0线扫描/1线扫描+波形/2波列图)
    QVector<QLabel*> m_cxBarLabels;           // 线性变换表下拉的20条缩略图(当前调色板合成)

    // ---- v1.0.98 编辑模块 ----
    QToolButton *m_btnEditMarker = nullptr;   // ribbon: 编辑标记
    QToolButton *m_btnEditBlock = nullptr;    // ribbon: 编辑数据块
    QToolButton *m_btnHZoom = nullptr;        // ribbon: 横向缩放
    QWidget *m_editPanel = nullptr;           // 右侧256px编辑属性面板
    QStackedWidget *m_editStack = nullptr;    // 页0=数据块 页1=横向缩放
    QLabel *m_editTitleLbl = nullptr;         // 面板标题(随页切换)
    QLabel *m_selStartLbl = nullptr;          // 选区几何: 起始道号
    QLabel *m_selEndLbl = nullptr;            // 选区几何: 终止道号
    QLabel *m_selTimeLbl = nullptr;           // 选区几何: 时间范围
    QLabel *m_selSizeLbl = nullptr;           // 选区几何: 切片尺寸
    QPushButton *m_btnNewRect = nullptr;      // 新建矩形框
    QPushButton *m_btnResetRect = nullptr;    // 重置选区
    QPushButton *m_btnCrop = nullptr;         // 确认裁剪
    QSlider *m_hZoomSlider = nullptr;         // 横向缩放滑条(1-10x)
    QSpinBox *m_hZoomSpin = nullptr;          // 横向缩放数字框
    bool m_syncingEditUi = false;             // syncEditUiState 防重入
    QWidget *m_markerPanel = nullptr;         // 底部标记面板(编辑标记开关)
    QTableWidget *m_markerTable = nullptr;    // 标记表(序号/道号/距离)
    MarkerThumbWidget *m_markerThumb = nullptr;
    bool m_fillingMarkers = false;            // 表格填充防 itemChanged 环
    QImage m_thumbCache;                      // 缩略图缓存
    QString m_thumbKey;                       // 缓存键(tab/rev/尺寸/调色板/变换)

    // ---- v1.0.108 数据解译 ----
    QToolButton *m_btnAutoTrack = nullptr;    // 自动追踪
    QToolButton *m_btnManualTrack = nullptr;  // 手动追踪
    QToolButton *m_btnAnoCircle = nullptr;    // 圆形
    QToolButton *m_btnAnoRect = nullptr;      // 矩形
    QToolButton *m_btnAnoPoly = nullptr;      // 闭合多边形
    QToolButton *m_btnAnoText = nullptr;      // 文本批注
    QWidget *m_interpPanel = nullptr;         // 右侧320px解译与管理面板
    QTreeWidget *m_horizonTree = nullptr;     // 层位列表(2层)
    QListWidget *m_anomalyList = nullptr;     // 异常标注列表
    QPushButton *m_btnPickSeed = nullptr;     // 拾取参考点
    QPushButton *m_btnTrackStart = nullptr;   // 开始
    QPushButton *m_btnTrackStop = nullptr;    // 停止
    QButtonGroup *m_trackGroup = nullptr;     // 追踪模式互斥组(自动/手动)
    QButtonGroup *m_annoGroup = nullptr;      // 标注工具互斥组(圆/矩/多边形/文本)
    int m_selectedAnomaly = -1;               // 异常列表选中索引

    // Tab group management (splitter)
    QSplitter *m_docSplitter = nullptr;
    QVector<QTabWidget*> m_tabGroups;
    QTabWidget *m_activeTabGroup = nullptr;
    void splitHorizontal(QTabWidget *srcGroup, int tabIdx);
    void splitVertical(QTabWidget *srcGroup, int tabIdx);
    void collapseEmptySplitters();
    void moveTabToGroup(QTabWidget *srcGroup, int tabIdx, QTabWidget *dstGroup);

    // Tab drag state
    QTabWidget *m_dragSrcGroup = nullptr;
    int m_dragSrcIdx = -1;
    QPoint m_dragStartPos;
    bool m_dragging = false;

    // Tab management
    QVector<TabData*> m_tabs;
    TabData *m_currentTab = nullptr;

    // Shortcut pointers to current tab's data/widgets
    QScrollArea *scrollArea;
    ImageLabel *imageLabel;
    CustomChartView *chartView;
    QLineSeries *chartSeries;
    QByteArray m_rawData;
    qint64 m_dataOffset;
    int m_pixelsPerRow;
    float m_gain;
    int m_transformMode;
    int m_lastChartX = 0;

    // Gain spinboxes (shared, created in constructor)
    QVector<QDoubleSpinBox*> m_gainSpinBoxes;

    // Color LUT (256 entries, index = pixelValue_display/65536 + 128)
    QRgb m_lut[256];
    int m_paletteIndex = 12;
    void loadLUT(int index = 1);
    int m_colorTransformIndex = 0;   // 颜色变换表索引(0=无,1-20=20种映射)
    void refreshCxBarThumbnails();  // 线性变换表缩略图重绘=当前调色板∘变换表 合成色(RADAN叠加规律)
    QString m_colorTransformName(int idx);  // 获取变换名称
    int m_traceCount;
    float m_hZoom = 1.0f;        // 当前 tab 的水平缩放
    bool m_wiggleMode = false;   // 当前 tab 的堆积图(wiggle)显示模式
    QToolButton *m_btnStack = nullptr;  // 堆积图按钮(同步 checked 状态)
    QToolButton *m_btnSwitchFile = nullptr;  // 切换文件下拉按钮(向下三角)
    void showFileSwitchDropdown();        // 弹出所有已加载文件缩略图+文件名下拉
    void activateTabData(TabData *tab);   // 激活指定 tab(跨选项卡组定位并选中)
    void repositionSwitchButton();        // 把切换三角按钮固定到文档区右上角(整个窗体最右)
    double m_timeRange;
    double m_depthRange;
    float m_signalPos = 0.0f;  // rhf_position from current file
    QByteArray m_header;          // 当前文件原始头(1024B)
    float m_headerRange = 20.0f;  // 记录长度 ns (offset 26)
    float m_epsr = 1.0f;          // 介电常数 (offset 54)
    int m_nsamp = 512;            // 采样点数 (offset 4)

    // Zero-point spinboxes & button
    QDoubleSpinBox *m_zeroOffsetSpin = nullptr;
    QDoubleSpinBox *m_zeroRangePctSpin = nullptr;
    QPushButton *m_zeroBtnApply = nullptr;

    // Digital filter dialog pointers (non-modal)
    QDialog *m_filterDlg = nullptr;
    QLineSeries *m_filterSeriesBefore = nullptr;
    QLineSeries *m_filterSeriesAfter = nullptr;
    QValueAxis *m_filterAxisXBefore = nullptr;
    QValueAxis *m_filterAxisXAfter = nullptr;
    QValueAxis *m_filterAxisYBefore = nullptr;
    QValueAxis *m_filterAxisYAfter = nullptr;
    QChart *m_filterChartAfter = nullptr;
    QChart *m_filterChartBefore = nullptr;
    QChartView *m_filterChartViewBefore = nullptr;
    QLineSeries *m_filterLowMarker = nullptr;
    QLineSeries *m_filterHighMarker = nullptr;
    QDoubleSpinBox *m_filterSpinLow = nullptr;
    QDoubleSpinBox *m_filterSpinHigh = nullptr;
    QButtonGroup *m_filterBandGroup = nullptr;
    QButtonGroup *m_filterTypeGroup = nullptr;
    QPushButton *m_filterBtnApply = nullptr;
    bool m_filterApplied = false;
    void updateFilterSpectrum(int traceIdx);
    void updateFilterSpectrumFiltered(int traceIdx);
    void computeFilteredSpectrumPreview();
    void updateFilterMarkerLine(QLineSeries *marker, double freq);

    // Background removal dialog pointers (non-modal)
    QDialog *m_bgRemovalDlg = nullptr;
    QComboBox *m_bgRemovalMethodCombo = nullptr;
    QSpinBox *m_bgRemovalWindowSpin = nullptr;
    QPushButton *m_bgRemovalBtnApply = nullptr;
    bool m_bgRemovalApplied = false;
    void showBackgroundRemoval();
    void applyBackgroundRemoval();

    // Moving average dialog pointers (non-modal)
    QDialog *m_movingAvgDlg = nullptr;
    QSpinBox *m_movingAvgWindowSpin = nullptr;
    QPushButton *m_movingAvgBtnApply = nullptr;
    bool m_movingAvgApplied = false;

    // Trace equalization dialog pointers (non-modal)
    QDialog *m_traceEqualDlg = nullptr;
    QPushButton *m_traceEqualBtnApply = nullptr;
    bool m_traceEqualApplied = false;
    void showTraceEqualization();
    void applyTraceEqualization();

    // Math operation dialog pointers (non-modal)
    QDialog *m_mathDlg = nullptr;
    QComboBox *m_mathOpTypeCombo = nullptr;
    QComboBox *m_mathNormalizeCombo = nullptr;
    QPushButton *m_mathBtnApply = nullptr;
    bool m_mathApplied = false;
    void showMathOperation();
    void applyMathOperation();

    // Deconvolution dialog pointers (non-modal)
    QDialog *m_deconvDlg = nullptr;
    QSpinBox *m_deconvFilterLenSpin = nullptr;
    QSpinBox *m_deconvPredStepSpin = nullptr;
    QPushButton *m_deconvBtnApply = nullptr;
    bool m_deconvApplied = false;
    void showDeconvolution();
    void applyDeconvolution();

    // Hilbert transform dialog pointers (non-modal)
    QDialog *m_hilbertDlg = nullptr;
    QComboBox *m_hilbertTypeCombo = nullptr;
    QPushButton *m_hilbertBtnApply = nullptr;
    bool m_hilbertApplied = false;
    void showHilbertTransform();
    void applyHilbertTransform();

    // Kirchhoff migration dialog pointers (non-modal)
    QDialog *m_kirchhoffDlg = nullptr;
    QDoubleSpinBox *m_kirchhoffFirstWaveSpin = nullptr;
    QDoubleSpinBox *m_kirchhoffVelocitySpin = nullptr;
    QSpinBox *m_kirchhoffWidthSpin = nullptr;
    QDoubleSpinBox *m_kirchhoffSpacingSpin = nullptr;
    QPushButton *m_kirchhoffBtnApply = nullptr;
    bool m_kirchhoffApplied = false;
    void showKirchhoffMigration();
    void applyKirchhoffMigration();
    void pushKirchhoffParamsToImage();

    // AI recognition (YOLOv8 classification)
    cv::dnn::Net m_yoloNet;
    bool m_yoloNetLoaded = false;
    QStringList m_yoloClasses = {"cavities", "intact", "utilities"};
    void showAIRecognition();
    void buildRadarCVMat(cv::Mat &out);
    void sliceAndSaveCrops(const cv::Mat &full, QList<cv::Rect> &rects);
    void runInference(const cv::Mat &full, const QList<cv::Rect> &rects, QList<int> &top1Ids, QList<float> &confidences);
    void drawResultOverlay(const cv::Mat &full, const QList<cv::Rect> &rects,
                           const QList<int> &top1Ids, const QList<float> &confidences, cv::Mat &out);
    void showAIResultDialog(const cv::Mat &annotated, const QList<cv::Rect> &rects,
                            const QList<int> &top1Ids, const QList<float> &confidences);
    void generateReport(const cv::Mat &annotated, const QList<cv::Rect> &rects,
                        const QList<int> &top1Ids, const QList<float> &confidences);

    // Processing range spinboxes (ribbon 数据处理 page)
    QSpinBox *m_startTraceSpin = nullptr;
    QSpinBox *m_endTraceSpin = nullptr;

    // Correct offset dialog pointers (non-modal)
    QDialog *m_correctOffsetDlg = nullptr;
    QDoubleSpinBox *m_correctTimeWindowSpin = nullptr;
    QDoubleSpinBox *m_correctAntennaFreqSpin = nullptr;
    QPushButton *m_correctBtnApply = nullptr;
    bool m_correctApplied = false;
    void showCorrectOffset();
    void applyCorrectOffset();

    // One-click processing dialog pointers (non-modal)
    QDialog *m_oneClickDlg = nullptr;
    QCheckBox *m_oneClickCorrectOffset = nullptr;
    QCheckBox *m_oneClickAmpComp = nullptr;
    QCheckBox *m_oneClickAdjZero = nullptr;
    QCheckBox *m_oneClickAdjGain = nullptr;
    QCheckBox *m_oneClickDigFilter = nullptr;
    QCheckBox *m_oneClickBgRemove = nullptr;
    QCheckBox *m_oneClickSmooth = nullptr;
    QDoubleSpinBox *m_oneClickTimeWindowSpin = nullptr;
    QDoubleSpinBox *m_oneClickAntennaFreqSpin = nullptr;
    QSpinBox *m_oneClickAmpCompSpin = nullptr;
    QSpinBox *m_oneClickZeroValueSpin = nullptr;
    QSpinBox *m_oneClickBgWindowSpin = nullptr;
    QSpinBox *m_oneClickSmoothWindowSpin = nullptr;
    QPushButton *m_oneClickBtnApply = nullptr;
    QChart *m_oneClickChart = nullptr;
    QLineSeries *m_oneClickSeries = nullptr;
    CustomChartView *m_oneClickChartView = nullptr;
    bool m_oneClickApplied = false;
    void showOneClickProcess();
    void applyOneClickProcess();
    void updateOneClickRefChart();
};

#endif
