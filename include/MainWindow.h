#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QScrollArea>
#include <QPoint>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSlider>
#include <QContextMenuEvent>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QTreeWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QToolButton>
#include <QStackedWidget>

QT_BEGIN_NAMESPACE
class QChart;
class QScrollBar;
class QGridLayout;
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
    bool m_gainVisible = true;
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
    void onImageClicked(const QPoint &pos);

private:
    QImage loadDZTFile(const QString &filePath);
    void updateCoordinateLabel(int x, int y);
    qint32 getPixelValue(int x, int y);
    void updateChart(int xValue);
    void refreshImage();
    void createMenuBar();
    void updateRulers();
    void resizeImageLabel();
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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

    // Shared/global widgets
    QPushButton *openButton;
    QLabel *coordinateLabel;
    QTreeWidget *gainTree;
    QWidget *m_leftPanel;
    QStackedWidget *m_leftStack;
    QWidget *m_gainPage;
    QWidget *m_zeroPage;
    QComboBox *m_gainTypeCombo;
    QPushButton *m_btnApply;
    QPushButton *m_btnOK;
    QPushButton *m_btnCancel;
    QTabWidget *ribbonTab;
    QLabel *welcomeLabel;
    QTabWidget *m_docTabWidget;

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
};

#endif
