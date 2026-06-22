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
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QToolButton>
#include <QStackedWidget>
#include <QCheckBox>
#include <QSplitter>
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
class QChart;
class QProgressBar;
class QScrollBar;
class QGridLayout;
class QTabBar;
QT_END_NAMESPACE

class HRulerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HRulerWidget(QWidget *parent = nullptr);
    void setDataRange(int dataWidth);
    void setOffset(int offset);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    int m_offset = 0;
    int m_dataWidth = 0;
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

class ImageLabel : public QLabel
{
    Q_OBJECT

public:
    ImageLabel(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    void setHyperbolaTracking(bool on);
    void setHyperbolaParams(double firstWave, double velocity, int width,
                            double traceSpacing, double timePerSample);

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

private:
    QImage m_image;
    QPoint m_crosshairPos;
    bool m_showCrosshair;
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
};

// 自定义标题栏：LOGO + 居中标题 + 最小/最大/关闭按钮
class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);
    void setTitleText(const QString &text);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QLabel *m_logoLabel;
    QLabel *m_titleLabel;
    QPushButton *m_btnMin;
    QPushButton *m_btnMax;
    QPushButton *m_btnClose;
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
    QByteArray header;            // 前 1024 字节原始文件头(保留所有文件头信息)
    int nsamp = 512;              // 采样点数/扫描 (offset 4)
    float headerRange = 20.0f;    // 记录长度 ns (offset 26)
    float epsr = 1.0f;            // 介电常数 (offset 54)

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

    // Tab management
    TabData* createTab(const QString &filePath, const QImage &image);
    void closeTab(int index);
    void switchToTab(int index);
    void applyGain();
    void saveProcessedFile();
    void showWelcome();
    void hideWelcome();
    void showFileHeader();
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
    QTabWidget *m_docTabWidget;
    CustomTitleBar *m_titleBar = nullptr;

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
    int m_traceCount;
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
