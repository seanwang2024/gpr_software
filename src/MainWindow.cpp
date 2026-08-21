#include "MainWindow.h"
#include "version.h"
#include "TopBar.h"
#include "MatIcon.h"

// 诊断终端输出(定义在 main.cpp,直接写 UTF-8 到控制台 stderr)
extern void diagPrint(const QString &msg);

// 前置声明(定义在后文): 数据解译默认两层位
static HorizonLayer makeDefaultHorizon(int idx);

// v1.0.129: 列表行容器 — 几何命中判定(替代事件过滤器方案, 彻底可靠)
// 所有子控件(QLineEdit/QToolButton/QLabel)一律 WA_TransparentForMouseEvents,
// 鼠标事件全部落在行容器自身:
//   单击行内任意位置 → clicked() 选中该项(名称/色点/图标/空白都能选)
//   双击 → doubleClicked(pos), 由创建方按子控件 geometry() 分派编辑/删除
// 编辑态的 QLineEdit 临时去掉穿透属性, editingFinished 后恢复。
#include <QLineEdit>
#include <QMouseEvent>
class ListRowWidget : public QWidget {
public:
    std::function<void()> clicked;
    std::function<void(const QPoint &)> doubleClicked;   // pos = 行内坐标
    explicit ListRowWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground, true);   // 样式表背景必定绘制
    }
protected:
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && clicked) clicked();
        e->accept();
    }
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && doubleClicked)
            doubleClicked(e->pos());
        e->accept();
    }
};

// 双击解锁QLineEdit编辑; editingFinished中恢复: setReadOnly(true)+NoFocus+穿透
static void unlockLineEditEdit(QLineEdit *ed)
{
    ed->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    ed->setReadOnly(false);
    ed->setFocusPolicy(Qt::StrongFocus);
    ed->setFocus();
    ed->selectAll();
}
static void relockLineEditEdit(QLineEdit *ed)
{
    ed->setReadOnly(true);
    ed->setFocusPolicy(Qt::NoFocus);
    ed->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

#include <QButtonGroup>
#include <QTimer>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QInputDialog>
#include <QKeyEvent>
#include <QXmlStreamReader>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDialog>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QCoreApplication>
#include <QRegion>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QScrollArea>
#include <QWheelEvent>
#include <QTextEdit>
#include <QSplitter>
#include <QPainter>
#include <QMouseEvent>
#include <QPen>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QChart>
#include <QMenu>
#include <QDialog>
#include <QTreeWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QMenuBar>
#include <QActionGroup>
#include <QWidgetAction>
#include <QTabWidget>
#include <QToolButton>
#include <QFrame>
#include <QGridLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QScrollBar>
#include <QFontMetrics>
#include <QTimer>
#include <QFileInfo>
#include <QDateTime>
#include <QProgressBar>
#include <QCoreApplication>
#include <QCheckBox>
#include <QTextStream>
#include <QFormLayout>
#include <QRegularExpression>
#include <QWindow>
#include <QPrinter>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <functional>
#include <cmath>
#include <complex>
#include <vector>
#include <limits>

// 可缩放滚动图像视图：Ctrl+滚轮缩放，普通滚轮滚动，拖拽平移
namespace {
class ZoomableImageView : public QScrollArea
{
public:
    explicit ZoomableImageView(const QPixmap &pix, QWidget *parent = nullptr)
        : QScrollArea(parent), m_orig(pix), m_zoom(1.0) {
        m_label = new QLabel(this);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setStyleSheet("background: #202020;");
        setBackgroundRole(QPalette::Dark);
        setWidget(m_label);
        setWidgetResizable(false);
        setAlignment(Qt::AlignCenter);
        applyZoom();
    }
protected:
    void wheelEvent(QWheelEvent *event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            double factor = (event->angleDelta().y() > 0) ? 1.20 : 1.0 / 1.20;
            double newZoom = m_zoom * factor;
            newZoom = qBound(0.1, newZoom, 10.0);
            if (qAbs(newZoom - m_zoom) < 1e-4) { event->accept(); return; }
            m_zoom = newZoom;
            applyZoom();
            event->accept();
        } else {
            QScrollArea::wheelEvent(event);
        }
    }
private:
    void applyZoom() {
        QSize sz = m_orig.size() * m_zoom;
        QPixmap pm = m_orig.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_label->setPixmap(pm);
        m_label->resize(pm.size());
    }
    QPixmap m_orig;
    QLabel *m_label;
    double m_zoom;
};
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#endif

// --- FilterChartView: chart with draggable vertical marker lines ---
class FilterChartView : public QChartView
{
public:
    FilterChartView(QChart *chart, QWidget *parent = nullptr)
        : QChartView(chart, parent), m_dragging(0)
    {
        setMouseTracking(true);
        setRenderHint(QPainter::Antialiasing);
    }

    void setMarkers(QLineSeries *lowMarker, QLineSeries *highMarker,
                    QValueAxis *axisX, QValueAxis *axisY,
                    QDoubleSpinBox *spinLow, QDoubleSpinBox *spinHigh)
    {
        m_lowMarker = lowMarker;
        m_highMarker = highMarker;
        m_axisX = axisX;
        m_axisY = axisY;
        m_spinLow = spinLow;
        m_spinHigh = spinHigh;
    }

    void setFreqChangedCallback(std::function<void()> cb) { m_freqChangedCb = cb; }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && chart()) {
            QPointF scenePos = mapToScene(event->pos());
            QPointF chartPt = chart()->mapToValue(scenePos);
            qreal freq = chartPt.x();
            qreal lowX = m_spinLow ? m_spinLow->value() : 200;
            qreal highX = m_spinHigh ? m_spinHigh->value() : 600;
            qreal rangeX = m_axisX->max() - m_axisX->min();
            qreal threshold = rangeX * 0.02;
            qreal distLow = qAbs(freq - lowX);
            qreal distHigh = qAbs(freq - highX);
            if (distLow < distHigh && distLow < threshold)
                m_dragging = 1;
            else if (distHigh < threshold)
                m_dragging = 2;
            else
                m_dragging = 0;

            if (m_dragging) return; // consume event
        }
        QChartView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging && chart()) {
            QPointF scenePos = mapToScene(event->pos());
            QPointF chartPt = chart()->mapToValue(scenePos);
            qreal freq = qBound(m_axisX->min(), chartPt.x(), m_axisX->max());
            if (m_dragging == 1) {
                m_spinLow->blockSignals(true);
                m_spinLow->setValue(freq);
                m_spinLow->blockSignals(false);
                updateMarker(m_lowMarker, freq);
            } else if (m_dragging == 2) {
                m_spinHigh->blockSignals(true);
                m_spinHigh->setValue(freq);
                m_spinHigh->blockSignals(false);
                updateMarker(m_highMarker, freq);
            }
            if (m_freqChangedCb) m_freqChangedCb();
            return;
        }
        QChartView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_dragging) {
            m_dragging = 0;
            if (m_freqChangedCb) m_freqChangedCb();
            return;
        }
        QChartView::mouseReleaseEvent(event);
    }

private:
    int m_dragging;
    std::function<void()> m_freqChangedCb;
    QLineSeries *m_lowMarker = nullptr;
    QLineSeries *m_highMarker = nullptr;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    QDoubleSpinBox *m_spinLow = nullptr;
    QDoubleSpinBox *m_spinHigh = nullptr;

    void updateMarker(QLineSeries *marker, double freq)
    {
        if (!marker || !m_axisY) return;
        marker->replace(0, QPointF(freq, m_axisY->min()));
        marker->replace(1, QPointF(freq, m_axisY->max()));
    }
};

// --- FFT ---
static void fft(std::vector<std::complex<double>> &x)
{
    int N = x.size();
    for (int i = 1, j = 0; i < N; i++) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }
    for (int len = 2; len <= N; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        std::complex<double> wn(cos(ang), sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<double> w(1);
            for (int j = 0; j < len / 2; j++) {
                auto u = x[i + j], v = w * x[i + j + len / 2];
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wn;
            }
        }
    }
}

// --- Inverse FFT (in-place, result divided by N) ---
static void ifft(std::vector<std::complex<double>> &x)
{
    int N = x.size();
    for (auto &v : x) v = std::conj(v);
    fft(x);
    for (auto &v : x) v = std::conj(v) / static_cast<double>(N);
}

static double niceInterval(double range, int maxTicks)
{
    if (range <= 0 || maxTicks <= 0) return range;
    double rough = range / maxTicks;
    double mag = pow(10.0, floor(log10(rough)));
    double norm = rough / mag;
    double nice;
    if (norm <= 1.0) nice = 1.0;
    else if (norm <= 2.0) nice = 2.0;
    else if (norm <= 5.0) nice = 5.0;
    else nice = 10.0;
    return nice * mag;
}

// --- CustomChartView ---
CustomChartView::CustomChartView(QWidget *parent)
    : QChartView(parent)
    , m_lineCount(0)
    , m_draggingIdx(-1)
    , m_series(nullptr)
{
    setMouseTracking(true);
}

void CustomChartView::setLineSeries(QLineSeries *series) { m_series = series; }

void CustomChartView::setLineCount(int count)
{
    // count=1 → internally 2 handles (top Y=0, bottom Y=511) connected by red line
    int actual = (count == 1) ? 2 : count;
    m_lineCount = qBound(0, actual, 16);
    m_lineY.resize(m_lineCount);
    m_handleX.resize(m_lineCount);
    for (int i = 0; i < m_lineCount; ++i) {
        if (m_lineCount == 1)
            m_lineY[i] = 0;
        else
            m_lineY[i] = (float)(m_sampleCount - 1) * i / (m_lineCount - 1);
        if (m_handleX[i] == 0) m_handleX[i] = 0;
    }
    update();
}

void CustomChartView::setSampleCount(int n)
{
    if (n > 0) m_sampleCount = n;
    if (m_lineCount > 0) {   // 重新分布手柄 Y 位置到 0..nsamp-1
        for (int i = 0; i < m_lineCount; ++i)
            m_lineY[i] = (m_lineCount == 1) ? 0.0f : (float)(m_sampleCount - 1) * i / (m_lineCount - 1);
        update();
    }
}

int CustomChartView::lineCount() const { return m_lineCount; }

void CustomChartView::setHandleX(int idx, float val)
{
    if (idx >= 0 && idx < m_lineCount) {
        m_handleX[idx] = val;
        update();
    }
}

float CustomChartView::handleX(int idx) const
{
    if (idx >= 0 && idx < m_lineCount) return m_handleX[idx];
    return 0;
}

void CustomChartView::setGainRange(float minVal, float maxVal)
{
    m_gainMin = minVal;
    m_gainMax = maxVal;
    update();
}

float CustomChartView::gainMin() const { return m_gainMin; }
float CustomChartView::gainMax() const { return m_gainMax; }

void CustomChartView::setGainVisible(bool visible)
{
    m_gainVisible = visible;
    update();
}

void CustomChartView::setYScale(float scale)
{
    m_yScale = scale;
    update();
}

float CustomChartView::yScale() const { return m_yScale; }

void CustomChartView::setZeroOffset(float offset)
{
    m_zeroOffset = offset;
    update();
}

float CustomChartView::interpolatedGain(float y) const
{
    if (m_lineCount == 0) return 1.0f;
    if (m_lineCount == 1) return static_cast<float>(m_handleX[0]);

    // Find the two handles that bracket y
    // m_lineY is sorted ascending (0, 511/N, 2*511/N, ..., 511)
    if (y <= m_lineY[0]) return static_cast<float>(m_handleX[0]);
    if (y >= m_lineY[m_lineCount - 1]) return static_cast<float>(m_handleX[m_lineCount - 1]);

    for (int i = 0; i < m_lineCount - 1; ++i) {
        if (y >= m_lineY[i] && y <= m_lineY[i + 1]) {
            float t = (m_lineY[i + 1] == m_lineY[i]) ? 0.0f :
                      (y - m_lineY[i]) / (m_lineY[i + 1] - m_lineY[i]);
            return static_cast<float>(m_handleX[i] + t * (m_handleX[i + 1] - m_handleX[i]));
        }
    }
    return static_cast<float>(m_handleX[m_lineCount - 1]);
}

// Map gain value to pixel X within plot area
qreal CustomChartView::mapGainToWidgetX(float gainVal)
{
    if (!chart()) return 0;
    QRectF pa = chart()->plotArea();
    float range = m_gainMax - m_gainMin;
    if (range <= 0) return pa.center().x();
    float t = (gainVal - m_gainMin) / range;
    return pa.left() + t * pa.width();
}

// Map pixel X back to gain value
float CustomChartView::mapWidgetToGainX(qreal widgetX)
{
    if (!chart()) return 0;
    QRectF pa = chart()->plotArea();
    float range = m_gainMax - m_gainMin;
    if (range <= 0) return 0;
    float t = static_cast<float>((widgetX - pa.left()) / pa.width());
    return m_gainMin + t * range;
}

qreal CustomChartView::mapChartToWidgetY(float y)
{
    if (!m_series || !chart()) return 0;
    return chart()->mapToPosition(QPointF(0, y), m_series).y();
}

void CustomChartView::paintEvent(QPaintEvent *event)
{
    if (!chart()) return;

    // Auto-adjust top margin to fit gain labels before QChartView lays out
    if (m_gainVisible) {
        QFontMetrics fm(font());
        int labelHeight = fm.height() + 6;
        QMargins mg = chart()->margins();
        if (mg.top() < labelHeight) {
            chart()->setMargins(QMargins(mg.left(), labelHeight, mg.right(), mg.bottom()));
        }
    }

    QChartView::paintEvent(event);

    QRectF plotArea = chart()->plotArea();

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_gainVisible) {
        // Draw gain scale ticks and labels at top of plot
        QPen tickPen(Qt::gray, 1, Qt::DotLine);
        painter.setPen(tickPen);
        QFontMetrics fm(painter.font());

        float rangeSpan = m_gainMax - m_gainMin;
        float step = 6.0f;
        if (rangeSpan > 0 && rangeSpan <= 20.0f)
            step = rangeSpan / 2.0f;
        else if (rangeSpan > 20.0f)
            step = 6.0f;

        int tickIdx = 0;
        for (float v = m_gainMin; v <= m_gainMax + 0.001f; v += step) {
            qreal tx = mapGainToWidgetX(v);
            if (tx >= plotArea.left() - 1 && tx <= plotArea.right() + 1) {
                painter.setPen(tickPen);
                painter.drawLine(QPointF(tx, plotArea.top()), QPointF(tx, plotArea.bottom()));
                QString label = QString::number(static_cast<int>(v));
                painter.setPen(Qt::black);
                painter.drawText(QPointF(tx - fm.horizontalAdvance(label) / 2.0,
                                         plotArea.top() - 3), label);
            }
            if (++tickIdx > 20) break;
        }

        for (int i = 0; i < m_lineCount; ++i) {
            qreal wy = mapChartToWidgetY(m_lineY[i] * m_yScale);
            qreal hx = mapGainToWidgetX(m_handleX[i]);

            QPen pen(Qt::red, 1);
            painter.setPen(pen);
            painter.drawLine(QPointF(plotArea.left(), wy), QPointF(plotArea.right(), wy));

            painter.setBrush(Qt::red);
            painter.drawRect(QRectF(hx - 4, wy - 4, 8, 8));

            if (i > 0) {
                qreal prevWy = mapChartToWidgetY(m_lineY[i - 1] * m_yScale);
                qreal prevHx = mapGainToWidgetX(m_handleX[i - 1]);
                painter.drawLine(QPointF(prevHx, prevWy), QPointF(hx, wy));
            }
        }
    } else if (m_yScale != 1.0f && m_zeroOffset < 0.0f) {
        // Zero-point mode: draw shaded rectangle for negative-Y region
        // bottom-left: (X min, Y=0), top-right: (X max, Y=zeroOffset)
        qreal wyTop = mapChartToWidgetY(m_zeroOffset);
        qreal wyBottom = mapChartToWidgetY(0.0f);
        QRectF negRect(plotArea.left(), wyTop, plotArea.width(), wyBottom - wyTop);
        painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
        painter.setBrush(QColor(200, 200, 200, 80));
        painter.drawRect(negRect);
    }
}

void CustomChartView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();
        for (int i = 0; i < m_lineCount; ++i) {
            qreal wy = mapChartToWidgetY(m_lineY[i] * m_yScale);
            qreal hx = mapGainToWidgetX(m_handleX[i]);
            if (qAbs(pos.y() - wy) < 6 && qAbs(pos.x() - hx) < 8) {
                m_draggingIdx = i;
                break;
            }
        }
    }
    QChartView::mousePressEvent(event);
}

void CustomChartView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingIdx >= 0 && m_draggingIdx < m_lineCount) {
        float val = mapWidgetToGainX(event->pos().x());
        // Clamp minimum only (max expands via 10*N)
        if (val < m_gainMin) val = m_gainMin;
        m_handleX[m_draggingIdx] = val;
        update();
        emit gainChanged(m_draggingIdx, static_cast<float>(m_handleX[m_draggingIdx]));
    }
    QChartView::mouseMoveEvent(event);
}

void CustomChartView::mouseReleaseEvent(QMouseEvent *event)
{
    m_draggingIdx = -1;
    QChartView::mouseReleaseEvent(event);
}

// --- ImageLabel ---
#include <QLineEdit>
#include <QDoubleValidator>
ImageLabel::ImageLabel(QWidget *parent)
    : QLabel(parent)
    , m_showCrosshair(false)
    , m_currentGainDb(0)
    , m_transformMode(0)
{
    setAlignment(Qt::AlignCenter);
    setMinimumSize(100, 50);
    setText("No image loaded");
    setStyleSheet("border: 1px solid gray;");
    setFocusPolicy(Qt::ClickFocus);   // v1.0.122 让 keyPressEvent(回车确认)生效
    // v1.0.120: 流动虚线动画(编辑态异常+多边形绘制时运行)
    m_dashTimer = new QTimer(this);
    m_dashTimer->setInterval(80);
    connect(m_dashTimer, &QTimer::timeout, this, [this]() {
        m_dashOffset = (m_dashOffset + 1) % 20;
        update();
    });
    setMouseTracking(true);
}

void ImageLabel::setImage(const QImage &img)
{
    m_image = img;
    m_showCrosshair = false;
    if (!img.isNull()) {
        m_originalSize = img.size();
        setPixmap(QPixmap::fromImage(img));
    } else {
        m_originalSize = QSize();
        clear();
        setText("No image loaded");
    }
    emit imageChanged();
}

void ImageLabel::setHyperbolaTracking(bool on)
{
    m_hyperbolaTracking = on;
    setMouseTracking(on);
    if (!on) {
        m_showHyperbola = false;
        update();
    }
}

void ImageLabel::setCrosshairDark(bool dark)
{
    m_crosshairDark = dark;
    update();
}

void ImageLabel::setHyperbolaParams(double firstWave, double velocity, int width,
                                    double traceSpacing, double timePerSample)
{
    m_hypFirstWave = firstWave;
    m_hypVelocity = velocity;
    m_hypWidth = width;
    m_hypTraceSpacing = traceSpacing;
    m_hypTimePerSample = (timePerSample > 1e-9) ? timePerSample : 1e-9;
    if (m_showHyperbola) update();
}

// ---- v1.0.98 编辑模块覆盖层 ----

void ImageLabel::setMarkerOverlay(bool on, const QVector<int> &traces)
{
    m_showMarkerOverlay = on;
    m_markerTraces = traces;
    update();
}

void ImageLabel::setEditBlocks(const QVector<EditBlk> &blocks, int activeIdx)
{
    m_blocks = blocks;
    m_activeBlock = (activeIdx >= 0 && activeIdx < blocks.size()) ? activeIdx : -1;
    if (m_dragIdx >= m_blocks.size()) { m_dragIdx = -1; m_dragMode = DragNone; }
    update();
}

void ImageLabel::setEditBlocksVisible(bool on)
{
    m_blocksVisible = on;
    if (!on) { m_dragMode = DragNone; m_dragIdx = -1; }
    update();
}

void ImageLabel::setGeometryForMapping(int traceCount, int drawRows, float pxPerTrace, int wiggleStep)
{
    m_mapTraceCount = traceCount;
    m_mapDrawRows = drawRows;
    m_mapPxPerTrace = (pxPerTrace > 0.01f) ? pxPerTrace : 1.0f;
    m_mapWiggleStep = wiggleStep;
    update();
}

// 道号 → widget x: 普通模式 t×pxPerTrace; wiggle 槽位中心 ((t/2)*32+16)×pxPerTrace
int ImageLabel::traceToWidgetX(int t) const
{
    if (m_mapWiggleStep == 2)
        return qRound(((t / 2) * 32 + 16) * static_cast<double>(m_mapPxPerTrace));
    return qRound(t * static_cast<double>(m_mapPxPerTrace));
}

int ImageLabel::widgetXToTrace(int x) const
{
    if (m_mapWiggleStep == 2)
        return qRound(((x / static_cast<double>(m_mapPxPerTrace)) / 32.0)) * 2;   // 对齐偶数道
    return qRound(x / static_cast<double>(m_mapPxPerTrace));
}

int ImageLabel::sampleToWidgetY(int s) const
{
    if (m_mapDrawRows <= 1) return 0;
    return qRound(static_cast<double>(s) * (height() - 1) / (m_mapDrawRows - 1));
}

int ImageLabel::widgetYToSample(int y) const
{
    if (height() <= 1) return 0;
    return qRound(static_cast<double>(y) * (m_mapDrawRows - 1) / qMax(1, height() - 1));
}

QRect ImageLabel::rectFromRectT(const QRectF &rT) const
{
    const int x1 = traceToWidgetX(qRound(rT.left()));
    const int x2 = traceToWidgetX(qRound(rT.right()));
    const int y1 = sampleToWidgetY(qRound(rT.top()));
    const int y2 = sampleToWidgetY(qRound(rT.bottom()));
    return QRect(QPoint(qMin(x1, x2), qMin(y1, y2)), QPoint(qMax(x1, x2), qMax(y1, y2)));
}

// 框上方右侧 [保留][删除] pill (画在 paintEvent, mousePress 命中区测试)
QRect ImageLabel::keepButtonRect(const QRect &r) const
{
    int y = r.top() - 26;
    if (y < 1) y = r.top() + 3;   // 框贴顶时画到框内
    return QRect(r.right() - 116, y, 54, 21);
}

QRect ImageLabel::deleteButtonRect(const QRect &r) const
{
    int y = r.top() - 26;
    if (y < 1) y = r.top() + 3;
    return QRect(r.right() - 58, y, 54, 21);
}

// 命中块索引: 活动块优先, 其余按包含关系(后画的在上)
int ImageLabel::hitBlock(const QPoint &pos) const
{
    if (!m_blocksVisible) return -1;
    if (m_activeBlock >= 0 && m_activeBlock < m_blocks.size()) {
        const QRect r = rectFromRectT(m_blocks[m_activeBlock].rectT);
        if (r.contains(pos) || keepButtonRect(r).contains(pos) || deleteButtonRect(r).contains(pos))
            return m_activeBlock;
    }
    for (int i = m_blocks.size() - 1; i >= 0; --i) {
        if (i == m_activeBlock) continue;
        if (rectFromRectT(m_blocks[i].rectT).contains(pos)) return i;
    }
    return -1;
}

ImageLabel::DragMode ImageLabel::hitTest(const QPoint &pos, int idx, bool *onKeepBtn, bool *onDeleteBtn) const
{
    *onKeepBtn = *onDeleteBtn = false;
    if (!m_blocksVisible || idx < 0 || idx >= m_blocks.size()) return DragNone;
    const QRect r = rectFromRectT(m_blocks[idx].rectT);
    if (keepButtonRect(r).contains(pos)) { *onKeepBtn = true; return DragNone; }
    if (deleteButtonRect(r).contains(pos)) { *onDeleteBtn = true; return DragNone; }
    static const int tol = 6;
    const QPoint cs[8] = {
        r.topLeft(), QPoint(r.center().x(), r.top()), r.topRight(),
        QPoint(r.right(), r.center().y()), r.bottomRight(),
        QPoint(r.center().x(), r.bottom()), r.bottomLeft(),
        QPoint(r.left(), r.center().y())};
    for (int i = 0; i < 8; ++i) {
        if (QRect(cs[i].x() - tol, cs[i].y() - tol, tol * 2, tol * 2).contains(pos))
            return static_cast<DragMode>(DragTL + i);
    }
    if (r.contains(pos)) return DragMove;
    return DragNone;
}

// 防重叠实时夹取: 候选矩形与任一其他块相交时, 沿最小推出量平移; 再夹到数据域
QRectF ImageLabel::clampNoOverlap(const QRectF &rIn, int selfIdx) const
{
    QRectF r = rIn.normalized();
    for (int pass = 0; pass < 4; ++pass) {   // 多块可能需要多轮推出
        bool moved = false;
        for (int i = 0; i < m_blocks.size(); ++i) {
            if (i == selfIdx) continue;
            const QRectF o = m_blocks[i].rectT.normalized();
            if (!r.intersects(o)) continue;
            // 四个方向的推出量, 取绝对值最小
            const qreal pushR = o.right() - r.left() + 0.5;    // 右移
            const qreal pushL = r.right() - o.left() + 0.5;    // 左移
            const qreal pushD = o.bottom() - r.top() + 0.5;    // 下移
            const qreal pushU = r.bottom() - o.top() + 0.5;    // 上移
            const qreal m = qMin(qMin(pushR, pushL), qMin(pushD, pushU));
            if (m == pushR)      r.translate(pushR, 0);
            else if (m == pushL) r.translate(-pushL, 0);
            else if (m == pushD) r.translate(0, pushD);
            else                 r.translate(0, -pushU);
            moved = true;
        }
        if (!moved) break;
    }
    // 数据域夹取 + 最小尺寸
    const double maxT = qMax(0.0, (double)m_mapTraceCount - 1);
    const double maxS = qMax(0.0, (double)m_mapDrawRows - 1);
    double l = qBound(0.0, r.left(), maxT), rr = qBound(0.0, r.right(), maxT);
    double t = qBound(0.0, r.top(), maxS), b = qBound(0.0, r.bottom(), maxS);
    if (rr - l < 1.0) { if (l > 0) l = rr - 1.0; else rr = l + 1.0; }
    if (b - t < 1.0) { if (t > 0) t = b - 1.0; else b = t + 1.0; }
    l = qBound(0.0, l, maxT); rr = qBound(l, rr, maxT);
    t = qBound(0.0, t, maxS); b = qBound(t, b, maxS);
    return QRectF(QPointF(l, t), QPointF(rr, b));
}

void ImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull()) {
        QLabel::mousePressEvent(event);
        return;
    }

    // v1.0.129: 点击图上任意异常(实线/编辑态) → 选中该项(三统一: 列表+菜单+属性)
    if (event->button() == Qt::LeftButton && !m_polyDrawing) {
        for (int i = 0; i < m_anomalies.size(); ++i) {
            const AnomalyMark &a = m_anomalies[i];
            if (a.shape < 0) continue;
            bool hit = false;
            if (a.shape == 2 && a.poly.size() >= 3)
                hit = polyContains(a.poly, event->pos());
            else if (a.shape >= 0 && a.shape <= 3 && a.shape != 2)
                hit = rectFromRectT(a.rect.normalized()).contains(event->pos());
            if (hit) {
                emit anomalyClickedOnImage(i);
                // 不return — 编辑态的仍走后续拖动逻辑
                break;
            }
        }
    }

    // v1.0.122: 点击编辑态异常外部 → 确认(变实线) — 多边形绘制模式中不触发!
    if (event->button() == Qt::LeftButton && !m_polyDrawing) {
        const int eIdx = editingAnomalyIndex();
        if (eIdx >= 0) {
            const AnomalyMark &ea = m_anomalies[eIdx];
            bool inside = false;
            if (ea.shape == 2 && ea.poly.size() >= 3)
                inside = polyContains(ea.poly, event->pos());
            else if (ea.shape >= 0 && ea.shape <= 3 && ea.shape != 2)
                inside = rectFromRectT(ea.rect.normalized()).contains(event->pos());
            if (!inside) {
                m_anomalies[eIdx].editing = false;
                emit anomalyConfirmed(eIdx);
                update();
                // 不 return, 继续处理(可能是选其他目标/十字线)
            }
        }
    }

    // v1.0.120: 多边形绘制模式 — 点击加顶点; 靠近首顶点闭合
    if (event->button() == Qt::LeftButton && m_polyDrawing) {
        const QPointF np(widgetXToTrace(event->pos().x()),
                         widgetYToSample(event->pos().y()));
        if (m_polyPoints.size() >= 3) {
            const QPointF &fp = m_polyPoints.first();
            const int fx = traceToWidgetX(qRound(fp.x()));
            const int fy = sampleToWidgetY(qRound(fp.y()));
            if (qAbs(event->pos().x() - fx) < 12 && qAbs(event->pos().y() - fy) < 12) {
                // 闭合
                m_polyDrawing = false;
                emit anomalyPolyDone(editingAnomalyIndex(), m_polyPoints);
                m_polyPoints.clear();
                update();
                return;
            }
        }
        m_polyPoints.append(np);
        update();
        return;
    }

    // v1.0.120: 编辑态异常 — 手柄调整大小 > 拖动移动(优先于数据块)
    if (event->button() == Qt::LeftButton) {
        const int aIdx = editingAnomalyIndex();
        if (aIdx >= 0) {
            const AnomalyMark &a = m_anomalies[aIdx];
            if (a.shape >= 0 && a.shape <= 3 && !(a.shape == 2 && a.poly.isEmpty())) {
                // 多边形: 先检测顶点手柄(可拖动单个顶点)
                if (a.shape == 2 && a.poly.size() >= 3) {
                    static const int vtol = 8;
                    for (int vi = 0; vi < a.poly.size(); ++vi) {
                        const int vx = traceToWidgetX(qRound(a.poly[vi].x()));
                        const int vy = sampleToWidgetY(qRound(a.poly[vi].y()));
                        if (QRect(vx - vtol, vy - vtol, vtol * 2, vtol * 2).contains(event->pos())) {
                            m_anomalyDragIdx = aIdx;
                            m_anomalyDragMode = DragNone;
                            m_anomalyVertexIdx = vi;   // 拖动此顶点
                            grabMouse();
                            return;
                        }
                    }
                }
                if (a.shape != 2) {
                    const QRect r = rectFromRectT(a.rect.normalized());
                    // 8手柄命中检测(±6px)
                    static const int tol = 6;
                    const QPoint cs[8] = {
                        r.topLeft(), QPoint(r.center().x(), r.top()), r.topRight(),
                        QPoint(r.right(), r.center().y()), r.bottomRight(),
                        QPoint(r.center().x(), r.bottom()), r.bottomLeft(),
                        QPoint(r.left(), r.center().y())};
                    for (int hi = 0; hi < 8; ++hi) {
                        if (QRect(cs[hi].x()-tol, cs[hi].y()-tol, tol*2, tol*2).contains(event->pos())) {
                            m_anomalyDragIdx = aIdx;
                            m_anomalyDragMode = static_cast<DragMode>(DragTL + hi);
                            m_dragAnchor = event->pos();
                            m_dragRectStart = a.rect.normalized();
                            grabMouse();
                            return;
                        }
                    }
                    // 内部拖动移动
                    if (r.contains(event->pos())) {
                        m_anomalyDragIdx = aIdx;
                        m_anomalyDragMode = DragMove;
                        m_dragAnchor = event->pos();
                        m_dragRectStart = a.rect.normalized();
                        grabMouse();
                        return;
                    }
                } else if (a.poly.size() >= 3 && polyContains(a.poly, event->pos())) {
                    m_anomalyDragIdx = aIdx;
                    m_anomalyDragMode = DragMove;
                    m_dragAnchor = event->pos();
                    grabMouse();
                    return;
                }
            }
        }
    }

    if (event->button() == Qt::LeftButton && m_blocksVisible) {
        // 数据块交互优先: 命中块(活动块优先) > [保留]/[删除]标记 > 8手柄 > 块内拖动 > 十字线
        const int idx = hitBlock(event->pos());
        if (idx >= 0) {
            m_activeBlock = idx;   // 点击即激活该块
            bool onKeep = false, onDel = false;
            const DragMode dm = hitTest(event->pos(), idx, &onKeep, &onDel);
            if (onKeep) { emit editMarkKeepRequested(idx); return; }
            if (onDel)  { emit editMarkDeleteRequested(idx); return; }
            if (dm != DragNone && !m_hyperbolaTracking) {
                m_dragMode = dm;
                m_dragIdx = idx;
                m_dragAnchor = event->pos();
                m_dragRectStart = m_blocks[idx].rectT;
                grabMouse();   // 按下即拖动: 抓取鼠标保证拖出控件边界仍连续跟踪
                update();
                return;   // 编辑拖拽中不走十字线
            }
        }
    }

    if (event->button() == Qt::LeftButton) {
        m_crosshairPos = event->pos();
        m_showCrosshair = true;
        int clampedY = qBound(0, event->pos().y(), height() - 1);
        int origY = (height() > 1) ? clampedY * (m_originalSize.height() - 1) / (height() - 1) : 0;
        int clampedX = qBound(0, event->pos().x(), width() - 1);
        int origX = (width() > 1) ? clampedX * (m_originalSize.width() - 1) / (width() - 1) : 0;
        QPoint origPos(origX, origY);
        emit imageClicked(origPos);
        update();
    }
    QLabel::mousePressEvent(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    // v1.0.120: 多边形绘制光标跟踪
    if (m_polyDrawing && !(event->buttons() & Qt::LeftButton)) {
        m_polyCursor = event->pos();
        update();
        // 不return, 允许hover光标等继续处理
    }

    // v1.0.120: 编辑态异常 — 顶点拖动 / 整体移动 / 手柄调整大小
    if (m_anomalyDragIdx >= 0 && m_anomalyDragIdx < m_anomalies.size()
        && (event->buttons() & Qt::LeftButton)) {
        AnomalyMark &a = m_anomalies[m_anomalyDragIdx];
        if (m_anomalyVertexIdx >= 0 && m_anomalyVertexIdx < a.poly.size()) {
            // 多边形顶点拖动: 更新单个顶点位置
            a.poly[m_anomalyVertexIdx] = QPointF(widgetXToTrace(event->pos().x()),
                                                  widgetYToSample(event->pos().y()));
            update();
            return;
        }
        if (m_anomalyDragMode == DragMove) {
            const double dTrace = widgetXToTrace(event->pos().x())
                                - widgetXToTrace(qRound(m_dragAnchor.x()));
            const double dSamp = widgetYToSample(event->pos().y())
                               - widgetYToSample(qRound(m_dragAnchor.y()));
            a.rect.translate(dTrace, dSamp);
            for (QPointF &pt : a.poly) pt += QPointF(dTrace, dSamp);
            m_dragAnchor = event->pos();
        } else if (m_anomalyDragMode != DragNone && a.shape != 2) {
            // 手柄调整大小(圆/矩/文本)
            const double dTrace = widgetXToTrace(event->pos().x())
                                - widgetXToTrace(qRound(m_dragAnchor.x()));
            const double dSamp = widgetYToSample(event->pos().y())
                               - widgetYToSample(qRound(m_dragAnchor.y()));
            QRectF r = m_dragRectStart;
            switch (m_anomalyDragMode) {
            case DragTL: r.setLeft(r.left()+dTrace); r.setTop(r.top()+dSamp); break;
            case DragT:  r.setTop(r.top()+dSamp); break;
            case DragTR: r.setRight(r.right()+dTrace); r.setTop(r.top()+dSamp); break;
            case DragR:  r.setRight(r.right()+dTrace); break;
            case DragBR: r.setRight(r.right()+dTrace); r.setBottom(r.bottom()+dSamp); break;
            case DragB:  r.setBottom(r.bottom()+dSamp); break;
            case DragBL: r.setLeft(r.left()+dTrace); r.setBottom(r.bottom()+dSamp); break;
            case DragL:  r.setLeft(r.left()+dTrace); break;
            default: break;
            }
            const double maxT = qMax(0.0, (double)m_mapTraceCount - 1);
            const double maxS = qMax(0.0, (double)m_mapDrawRows - 1);
            double l = qBound(0.0, r.normalized().left(), maxT);
            double rr = qBound(0.0, r.normalized().right(), maxT);
            double t = qBound(0.0, r.normalized().top(), maxS);
            double b = qBound(0.0, r.normalized().bottom(), maxS);
            if (rr - l < 1.0) rr = l + 1.0;
            if (b - t < 1.0) b = t + 1.0;
            a.rect = QRectF(QPointF(l, t), QPointF(rr, b));
        }
        update();
        return;
    }

    // 数据块拖动/调整: 按锚点差更新 trace/sample 域, clampNoOverlap 防重叠实时夹取
    if (m_dragMode != DragNone && m_dragIdx >= 0 && m_dragIdx < m_blocks.size()
        && (event->buttons() & Qt::LeftButton)) {
        const double dTrace = widgetXToTrace(event->pos().x()) - widgetXToTrace(qRound(m_dragAnchor.x()));
        const double dSamp  = widgetYToSample(event->pos().y()) - widgetYToSample(qRound(m_dragAnchor.y()));
        QRectF r = m_dragRectStart;
        switch (m_dragMode) {
        case DragMove: r.translate(dTrace, dSamp); break;
        case DragTL: r.setLeft(r.left() + dTrace); r.setTop(r.top() + dSamp); break;
        case DragT:  r.setTop(r.top() + dSamp); break;
        case DragTR: r.setRight(r.right() + dTrace); r.setTop(r.top() + dSamp); break;
        case DragR:  r.setRight(r.right() + dTrace); break;
        case DragBR: r.setRight(r.right() + dTrace); r.setBottom(r.bottom() + dSamp); break;
        case DragB:  r.setBottom(r.bottom() + dSamp); break;
        case DragBL: r.setLeft(r.left() + dTrace); r.setBottom(r.bottom() + dSamp); break;
        case DragL:  r.setLeft(r.left() + dTrace); break;
        default: break;
        }
        m_blocks[m_dragIdx].rectT = clampNoOverlap(r, m_dragIdx);
        update();
        return;
    }

    // hover 光标(数据块可见且无按键)
    if (m_blocksVisible && !(event->buttons() & Qt::LeftButton)) {
        const int idx = hitBlock(event->pos());
        Qt::CursorShape cs = Qt::ArrowCursor;
        if (idx >= 0) {
            if (idx == m_activeBlock) {
                bool onKeep = false, onDel = false;
                const DragMode dm = hitTest(event->pos(), idx, &onKeep, &onDel);
                if (onKeep || onDel) cs = Qt::PointingHandCursor;
                else if (dm == DragMove) cs = Qt::SizeAllCursor;
                else switch (dm) {
                case DragTL: case DragBR: cs = Qt::SizeFDiagCursor; break;
                case DragTR: case DragBL: cs = Qt::SizeBDiagCursor; break;
                case DragT: case DragB:   cs = Qt::SizeVerCursor; break;
                case DragL: case DragR:   cs = Qt::SizeHorCursor; break;
                default: break;
                }
            } else {
                cs = Qt::PointingHandCursor;   // 非活动块: 点击激活
            }
        }
        setCursor(cs);
    }

    if (m_showCrosshair && !m_image.isNull() && (event->buttons() & Qt::LeftButton)) {
        m_crosshairPos = event->pos();
        int clampedY = qBound(0, event->pos().y(), height() - 1);
        int origY = (height() > 1) ? clampedY * (m_originalSize.height() - 1) / (height() - 1) : 0;
        int clampedX = qBound(0, event->pos().x(), width() - 1);
        int origX = (width() > 1) ? clampedX * (m_originalSize.width() - 1) / (width() - 1) : 0;
        QPoint origPos(origX, origY);
        emit imageClicked(origPos);
        update();
    }
    if (m_hyperbolaTracking && !m_image.isNull()) {
        m_hyperbolaApex = event->pos();
        m_showHyperbola = true;
        update();
    }
    QLabel::mouseMoveEvent(event);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_anomalyDragIdx >= 0 && m_anomalyDragIdx < m_anomalies.size()) {
            releaseMouse();
            const AnomalyMark &a = m_anomalies[m_anomalyDragIdx];
            const int idx = m_anomalyDragIdx;
            m_anomalyDragIdx = -1;
            m_anomalyDragMode = DragNone;
            m_anomalyVertexIdx = -1;
            // 同步回 MainWindow(tab->anomalies)
            emit anomalyMoved(idx, a.rect, a.poly);
            update();
            QLabel::mouseReleaseEvent(event);
            return;
        }
        if (m_dragMode != DragNone && m_dragIdx >= 0 && m_dragIdx < m_blocks.size()) {
            releaseMouse();   // 结束鼠标抓取
            const int idx = m_dragIdx;
            m_blocks[idx].rectT = m_blocks[idx].rectT.normalized();
            m_dragMode = DragNone;
            m_dragIdx = -1;
            emit editRectChanged(idx, m_blocks[idx].rectT);
            update();
            QLabel::mouseReleaseEvent(event);
            return;
        }
        m_showCrosshair = false;
        update();
    }
    QLabel::mouseReleaseEvent(event);
}

void ImageLabel::paintEvent(QPaintEvent *event)
{
    if (!m_image.isNull()) {
        QPainter p(this);
        p.drawImage(rect(), m_image);
    } else {
        QLabel::paintEvent(event);
    }

    if (m_showCrosshair && !m_image.isNull()) {
        QPainter painter(this);
        // 浅色背景(堆积图)用黑色十字,深色背景(B-SCAN雷达图)用白色十字
        QPen pen(m_crosshairDark ? Qt::black : Qt::white, 1);
        painter.setPen(pen);

        int x = m_crosshairPos.x();
        int y = m_crosshairPos.y();

        painter.drawLine(0, y, width(), y);
        painter.drawLine(x, 0, x, height());
    }

    if (m_showHyperbola && !m_image.isNull() && m_hypVelocity > 1e-6) {
        QPainter painter(this);
        QPen pen(QColor(0, 255, 0), 2);
        painter.setPen(pen);

        int w = width();
        int h = height();
        int imgW = m_originalSize.width();
        int imgH = m_originalSize.height();
        if (w < 2 || h < 2 || imgW < 1 || imgH < 1) return;

        // 鼠标 widget 坐标 -> 原始图像 (trace, sample) 坐标
        int apexTrace = m_hyperbolaApex.x() * (imgW - 1) / (w - 1);
        int apexSample = m_hyperbolaApex.y() * (imgH - 1) / (h - 1);

        // 双曲线开口宽度（左右各 W/2 道）
        int halfW = m_hypWidth / 2;

        // 在 apex 处的双程走时（相对于首波）
        double t0_ns = (apexSample - m_hypFirstWave) * m_hypTimePerSample;

        QVector<QPoint> pts;
        if (t0_ns > 0.0) {
            for (int dx = -halfW; dx <= halfW; ++dx) {
                int trace = apexTrace + dx;
                if (trace < 0 || trace >= imgW) continue;
                double dxMeters = dx * m_hypTraceSpacing;
                // 双曲线方程: t(dx) = sqrt(t0^2 + (dx/v_mig)^2)
                // v_mig 是等效偏移速度(已含发收双程因子)，业界商业GPR惯例
                double tNs = std::sqrt(t0_ns * t0_ns + std::pow(dxMeters / m_hypVelocity, 2.0));
                int sampleAtDx = static_cast<int>(m_hypFirstWave + tNs / m_hypTimePerSample + 0.5);
                if (sampleAtDx < 0 || sampleAtDx >= imgH) continue;
                // 反向映射回 widget 坐标
                int pxX = trace * (w - 1) / (imgW - 1);
                int pxY = sampleAtDx * (h - 1) / (imgH - 1);
                pts.append(QPoint(pxX, pxY));
            }
        }
        // 即便 t0 无效也画一个标记圆点（提示鼠标位置）
        if (pts.size() >= 2) {
            QPainterPath path;
            path.moveTo(pts[0]);
            for (int i = 1; i < pts.size(); ++i) path.lineTo(pts[i]);
            painter.drawPath(path);
        }
        painter.setBrush(QColor(0, 255, 0, 180));
        painter.setPen(QPen(QColor(0, 255, 0), 1));
        painter.drawEllipse(m_hyperbolaApex, 4, 4);
    }

    // v1.0.98/107 编辑模块覆盖层: 标记红虚线 + 多数据块矩形框
    if (!m_image.isNull() && (m_showMarkerOverlay || m_blocksVisible))
        drawEditOverlay();

    // v1.0.108 数据解译叠加: 层位曲线 + 异常标注 + 种子
    if (!m_image.isNull() && hasInterpOverlay())
        drawInterpOverlay();
}

// 编辑覆盖层绘制: 红虚线标记(+道号标签) → 矩形框(+8手柄+[保留][删除]pill)
void ImageLabel::drawEditOverlay()
{
    QPainter p(this);

    if (m_showMarkerOverlay) {
        QPen pen(QColor(0xb3, 0x27, 0x2d), 2, Qt::DashLine);
        p.setPen(pen);
        const QFont mono = MatIcon::monoFont(10);
        for (int t : m_markerTraces) {
            if (t < 0 || t >= m_mapTraceCount) continue;
            const int x = traceToWidgetX(t);
            if (x < 0 || x > width()) continue;
            p.drawLine(x, 0, x, height());
            // 顶部"道号 N"红底白字小标签
            const QString lbl = QString::fromUtf8("道号 %1").arg(t);
            p.setFont(mono);
            const int tw = QFontMetrics(mono).horizontalAdvance(lbl) + 10;
            QRect badge(QPoint(x - tw / 2, 2), QSize(tw, 15));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xb3, 0x27, 0x2d));
            p.drawRoundedRect(badge, 2, 2);
            p.setPen(QPen(Qt::white, 1));
            p.drawText(badge, Qt::AlignCenter, lbl);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
        }
    }

    if (m_blocksVisible) {
        QFont bf; bf.setPixelSize(11);
        p.setFont(bf);
        for (int i = 0; i < m_blocks.size(); ++i) {
            const QRect r = rectFromRectT(m_blocks[i].rectT);
            const bool active = (i == m_activeBlock);
            const int st = m_blocks[i].state;
            const QColor border = (st == 2) ? QColor(0xba, 0x1a, 0x1a) : QColor(0x00, 0x48, 0xaf);
            // 边框: 保留块=实线+淡填充, 删除块=红虚线, 未标记=蓝虚线(活动块带淡填充)
            if (st == 1) {
                p.setPen(QPen(border, 2));
                p.setBrush(QColor(0, 0x48, 0xaf, 26));
                p.drawRect(r);
            } else {
                p.setPen(QPen(border, 2, Qt::DashLine));
                p.setBrush(active ? QColor(0, 0x48, 0xaf, 13) : Qt::NoBrush);
                p.drawRect(r);
            }
            if (!active) continue;   // 手柄与按钮只画在活动块上

            // 8 个 8×8 白底蓝边手柄(4角+4边中点)
            const QPoint cs[8] = {
                r.topLeft(), QPoint(r.center().x(), r.top()), r.topRight(),
                QPoint(r.right(), r.center().y()), r.bottomRight(),
                QPoint(r.center().x(), r.bottom()), r.bottomLeft(),
                QPoint(r.left(), r.center().y())};
            p.setBrush(Qt::white);
            p.setPen(QPen(border, 1));
            for (const QPoint &c : cs)
                p.drawRect(QRect(c.x() - 4, c.y() - 4, 8, 8));

            // 框上方 [保留][删除] pill(当前标记态高亮: 保留=蓝底白字, 删除=红底白字)
            const QRect kb = keepButtonRect(r), db = deleteButtonRect(r);
            p.setPen(Qt::NoPen);
            p.setBrush(st == 1 ? QColor(0x00, 0x48, 0xaf) : QColor(0xff, 0xff, 0xff));
            p.drawRoundedRect(kb, 10, 10);
            p.setBrush(st == 2 ? QColor(0xba, 0x1a, 0x1a) : QColor(0xff, 0xff, 0xff));
            p.drawRoundedRect(db, 10, 10);
            p.setPen(st == 1 ? Qt::white : QColor(0x00, 0x48, 0xaf));
            p.drawText(kb, Qt::AlignCenter, QString::fromUtf8("保留"));
            p.setPen(st == 2 ? Qt::white : QColor(0xba, 0x1a, 0x1a));
            p.drawText(db, Qt::AlignCenter, QString::fromUtf8("删除"));
        }
    }
}

// v1.0.122: 回车确认编辑态异常(变实线)
void ImageLabel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const int idx = editingAnomalyIndex();
        if (idx >= 0) {
            m_anomalies[idx].editing = false;
            emit anomalyConfirmed(idx);
            update();
            return;
        }
    }
    QLabel::keyPressEvent(event);
}

void ImageLabel::contextMenuEvent(QContextMenuEvent *event)
{
    // v1.0.122: 右键优先检测异常 — 命中实线异常弹"编辑/取消"菜单(屏蔽原右键)
    for (int i = 0; i < m_anomalies.size(); ++i) {
        const AnomalyMark &a = m_anomalies[i];
        if (a.shape < 0 || a.editing) continue;   // 只检实线(非编辑态)
        const QPoint pos = event->pos();
        bool hit = false;
        if (a.shape == 2 && a.poly.size() >= 3) {
            hit = polyContains(a.poly, pos);
        } else if (a.shape >= 0 && a.shape <= 3 && a.shape != 2) {
            hit = rectFromRectT(a.rect.normalized()).contains(pos);
        }
        if (hit) {
            QMenu menu(this);
            menu.setStyleSheet(
                "QMenu { background: #ffffff; border: 1px solid #c3c6d6; border-radius: 4px; padding: 4px 0; }"
                "QMenu::item { padding: 6px 24px 6px 16px; color: #121c2a; font-size: 13px; }"
                "QMenu::item:selected { background: #dee9fc; }");
            QAction *editAct = menu.addAction(QString::fromUtf8("编辑"));
            menu.addSeparator();
            menu.addAction(QString::fromUtf8("取消"));
            QAction *sel = menu.exec(event->globalPos());
            if (sel == editAct) {
                // 通知 MainWindow: 选中该项+进入编辑态(列表自动高亮)
                emit anomalyEditRequested(i);
                setFocus();
            }
            return;
        }
    }

    // 原有右键菜单(增益/变换)
    QMenu menu(this);

    QMenu *gainMenu = menu.addMenu("1 增益");
    QList<float> gainValues = {60, 40, 30, 20, 12, 6, 3, 0, -6};
    QMap<QAction*, float> gainMap;
    bool isPreset = gainValues.contains(m_currentGainDb);
    for (float g : gainValues) {
        QString label = QString::number(static_cast<int>(g));
        if (g == m_currentGainDb) label += " \xE2\x97\x8F";
        QAction *act = gainMenu->addAction(label);
        gainMap[act] = g;
    }
    gainMenu->addSeparator();
    QString customLabel = "自定义";
    if (!isPreset) customLabel = "\xE2\x97\x8F 自定义";
    QAction *customAct = gainMenu->addAction(customLabel);

    QMenu *transformMenu = menu.addMenu("2 变换");
    QMap<QAction*, int> transformMap;
    QList<QPair<QString, int>> transforms = {{"无", 0}, {"绝对值", 1}, {"取反", 2}, {"频谱", 3}};
    for (auto &t : transforms) {
        QString label = t.first;
        if (t.second == m_transformMode) label += " \xE2\x97\x8F";
        QAction *act = transformMenu->addAction(label);
        transformMap[act] = t.second;
    }

    QAction *selected = menu.exec(event->globalPos());
    if (selected && gainMap.contains(selected)) {
        m_currentGainDb = gainMap[selected];
        emit gainSelected(gainMap[selected]);
    } else if (selected == customAct) {
        QDialog dlg(this);
        dlg.setWindowTitle("输入增益值");
        QHBoxLayout *layout = new QHBoxLayout(&dlg);

        QLineEdit *input = new QLineEdit(&dlg);
        input->setValidator(new QDoubleValidator(-999.0, 999.0, 2, &dlg));
        input->setText("0.00");
        input->setFixedWidth(100);

        QPushButton *okBtn = new QPushButton("确定", &dlg);

        layout->addWidget(input);
        layout->addWidget(okBtn);

        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

        if (dlg.exec() == QDialog::Accepted) {
            float val = input->text().toFloat();
            m_currentGainDb = val;
            emit gainSelected(val);
        }
    } else if (selected && transformMap.contains(selected)) {
        m_transformMode = transformMap[selected];
        emit transformSelected(m_transformMode);
    }
}

// --- HRulerWidget ---
HRulerWidget::HRulerWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(40);
    setMinimumWidth(100);
}

void HRulerWidget::setDataRange(int dataWidth)
{
    m_dataWidth = dataWidth;
    update();
}

void HRulerWidget::setOffset(int offset)
{
    m_offset = offset;
    update();
}

void HRulerWidget::setZoom(float zoom)
{
    if (zoom < 0.01f) zoom = 0.01f;
    m_hZoom = zoom;
    update();
}

void HRulerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(245, 245, 245));
    p.setPen(QPen(Qt::black, 1));
    p.setFont(QFont("Microsoft YaHei", 8));

    int h = height();
    int w = width();

    p.drawText(QRect(2, 2, 40, 16), Qt::AlignLeft | Qt::AlignTop,
               QString::fromUtf8("道号"));

    if (m_dataWidth <= 0 || m_hZoom <= 0.0f) return;

    p.drawLine(0, h - 1, w, h - 1);

    const float zoom = m_hZoom;

    // 显示像素 ↔ 道号换算: 道号 = (m_offset + x) / zoom
    double startTrace = m_offset / zoom;            // 左边缘对应的道号
    double endTrace   = (m_offset + w) / zoom;      // 右边缘对应的道号
    if (endTrace > m_dataWidth) endTrace = m_dataWidth;
    if (startTrace < 0) startTrace = 0;

    // 自适应刻度间隔: 主刻度间距约 100px, 副刻度 5 等分
    auto niceStep = [](double targetTraces) -> int {
        if (targetTraces < 1.0) return 1;
        double mag = std::pow(10.0, std::floor(std::log10(targetTraces)));
        double norm = targetTraces / mag;            // 1..10
        double step;
        if (norm < 1.5) step = 1;
        else if (norm < 3.5) step = 2;
        else if (norm < 7.5) step = 5;
        else step = 10;
        return qMax(1, int(step * mag));
    };
    int majorStep = niceStep(100.0 / zoom);          // ~100px 一格主刻度
    int minorStep = (majorStep >= 5) ? majorStep / 5 : 1;

    // 副刻度
    {
        int firstMinor = int(std::floor(startTrace / minorStep)) * minorStep;
        for (int val = firstMinor; val <= int(std::ceil(endTrace)); val += minorStep) {
            if (val % majorStep == 0) continue;       // 主刻度单独画
            int x = qRound(val * zoom - m_offset);
            if (x < -2 || x > w + 2) continue;
            p.drawLine(x, h - 5, x, h);
        }
    }

    // 主刻度 + 数字
    {
        int firstMajor = int(std::floor(startTrace / majorStep)) * majorStep;
        for (int val = firstMajor; val <= int(std::ceil(endTrace)); val += majorStep) {
            int x = qRound(val * zoom - m_offset);
            if (x < -30 || x > w + 30) continue;
            p.drawLine(x, h - 10, x, h);
            int textX = qMax(0, x - 30);
            p.drawText(textX, 14, 60, h - 24, Qt::AlignLeft,
                       QString::number(val));
        }
    }
}

// --- VRulerWidget ---
VRulerWidget::VRulerWidget(Direction dir, QWidget *parent)
    : QWidget(parent), m_direction(dir)
{
    setFixedWidth(60);
    setMinimumHeight(50);
}

void VRulerWidget::setRange(double minVal, double maxVal)
{
    m_minVal = minVal;
    m_maxVal = maxVal;
    update();
}

void VRulerWidget::setLabel(const QString &label)
{
    m_label = label;
    update();
}

void VRulerWidget::setImageHeight(int height)
{
    m_imageHeight = height;
}

void VRulerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(245, 245, 245));
    p.setPen(QPen(Qt::black, 1));
    p.setFont(QFont("Microsoft YaHei", 8));

    int w = width();
    int imgH = height();

    double range = m_maxVal - m_minVal;
    if (range <= 0 || imgH <= 0) return;

    // Draw label: each character horizontal, arranged vertically, "(ns)" "(m)" as one unit
    p.save();
    QFontMetrics fm(p.font());
    // Split label into display units: each char except "(...)" stays together
    QStringList units;
    int li = 0;
    while (li < m_label.length()) {
        if (m_label[li] == '(') {
            int j = m_label.indexOf(')', li);
            if (j >= 0) {
                units.append(m_label.mid(li, j - li + 1));
                li = j + 1;
            } else {
                units.append(m_label.mid(li, 1));
                li++;
            }
        } else {
            units.append(m_label.mid(li, 1));
            li++;
        }
    }
    int lineH = fm.height();
    int totalTextH = units.size() * lineH;
    int startY = (imgH - totalTextH) / 2 + fm.ascent();
    int offset = (m_direction == Left) ? -fm.horizontalAdvance(QString::fromUtf8("时")) : fm.horizontalAdvance(QString::fromUtf8("深"));
    for (int i = 0; i < units.size(); ++i) {
        int cw = fm.horizontalAdvance(units[i]);
        int cx = (w - cw) / 2 + offset;
        p.drawText(cx, startY + i * lineH, units[i]);
    }
    p.restore();

    if (m_direction == Left)
        p.drawLine(w - 1, 0, w - 1, imgH);
    else
        p.drawLine(0, 0, 0, imgH);

    if (m_direction == Left) {
        double majorInterval = niceInterval(range, 8);   // 自适应:按当前时间范围选合适刻度
        double minorInterval = majorInterval / 10.0;

        double firstMinor = ceil(m_minVal / minorInterval) * minorInterval;
        for (double val = firstMinor; val <= m_maxVal; val += minorInterval) {
            double fraction = (val - m_minVal) / range;
            int y = (int)(fraction * imgH);
            bool isMajor = qAbs(fmod(val, majorInterval)) < 1e-9
                        || qAbs(fmod(val, majorInterval) - majorInterval) < 1e-9;
            if (!isMajor)
                p.drawLine(w - 4, y, w, y);
        }

        double firstMajor = ceil(m_minVal / majorInterval) * majorInterval;
        for (double val = firstMajor; val <= m_maxVal; val += majorInterval) {
            double fraction = (val - m_minVal) / range;
            int y = (int)(fraction * imgH);
            p.drawLine(w - 8, y, w, y);
            int textY = (imgH > 16) ? qBound(0, y - 8, imgH - 16) : qMax(0, y - 8);
            p.drawText(QRect(20, textY, w - 30, 16),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(val, 'f', 2));
        }
    } else {
        double majorInterval = niceInterval(range, 8);   // 自适应:按当前深度范围选合适刻度
        double minorInterval = majorInterval / 10.0;

        double firstMinor = ceil(m_minVal / minorInterval) * minorInterval;
        for (double val = firstMinor; val <= m_maxVal; val += minorInterval) {
            double fraction = (val - m_minVal) / range;
            int y = (int)(fraction * imgH);
            bool isMajor = qAbs(fmod(val, majorInterval)) < 1e-9
                        || qAbs(fmod(val, majorInterval) - majorInterval) < 1e-9;
            if (!isMajor)
                p.drawLine(0, y, 4, y);
        }

        double firstMajor = ceil(m_minVal / majorInterval) * majorInterval;
        for (double val = firstMajor; val <= m_maxVal; val += majorInterval) {
            double fraction = (val - m_minVal) / range;
            int y = (int)(fraction * imgH);
            p.drawLine(0, y, 8, y);
            int textY = (imgH > 16) ? qBound(0, y - 8, imgH - 16) : qMax(0, y - 8);
            p.drawText(QRect(8, textY, w - 20, 16),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(val, 'f', 3));
        }
    }
}

// --- MainWindow ---

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , scrollArea(nullptr)
    , imageLabel(nullptr)
    , chartView(nullptr)
    , chartSeries(nullptr)
    , m_dataOffset(0)
    , m_pixelsPerRow(512)
    , m_gain(1.0f)
    , m_transformMode(0)
    , m_traceCount(0)
    , m_timeRange(20.0)
    , m_depthRange(1.25)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout *contentLayout = new QHBoxLayout();

    // --- Shared: gain tree ---
    gainTree = new QTreeWidget();
    gainTree->setColumnCount(2);
    gainTree->setHeaderHidden(true);
    gainTree->setRootIsDecorated(false);
    gainTree->setIndentation(20);
    gainTree->setColumnWidth(0, 174);
    gainTree->setColumnWidth(1, 80);
    gainTree->setAnimated(true);
    gainTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gainTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gainTree->setSelectionMode(QAbstractItemView::NoSelection);
    gainTree->setMaximumWidth(280);
    gainTree->setMinimumWidth(220);
    gainTree->header()->setStretchLastSection(true);
    gainTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    gainTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    gainTree->setStyleSheet(
        "QTreeWidget { border: 1px solid #a0a0a0; font-size: 12px; "
        "  gridline-color: #c0c0c0; show-decoration-selected: 0; }"
        "QTreeWidget::item { padding: 1px 2px; border-bottom: 1px solid #d0d0d0; }"
        "QTreeWidget::item:selected { background: transparent; }"
    );

    // Root: 增益调整
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(gainTree, QStringList() << "增益调整" << "");
    rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsEditable);
    rootItem->setExpanded(true);
    QFont rootFont = rootItem->font(0);
    rootFont.setBold(true);
    rootFont.setPointSize(11);
    rootItem->setFont(0, rootFont);
    rootItem->setBackground(0, QBrush(QColor("#e0e0e0")));
    rootItem->setBackground(1, QBrush(QColor("#e0e0e0")));

    // 通道数量 (固定值，不可编辑)
    QTreeWidgetItem *channelCountItem = new QTreeWidgetItem(rootItem, QStringList() << "通道数量" << "1");
    channelCountItem->setFlags(channelCountItem->flags() & ~Qt::ItemIsEditable);

    // 增益类型
    QTreeWidgetItem *gainTypeItem = new QTreeWidgetItem(rootItem, QStringList() << "增益类型" << "");
    gainTypeItem->setFlags(gainTypeItem->flags() & ~Qt::ItemIsEditable);
    QComboBox *gainTypeCombo = new QComboBox();
    m_gainTypeCombo = gainTypeCombo;
    gainTypeCombo->addItems(QStringList() << "自动" << "指数" << "线性" << "智能");
    gainTypeCombo->setCurrentIndex(0);
    gainTree->setItemWidget(gainTypeItem, 1, gainTypeCombo);

    // 通道参数 (expandable)
    QTreeWidgetItem *channelParamItem = new QTreeWidgetItem(rootItem, QStringList() << "通道参数" << "");
    channelParamItem->setFlags(channelParamItem->flags() & ~Qt::ItemIsEditable);
    channelParamItem->setExpanded(true);
    channelParamItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    QFont groupFont = channelParamItem->font(0);
    groupFont.setBold(true);
    channelParamItem->setFont(0, groupFont);

    // 点数
    QTreeWidgetItem *pointCountItem = new QTreeWidgetItem(channelParamItem, QStringList() << "点数" << "");
    pointCountItem->setFlags(pointCountItem->flags() & ~Qt::ItemIsEditable);
    QSpinBox *pointSpinBox = new QSpinBox();
    pointSpinBox->setRange(1, 16);
    pointSpinBox->setValue(1);
    gainTree->setItemWidget(pointCountItem, 1, pointSpinBox);

    // 增益 rows (1-16, dynamic)
    QVector<QTreeWidgetItem*> gainItems(17);
    m_gainSpinBoxes.resize(17);
    for (int i = 1; i <= 16; ++i) {
        gainItems[i] = new QTreeWidgetItem(channelParamItem, QStringList() << QString("增益%1").arg(i) << "");
        gainItems[i]->setFlags(gainItems[i]->flags() & ~Qt::ItemIsEditable);
        gainItems[i]->setHidden(i > 3);
        QDoubleSpinBox *dsb = new QDoubleSpinBox();
        dsb->setRange(-20.0, 60.0);
        dsb->setSingleStep(0.5);
        dsb->setValue(0.0);
        dsb->setDecimals(2);
        dsb->setSuffix(" dB");
        m_gainSpinBoxes[i] = dsb;
        gainTree->setItemWidget(gainItems[i], 1, dsb);
    }

    // 整体增益(db)
    QTreeWidgetItem *overallGainItem = new QTreeWidgetItem(channelParamItem, QStringList() << "整体增益(db)" << "");
    overallGainItem->setFlags(overallGainItem->flags() & ~Qt::ItemIsEditable);
    QDoubleSpinBox *overallGainSpinBox = new QDoubleSpinBox();
    overallGainSpinBox->setRange(-20.0, 60.0);
    overallGainSpinBox->setSingleStep(0.5);
    overallGainSpinBox->setValue(0.0);
    overallGainSpinBox->setDecimals(2);
    overallGainSpinBox->setSuffix(" dB");
    gainTree->setItemWidget(overallGainItem, 1, overallGainSpinBox);
    m_gainSpinBoxes[0] = overallGainSpinBox;

    // 水平时间常数(固定值，不可编辑)
    QTreeWidgetItem *scanConstItem = new QTreeWidgetItem(channelParamItem, QStringList() << "水平时间常数" << "1");
    scanConstItem->setFlags(scanConstItem->flags() & ~Qt::ItemIsEditable);

    // 采样点数 (expandable)
    QTreeWidgetItem *sampleItem = new QTreeWidgetItem(rootItem, QStringList() << "采样点数" << "");
    sampleItem->setFlags(sampleItem->flags() & ~Qt::ItemIsEditable);
    sampleItem->setExpanded(true);
    sampleItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    sampleItem->setFont(0, groupFont);

    // 开始 (固定值，不可编辑)
    QTreeWidgetItem *startItem = new QTreeWidgetItem(sampleItem, QStringList() << "开始" << "0");
    startItem->setFlags(startItem->flags() & ~Qt::ItemIsEditable);

    // 结束 (随文件头采样点数更新,默认 511)
    m_gainSampleEndItem = new QTreeWidgetItem(sampleItem, QStringList() << "结束" << "511");
    m_gainSampleEndItem->setFlags(m_gainSampleEndItem->flags() & ~Qt::ItemIsEditable);

    // Helper: compute gain range from spin boxes and apply to chartView
    auto updateGainRange = [this]() {
        if (!chartView) return;
        bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;

        float maxAbs = isLinear ? 10.0f : 6.0f;
        float baseStep = isLinear ? 10.0f : 6.0f;

        if (m_gainSpinBoxes[0]) {
            float val = static_cast<float>(qAbs(m_gainSpinBoxes[0]->value()));
            if (val > maxAbs) maxAbs = val;
        }
        for (int i = 1; i <= 16; ++i) {
            if (m_gainSpinBoxes[i] && !m_gainSpinBoxes[i]->isHidden()) {
                float val = static_cast<float>(qAbs(m_gainSpinBoxes[i]->value()));
                if (val > maxAbs) maxAbs = val;
            }
        }
        float n = std::ceil(maxAbs / baseStep);
        if (n < 1.0f) n = 1.0f;
        float range = baseStep * n;

        if (isLinear)
            chartView->setGainRange(0.0f, range);
        else
            chartView->setGainRange(-range, range);
    };

    // 点数 spinner → show/hide gain rows
    connect(pointSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this, gainItems, updateGainRange](int count) {
            bool isAuto = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 0;
            if (isAuto) {
                for (int i = 1; i <= 16; ++i)
                    gainItems[i]->setHidden(true);
            } else {
                for (int i = 1; i <= 16; ++i)
                    gainItems[i]->setHidden(i > count);
            }
            if (chartView) {
                chartView->setLineCount(count);
                updateGainRange();
            }
        });

    // 增益类型切换
    connect(gainTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this, gainItems, overallGainItem, pointSpinBox, updateGainRange](int index) {
            bool isAuto = (index == 0);
            bool isLinear = (index == 2);
            overallGainItem->setHidden(!isAuto);
            if (isAuto) {
                for (int i = 1; i <= 16; ++i)
                    gainItems[i]->setHidden(true);
            } else {
                int count = pointSpinBox->value();
                for (int i = 1; i <= 16; ++i)
                    gainItems[i]->setHidden(i > count);
            }

            // 线性模式：转换 dB → linear，改 suffix 和 range
            if (isLinear) {
                for (int i = 1; i <= 16; ++i) {
                    if (m_gainSpinBoxes[i]) {
                        m_gainSpinBoxes[i]->blockSignals(true);
                        double dbVal = m_gainSpinBoxes[i]->value();
                        double linearVal = std::pow(10.0, dbVal / 20.0);
                        m_gainSpinBoxes[i]->setRange(0.0, 1000.0);
                        m_gainSpinBoxes[i]->setValue(linearVal);
                        m_gainSpinBoxes[i]->setSuffix("");
                        m_gainSpinBoxes[i]->setSingleStep(0.1);
                        m_gainSpinBoxes[i]->blockSignals(false);
                    }
                }
                if (m_gainSpinBoxes[0]) {
                    m_gainSpinBoxes[0]->blockSignals(true);
                    double dbVal = m_gainSpinBoxes[0]->value();
                    double linearVal = std::pow(10.0, dbVal / 20.0);
                    m_gainSpinBoxes[0]->setRange(0.0, 1000.0);
                    m_gainSpinBoxes[0]->setValue(linearVal);
                    m_gainSpinBoxes[0]->setSuffix("");
                    m_gainSpinBoxes[0]->setSingleStep(0.1);
                    m_gainSpinBoxes[0]->blockSignals(false);
                }
                // 同步chart手柄到转换后的值
                if (chartView) {
                    int actual = (pointSpinBox->value() == 1) ? 2 : pointSpinBox->value();
                    for (int i = 0; i < actual; ++i) {
                        float val = (i < 16 && m_gainSpinBoxes[i + 1])
                                    ? static_cast<float>(m_gainSpinBoxes[i + 1]->value())
                                    : 1.0f;
                        chartView->setHandleX(i, val);
                    }
                }
            } else {
                // 非线性模式：恢复 dB suffix 和 range
                for (int i = 1; i <= 16; ++i) {
                    if (m_gainSpinBoxes[i]) {
                        m_gainSpinBoxes[i]->blockSignals(true);
                        m_gainSpinBoxes[i]->setRange(-20.0, 60.0);
                        m_gainSpinBoxes[i]->setSuffix(" dB");
                        m_gainSpinBoxes[i]->setSingleStep(0.5);
                        m_gainSpinBoxes[i]->blockSignals(false);
                    }
                }
                if (m_gainSpinBoxes[0]) {
                    m_gainSpinBoxes[0]->blockSignals(true);
                    m_gainSpinBoxes[0]->setRange(-20.0, 60.0);
                    m_gainSpinBoxes[0]->setSuffix(" dB");
                    m_gainSpinBoxes[0]->setSingleStep(0.5);
                    m_gainSpinBoxes[0]->blockSignals(false);
                }
            }

            updateGainRange();
        });

    // 默认增益类型为"自动"(index 0)，显示整体增益，隐藏增益#行
    overallGainItem->setHidden(false);
    for (int i = 1; i <= 16; ++i)
        gainItems[i]->setHidden(true);

    // 整体增益(db) → 自动模式：更新m_gain + chart所有handle同步
    connect(overallGainSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, pointSpinBox, updateGainRange](double dbValue) {
            float linearGain = std::pow(10.0f, static_cast<float>(dbValue) / 20.0f);
            m_gain = linearGain;
            if (m_currentTab) {
                m_currentTab->gain = m_gain;
            }
            // 自动模式：chart上所有handle同步到同一值
            if (chartView && m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 0) {
                int actual = (pointSpinBox->value() == 1) ? 2 : pointSpinBox->value();
                for (int j = 0; j < actual; ++j)
                    chartView->setHandleX(j, static_cast<float>(dbValue));
            }
            updateGainRange();
            updateChart(m_lastChartX);
        });

    // 每个增益N输入 → 更新chart中对应handle位置和gain range (仅非自动模式)
    for (int i = 1; i <= 16; ++i) {
        connect(m_gainSpinBoxes[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, i, updateGainRange](double val) {
                if (chartView) {
                    chartView->setHandleX(i - 1, static_cast<float>(val));
                    updateGainRange();
                }
            });
    }

    // --- Shared: welcome label ---
    welcomeLabel = new QLabel(this);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLabel->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "stop:0 #0a1929, stop:0.5 #14304f, stop:1 #1a4a7a);"
    );
    m_welcomePix = QPixmap(":/icons/resources/welcome.png");
    welcomeLabel->setPixmap(m_welcomePix);
    welcomeLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);  // 占满整个内容区
    // 保持比例铺满:updateWelcomePixmap() 按 KeepAspectRatio 缩放原图,居中显示
    // welcome 底部 4 个功能图标的热区 + 右上角功能说明(悬停显示)
    m_welcomeTips = {
        QString::fromUtf8("<div style='font-size:20pt;font-weight:bold;color:#ffffff;'>智能识别</div>"
                          "<div style='font-size:12pt;color:#b8d0e8;margin-top:6px;'>AI 自动识别雷达剖面中的空洞 / 异常体 / 管线等目标</div>"),
        QString::fromUtf8("<div style='font-size:20pt;font-weight:bold;color:#ffffff;'>高效处理</div>"
                          "<div style='font-size:12pt;color:#b8d0e8;margin-top:6px;'>一键完成零点校正、增益、数字滤波等批量数据处理</div>"),
        QString::fromUtf8("<div style='font-size:20pt;font-weight:bold;color:#ffffff;'>精确成像</div>"
                          "<div style='font-size:12pt;color:#b8d0e8;margin-top:6px;'>高分辨率 B-scan 成像,支持增益、调色板、堆积图</div>"),
        QString::fromUtf8("<div style='font-size:20pt;font-weight:bold;color:#ffffff;'>深度洞察</div>"
                          "<div style='font-size:12pt;color:#b8d0e8;margin-top:6px;'>道号 / 采样点 / 双程走时 / 深度 多维坐标分析,深度按介电常数换算</div>"),
    };
    for (int i = 0; i < 4; ++i) {
        QWidget *hs = new QWidget(welcomeLabel);
        hs->setStyleSheet("background: transparent;");
        hs->setCursor(Qt::PointingHandCursor);
        hs->installEventFilter(this);
        hs->show();
        m_welcomeHotspots.append(hs);
    }
    // 右上角功能说明(悬停时显示)
    m_welcomeTip = new QLabel(welcomeLabel);
    m_welcomeTip->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_welcomeTip->setWordWrap(true);
    m_welcomeTip->setStyleSheet("background: rgba(10,25,42,210); border-radius: 12px; padding: 14px;");
    m_welcomeTip->hide();
    // 悬停放大用:圆形放大标签(鼠标穿透,不抢热区焦点)+ 预切 4 个图标区域(原图底部图标位)
    m_welcomeZoom = new QLabel(welcomeLabel);
    m_welcomeZoom->setStyleSheet("background: transparent; border: none;");
    m_welcomeZoom->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_welcomeZoom->hide();
    {
        static const double ICON_CX[4] = {0.304, 0.444, 0.580, 0.714};  // 4 个图标中心(亮度检测实测)
        int iw0 = m_welcomePix.width(), ih0 = m_welcomePix.height();
        for (int i = 0; i < 4; ++i) {
            QRect r(int((ICON_CX[i] - 0.028) * iw0), int(0.86 * ih0), int(0.056 * iw0), int(0.105 * ih0));
            m_welcomeIconPix.append(m_welcomePix.copy(r));
        }
    }

    // --- Shared: document tab widget ---
    m_docTabWidget = new QTabWidget(this);
    m_docTabWidget->setTabsClosable(true);
    m_docTabWidget->setDocumentMode(true);
    m_docTabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { background: #eef4ff; padding: 6px 16px; border: 1px solid #d5dae8; min-width: 80px; color: #444855; }"
        "QTabBar::tab:selected { background: #ffffff; font-weight: bold; color: #004aae; }"
    );
    m_docTabWidget->tabBar()->installEventFilter(this);

    // Splitter wrapping tab groups for horizontal/vertical splits
    m_docSplitter = new QSplitter(Qt::Vertical, this);
    m_docSplitter->addWidget(m_docTabWidget);
    m_docSplitter->setChildrenCollapsible(false);
    m_tabGroups.append(m_docTabWidget);
    m_activeTabGroup = m_docTabWidget;

    // Left panel: stacked gain / zero-point pages
    m_leftPanel = new QDialog(this);
    m_leftPanel->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    QVBoxLayout *leftOuterLayout = new QVBoxLayout(m_leftPanel);
    leftOuterLayout->setContentsMargins(0, 0, 0, 0);
    leftOuterLayout->setSpacing(0);

    m_leftStack = new QStackedWidget;

    // --- Gain page ---
    m_gainPage = new QWidget;
    QVBoxLayout *gainPageLayout = new QVBoxLayout(m_gainPage);
    gainPageLayout->setContentsMargins(0, 0, 0, 0);
    gainPageLayout->setSpacing(4);
    gainPageLayout->addWidget(gainTree);

    QHBoxLayout *gainBtnLayout = new QHBoxLayout;
    m_btnApply = new QPushButton("应用");
    m_btnOK = new QPushButton("确定");
    m_btnCancel = new QPushButton("取消");
    m_btnApply->setEnabled(false);
    m_btnOK->setEnabled(false);
    m_btnCancel->setEnabled(false);
    gainBtnLayout->addWidget(m_btnApply);
    gainBtnLayout->addWidget(m_btnCancel);
    gainBtnLayout->addWidget(m_btnOK);
    gainPageLayout->addLayout(gainBtnLayout);

    connect(m_btnApply, &QPushButton::clicked, this, &MainWindow::applyGain);
    connect(m_btnOK, &QPushButton::clicked, this, &MainWindow::saveProcessedFile);
    connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_currentTab && m_currentTab->gainApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_btnApply->setText("应用");
            refreshImage();
        }
        if (chartView) chartView->setGainVisible(false);
        m_leftPanel->hide();
        syncAscanVisibility();   // 关闭编辑面板后按显示模式恢复波形列
    });

    // X 按钮关闭也隐藏增益handle
    connect(m_leftPanel, &QDialog::rejected, this, [this]() {
        if (chartView) chartView->setGainVisible(false);
        syncAscanVisibility();   // X 关闭面板后按显示模式恢复波形列
    });

    m_leftStack->addWidget(m_gainPage);

    // --- Zero-point page ---
    m_zeroPage = new QWidget;
    QVBoxLayout *zeroPageLayout = new QVBoxLayout(m_zeroPage);
    zeroPageLayout->setContentsMargins(0, 0, 0, 0);
    zeroPageLayout->setSpacing(4);

    QTreeWidget *zeroTree = new QTreeWidget();
    zeroTree->setHeaderHidden(true);
    zeroTree->setColumnCount(2);
    zeroTree->setRootIsDecorated(false);
    zeroTree->setIndentation(20);
    zeroTree->setAnimated(true);
    zeroTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    zeroTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    zeroTree->setSelectionMode(QAbstractItemView::NoSelection);
    zeroTree->setMaximumWidth(280);
    zeroTree->setMinimumWidth(220);
    zeroTree->header()->setStretchLastSection(true);
    zeroTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    zeroTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    zeroTree->setStyleSheet(
        "QTreeWidget { border: 1px solid #a0a0a0; font-size: 12px; "
        "  gridline-color: #c0c0c0; show-decoration-selected: 0; }"
        "QTreeWidget::item { padding: 1px 2px; border-bottom: 1px solid #d0d0d0; }"
        "QTreeWidget::item:selected { background: transparent; }"
    );

    QTreeWidgetItem *zeroRoot = new QTreeWidgetItem(zeroTree, QStringList() << "时间零点" << "");
    zeroRoot->setFlags(zeroRoot->flags() & ~Qt::ItemIsEditable);
    zeroRoot->setExpanded(true);
    zeroRoot->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    QFont zeroRootFont = zeroRoot->font(0);
    zeroRootFont.setBold(true);
    zeroRoot->setFont(0, zeroRootFont);

    // 时间通道
    QTreeWidgetItem *timeChItem = new QTreeWidgetItem(zeroRoot, QStringList() << "时间通道" << "");
    timeChItem->setFlags(timeChItem->flags() & ~Qt::ItemIsEditable);
    QSpinBox *timeChSpin = new QSpinBox();
    timeChSpin->setRange(1, 16);
    timeChSpin->setValue(1);
    zeroTree->setItemWidget(timeChItem, 1, timeChSpin);

    // 方法
    QTreeWidgetItem *methodItem = new QTreeWidgetItem(zeroRoot, QStringList() << "方法" << "");
    methodItem->setFlags(methodItem->flags() & ~Qt::ItemIsEditable);
    QComboBox *methodCombo = new QComboBox();
    methodCombo->addItems(QStringList() << "手动" << "自动");
    zeroTree->setItemWidget(methodItem, 1, methodCombo);

    // 通道参数
    QTreeWidgetItem *chParamItem = new QTreeWidgetItem(zeroRoot, QStringList() << "通道参数" << "");
    chParamItem->setFlags(chParamItem->flags() & ~Qt::ItemIsEditable);
    chParamItem->setExpanded(true);
    chParamItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    QFont chParamFont = chParamItem->font(0);
    chParamFont.setBold(true);
    chParamItem->setFont(0, chParamFont);

    // 显示通道
    QTreeWidgetItem *showChItem = new QTreeWidgetItem(chParamItem, QStringList() << "显示通道" << "");
    showChItem->setFlags(showChItem->flags() & ~Qt::ItemIsEditable);
    QComboBox *showChCombo = new QComboBox();
    showChCombo->addItems(QStringList() << "Yes" << "No");
    zeroTree->setItemWidget(showChItem, 1, showChCombo);

    // 偏移量(ns)
    QTreeWidgetItem *offsetItem = new QTreeWidgetItem(chParamItem, QStringList() << "偏移量(ns)" << "");
    offsetItem->setFlags(offsetItem->flags() & ~Qt::ItemIsEditable);
    QDoubleSpinBox *offsetSpin = new QDoubleSpinBox();
    offsetSpin->setRange(-1000.0, 1000.0);
    offsetSpin->setValue(0.0);
    offsetSpin->setDecimals(1);
    zeroTree->setItemWidget(offsetItem, 1, offsetSpin);
    m_zeroOffsetSpin = offsetSpin;

    // 时间位置零点(ns) — computed: 位置范围百分点 * 20
    QTreeWidgetItem *zeroPosItem = new QTreeWidgetItem(chParamItem, QStringList() << "时间位置零点(ns)" << "");
    zeroPosItem->setFlags(zeroPosItem->flags() & ~Qt::ItemIsEditable);
    QLabel *zeroPosLabel = new QLabel("0.0");
    zeroPosLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    zeroTree->setItemWidget(zeroPosItem, 1, zeroPosLabel);

    // 位置范围百分点
    QTreeWidgetItem *rangePctItem = new QTreeWidgetItem(chParamItem, QStringList() << "位置范围百分点" << "");
    rangePctItem->setFlags(rangePctItem->flags() & ~Qt::ItemIsEditable);
    QDoubleSpinBox *rangePctSpin = new QDoubleSpinBox();
    rangePctSpin->setRange(0.0, 100.0);
    rangePctSpin->setValue(10.0);
    rangePctSpin->setDecimals(1);
    zeroTree->setItemWidget(rangePctItem, 1, rangePctSpin);
    m_zeroRangePctSpin = rangePctSpin;

    // Initial display
    zeroPosLabel->setText(QString::number(-rangePctSpin->value() * 0.2, 'f', 1));

    // 位置范围百分比变化 → 更新时间位置零点 + 刷新chart
    connect(rangePctSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, zeroPosLabel](double val) {
            zeroPosLabel->setText(QString::number(-val * 0.2, 'f', 1));
            if (chartView && chartView->yScale() != 1.0f) {
                chartView->setZeroOffset(static_cast<float>(-val * 0.2));
                updateChart(m_lastChartX);
            }
        });

    zeroTree->expandAll();

    // 偏移量变化 → 刷新chart (insert zeros into data)
    connect(offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this](double) {
            if (chartView && chartView->yScale() != 1.0f)
                updateChart(m_lastChartX);
        });

    zeroPageLayout->addWidget(zeroTree);

    // Zero-point buttons (same style as gain buttons)
    QHBoxLayout *zeroBtnLayout = new QHBoxLayout;
    QPushButton *zeroBtnOK = new QPushButton("确定");
    QPushButton *zeroBtnCancel = new QPushButton("取消");
    QPushButton *zeroBtnApply = new QPushButton("应用");
    m_zeroBtnApply = zeroBtnApply;
    zeroBtnLayout->addWidget(zeroBtnOK);
    zeroBtnLayout->addWidget(zeroBtnCancel);
    zeroBtnLayout->addWidget(zeroBtnApply);
    zeroPageLayout->addLayout(zeroBtnLayout);

    connect(zeroBtnCancel, &QPushButton::clicked, this, [this]() {
        m_leftPanel->hide();
    });

    connect(zeroBtnOK, &QPushButton::clicked, this, [this]() {
        if (!requireOpenFile()) return;

        // 创建 proc 目录
        QFileInfo fi(m_currentTab->filePath);
        QString baseName = fi.completeBaseName();  // e.g. "1103_010"
        QString procDir = fi.absolutePath() + "/proc";
        QDir().mkpath(procDir);

        // 找到可用的文件名  P_#.DZT
        int N = 1;
        QString outPath;
        do {
            outPath = procDir + QString("/%1 P_%2.DZT").arg(baseName).arg(N++);
        } while (QFile::exists(outPath));

        // 复制原文件到新文件
        QFile srcFile(m_currentTab->filePath);
        QFile outFile(outPath);
        if (!srcFile.open(QIODevice::ReadOnly) || !outFile.open(QIODevice::ReadWrite)) {
            QMessageBox::warning(this, "Error", "Failed to save file.");
            return;
        }
        outFile.write(srcFile.readAll());
        srcFile.close();

        // 0. 写入编辑时间到 offset 36 (rhb_mdt, tagRFDate 4 bytes)
        // tagRFDate bitfield: sec2[4:0] min[10:5] hour[15:11] day[20:16] month[24:21] year[31:25]
        QDateTime now = QDateTime::currentDateTime();
        QDate d = now.date();
        QTime t = now.time();
        quint32 mdt = 0;
        mdt |= (t.second() / 2) & 0x1F;                    // sec2
        mdt |= (t.minute() & 0x3F) << 5;                   // min
        mdt |= (t.hour() & 0x1F) << 11;                    // hour
        mdt |= (d.day() & 0x1F) << 16;                     // day
        mdt |= (d.month() & 0xF) << 21;                    // month
        mdt |= ((d.year() - 1980) & 0x7F) << 25;           // year-1980
        outFile.seek(36);
        outFile.write(reinterpret_cast<const char*>(&mdt), 4);

        // 1. 写入时间位置零点到 offset 22 (rhf_position, 信号位置 ns)
        float zeroPosVal = m_zeroRangePctSpin ? static_cast<float>(-m_zeroRangePctSpin->value() * 0.2) : 0.0f;
        outFile.seek(22);
        outFile.write(reinterpret_cast<const char*>(&zeroPosVal), 4);

        // 2. 追加偏移量作为处理记录到 offset 128 尾部
        // 格式: {short typeCode, float value} 每条6字节
        outFile.seek(50);  // rh_nproc
        qint16 procSize;
        outFile.read(reinterpret_cast<char*>(&procSize), 2);
        quint16 nextIdx = 0;
        if (procSize > 0)
            nextIdx = procSize / 6;
        int writeOff = 128 + procSize;
        quint16 typeCode = (nextIdx << 8) | 0x4D;
        float offsetVal = m_zeroOffsetSpin ? static_cast<float>(m_zeroOffsetSpin->value()) : 0.0f;
        outFile.seek(writeOff);
        outFile.write(reinterpret_cast<const char*>(&typeCode), 2);
        outFile.write(reinterpret_cast<const char*>(&offsetVal), 4);

        // 更新 rh_nproc
        qint16 newSize = procSize + 6;
        outFile.seek(50);
        outFile.write(reinterpret_cast<const char*>(&newSize), 2);

        outFile.close();

        // 恢复原 tab 到未调节零点状态:调节结果已写入新文件,
        // 原文件(显示)应保持不变(与 RADAN 逻辑一致)。
        // 必须在 loadDZTFile/createTab 之前刷新,此时 m_currentTab 仍是原 tab。
        m_currentTab->zeroApplied = false;
        m_currentTab->zeroSkipRows = 0;
        if (m_zeroBtnApply) m_zeroBtnApply->setText(QString::fromUtf8("应用"));
        refreshImage();
        updateRulers();

        // 打开新文件作为新 tab
        QImage image = loadDZTFile(outPath);
        if (!image.isNull()) {
            createTab(outPath, image);
        }
    });

    connect(zeroBtnApply, &QPushButton::clicked, this, [this]() {
        if (!requireOpenFile()) return;
        if (m_currentTab->zeroApplied) {
            // 重设: restore original image, keep spinbox values
            m_currentTab->zeroApplied = false;
            m_currentTab->zeroSkipRows = 0;
            m_zeroBtnApply->setText("应用");
            refreshImage();
            updateRulers();
        } else {
            // 应用: skip first N rows based on 时间位置零点
            double rangePct = m_zeroRangePctSpin ? m_zeroRangePctSpin->value() : 0.0;
            double zeroOff = -rangePct * 0.2;  // e.g. -2.0
            int skip = qRound(512 * (-zeroOff) / 20.0);  // e.g. 512*2/20=51
            if (skip <= 0) return;
            m_currentTab->zeroApplied = true;
            m_currentTab->zeroSkipRows = skip;
            m_zeroBtnApply->setText("重设");
            refreshImage();
            updateRulers();
        }
    });

    m_leftStack->addWidget(m_zeroPage);

    leftOuterLayout->addWidget(m_leftStack);
    m_leftStack->setCurrentIndex(0);
    m_leftPanel->setMinimumSize(360, 600);
    m_leftPanel->resize(360, 600);
    contentLayout->addWidget(welcomeLabel, 1);
    contentLayout->addWidget(m_docSplitter, 1);

    // Initially show welcome, hide others
    m_leftPanel->hide();
    m_docSplitter->hide();
    welcomeLabel->show();

    // v1.0.87: 右侧 350px 文件头属性栏(默认隐藏,主页"文件头"按钮开关)
    createHeaderPanel();
    contentLayout->addWidget(m_headerPanel);

    // v1.0.98: 右侧 256px 编辑属性面板(数据块/横向缩放两页, 与文件头栏互斥, 默认隐藏)
    createEditPanel();
    contentLayout->addWidget(m_editPanel);
    m_editPanel->hide();

    mainLayout->addLayout(contentLayout);

    // v1.0.98: 底部标记面板(编辑标记开关, 标记表+缩略图, 默认隐藏)
    createMarkerPanel();
    mainLayout->addWidget(m_markerPanel);

    // v1.0.108: 右侧 320px 解译与管理面板(数据解译模块, 默认隐藏)
    createInterpPanel();
    contentLayout->addWidget(m_interpPanel);
    m_interpPanel->hide();

    // --- 状态栏 (v1.0.87 28px,按设计稿: 左 ●就绪 | 右 道号/深度 等宽 + 进度条) ---
    m_progressBar = new QProgressBar(this);
    m_net = new QNetworkAccessManager(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(18);
    m_progressBar->setFixedWidth(220);
    m_progressBar->hide();

    QWidget *statusBar = new QWidget(this);
    statusBar->setObjectName("gprStatusBar");
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet("#gprStatusBar { background: #d9e3f6; }");
    QHBoxLayout *buttonLayout = new QHBoxLayout(statusBar);
    buttonLayout->setContentsMargins(12, 0, 12, 0);
    buttonLayout->setSpacing(12);

    QFrame *statusDot = new QFrame(statusBar);
    statusDot->setFixedSize(8, 8);
    statusDot->setStyleSheet("background: #0048af; border-radius: 4px; border: none;");
    buttonLayout->addWidget(statusDot);
    QLabel *statusLabel = new QLabel(QString::fromUtf8("就绪"), statusBar);
    statusLabel->setStyleSheet("color: #121c2a; font-size: 12px; border: none; background: transparent;");
    if (MatIcon::ready()) statusLabel->setFont(MatIcon::monoFont(12));
    buttonLayout->addWidget(statusLabel);

    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_progressBar);   // 升级/AI 推理进度

    coordinateLabel = new QLabel(QString::fromUtf8("道号: -  深度: -"), statusBar);
    coordinateLabel->setStyleSheet("color: #121c2a; font-size: 12px; border: none;"
                                   " border-left: 1px solid #c3c6d6; padding-left: 12px; background: transparent;");
    if (MatIcon::ready()) coordinateLabel->setFont(MatIcon::monoFont(12));
    buttonLayout->addWidget(coordinateLabel);

    // 状态栏顶边线: 实体色条(QSS border 不可靠)
    QWidget *sbTopLine = new QWidget(this);
    sbTopLine->setFixedHeight(1);
    sbTopLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sbTopLine->setStyleSheet("background: #c3c6d6;");
    mainLayout->addWidget(sbTopLine);
    mainLayout->addWidget(statusBar);

    setCentralWidget(centralWidget);

    // 文件切换"向下三角"按钮:浮在文档区右上角(整个窗体最右边,与TAB平行)。
    // 用 corner widget 会随某个分组固定,故改为覆盖层,resize/分组变化时重新定位。
    {
        QToolButton *btnSwitch = new QToolButton(centralWidget);
        const int isz = 16;
        QPixmap pix(isz, isz); pix.fill(Qt::transparent);
        QPainter pt(&pix); pt.setRenderHint(QPainter::Antialiasing, true);
        pt.setBrush(QColor("#333333")); pt.setPen(Qt::NoPen);
        QPolygon tri;
        tri << QPoint(isz * 2 / 8, isz * 3 / 8)
            << QPoint(isz * 6 / 8, isz * 3 / 8)
            << QPoint(isz / 2, isz * 6 / 8);
        pt.drawPolygon(tri); pt.end();
        btnSwitch->setIcon(QIcon(pix));
        btnSwitch->setIconSize(QSize(16, 16));
        btnSwitch->setToolTip(QString::fromUtf8("切换文件(显示所有已加载文件)"));
        btnSwitch->setAutoRaise(true);
        btnSwitch->setCursor(Qt::ArrowCursor);
        btnSwitch->setStyleSheet("QToolButton { border: none; padding: 6px 8px; background: transparent; }"
                                 "QToolButton:hover { background: #dce7f5; border-radius: 3px; }");
        btnSwitch->hide();   // 无文件时隐藏,由 repositionSwitchButton() 控制显隐
        m_btnSwitchFile = btnSwitch;
        connect(btnSwitch, &QToolButton::clicked, this, &MainWindow::showFileSwitchDropdown);
    }
    // 文档区尺寸/可见性变化时重定位三角按钮
    if (m_docSplitter) m_docSplitter->installEventFilter(this);

    setWindowTitle("劳雷");
    setAcceptDrops(true);

    // 自定义标题栏 + 无边框窗口 (v1.0.87: TopBar 40px 品牌栏, 严格按 主页-文件头.png)
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    m_topBar = new TopBar(this);
    setMenuWidget(m_topBar);

    resize(1440, 840);

    createMenuBar();

    // 渲染自检: 布局稳定后保存顶栏/整窗/编辑页ribbon离屏渲染图(QWidget::grab, 不受DPI/截屏干扰)
    QTimer::singleShot(800, this, [this]() {
        const QString dir = QCoreApplication::applicationDirPath();
        if (m_topBar) m_topBar->grab().save(dir + "/topbar_render.png");
        grab().save(dir + "/window_render.png");
        if (ribbonTab) {
            const int cur = ribbonTab->currentIndex();
            ribbonTab->setCurrentIndex(1);   // 编辑页
            QTimer::singleShot(300, this, [this, cur]() {
                ribbonTab->grab().save(QCoreApplication::applicationDirPath() + "/ribbon_edit_render.png");
                grab().save(QCoreApplication::applicationDirPath() + "/editpage_render.png");   // 含底部标记面板+缩略图
                if (m_markerPanel && m_markerPanel->isVisible())
                    m_markerPanel->grab().save(QCoreApplication::applicationDirPath() + "/markerpanel_render.png");
                // 数据解译页渲染
                ribbonTab->setCurrentIndex(3);
                QTimer::singleShot(300, this, [this, cur]() {
                    grab().save(QCoreApplication::applicationDirPath() + "/interppage_render.png");
                    if (m_interpPanel && m_interpPanel->isVisible())
                        m_interpPanel->grab().save(QCoreApplication::applicationDirPath()
                                                  + "/interppanel_render.png");
                    ribbonTab->setCurrentIndex(cur);
                });
            });
        }
    });

    // 顶栏 5 模块标签 ↔ ribbon 页双向联动(程序化 setChecked 不发 idClicked, 无环)
    connect(m_topBar, &TopBar::moduleChanged, ribbonTab, &QTabWidget::setCurrentIndex);
    connect(ribbonTab, &QTabWidget::currentChanged, m_topBar, &TopBar::setModuleIndex);
    // v1.0.100: 进入编辑页默认激活"编辑标记"; 切走自动收起编辑工具面板
    connect(ribbonTab, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 1) {   // 1 = 编辑页
            if (!m_tabs.isEmpty()
                && (!m_btnEditMarker || !m_btnEditMarker->isChecked())
                && (!m_btnEditBlock || !m_btnEditBlock->isChecked()))
                m_btnEditMarker->setChecked(true);
            syncEditUiState();
            // 渲染自检: 运行中进入编辑页且面板可见时抓取(供布局核查)
            QTimer::singleShot(400, this, [this]() {
                if (m_markerPanel && m_markerPanel->isVisible())
                    m_markerPanel->grab().save(QCoreApplication::applicationDirPath()
                                               + "/markerpanel_render.png");
            });
            return;
        }
        if (idx == 3) {   // 3 = 数据解译页
            syncInterpUiState();
            return;
        }
        bool any = false;
        for (auto *b : { m_btnEditMarker, m_btnEditBlock, m_btnHZoom })
            if (b && b->isChecked()) { b->setChecked(false); any = true; }
        if (any) syncEditUiState();
        syncInterpUiState();   // 离开数据解译页也收起其面板
    });
    // 品牌下拉菜单
    connect(m_topBar, &TopBar::openFileRequested, this, &MainWindow::onOpenFile);
    connect(m_topBar, &TopBar::closeFileRequested, this, [this]() {
        if (!m_tabs.isEmpty()) {
            int idx = m_docTabWidget->currentIndex();
            if (idx >= 0) closeTab(idx);
        }
    });
    connect(m_topBar, &TopBar::saveFileRequested, this, [this]() {
        if (m_currentTab) saveProcessedFile();
    });
    // 齿轮菜单: 关于 / 检查升级
    connect(m_topBar, &TopBar::aboutRequested, this, &MainWindow::showAbout);
    connect(m_topBar, &TopBar::upgradeRequested, this, &MainWindow::showUpgrade);
    // 帮助: 帮助文档
    connect(m_topBar, &TopBar::helpRequested, this, [this]() {
        QMessageBox::information(this, QString::fromUtf8("帮助"), QString::fromUtf8(
            "基本操作指引:\n"
            "1. 打开数据: 左上角\"劳雷▾\"菜单或主页\"打开\"按钮, 选择 DZT/DZX 文件\n"
            "2. 图像显示: 主页-图像显示组 切换 线扫描 / 线扫描+波形 / 波列图\n"
            "3. 色彩渲染: 主页-色彩渲染组, 彩虹色调色板与线性变换表叠加生效(RADAN规律)\n"
            "4. 文件头: 主页\"文件头\"按钮 在右侧栏查看当前文件元数据\n"
            "5. 编辑: \"编辑\"标签 — 编辑标记(底部标记表+缩略图, 标记存入DZX文件, 重开仍在);\n"
            "          编辑数据块(多矩形框+保留标记, 确认裁剪为选区); 横向缩放(右侧缩放条1-10x)\n"
            "6. 数据解译: \"数据解译\"标签 — 手动追踪(选层后图上点击连线)/自动追踪(拾取参考点+开始,\n"
            "          峰值跟随); 异常标注(圆形/矩形/闭合多边形/文本批注); 层位与异常存入DZX文件\n"
            "7. 数据处理: \"数据处理\"标签提供 零点调节/滤波/增益/一键处理/批处理\n"
            "8. 多文件: 可同时打开多个文件, 文档区标签页切换, 右上角三角按钮快速切换\n"
            "9. 关于与升级: 顶栏右上角齿轮菜单"));
    });
    // 账号: 账号信息
    connect(m_topBar, &TopBar::accountRequested, this, [this]() {
        QMessageBox::information(this, QString::fromUtf8("账号信息"),
            QString::fromUtf8("劳雷探地雷达数据处理软件\n版本: v%1\n\n账号登录与授权管理将在后续版本提供。")
                .arg(APP_VERSION));
    });
    loadLUT(12);
    connect(m_docTabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_docTabWidget, &QTabWidget::currentChanged, this, &MainWindow::switchToTab);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_tabs);
}

// (v1.0.87 旧 CustomTitleBar 已删除, 由 TopBar 替代 — 严格按 主页-文件头.png)

// --- Tab management ---

namespace {
const char *activeGroupSS =
    "QTabWidget::pane { border: none; }"
    "QTabBar::tab { background: #e0e0e0; padding: 6px 16px; border: 1px solid #c0c0c0; min-width: 80px; }"
    "QTabBar::tab:selected { background: #ffffff; font-weight: bold; }";
const char *inactiveGroupSS =
    "QTabWidget::pane { border: none; }"
    "QTabBar::tab { background: #e0e0e0; padding: 6px 16px; border: 1px solid #c0c0c0; min-width: 80px; }"
    "QTabBar::tab:selected { background: #ffffff; }";
}

static void updateGroupStyles(QTabWidget *activeGroup, const QVector<QTabWidget*> &groups)
{
    for (auto *grp : groups)
        grp->setStyleSheet(grp == activeGroup ? activeGroupSS : inactiveGroupSS);
}

TabData* MainWindow::createTab(const QString &filePath, const QImage &image)
{
    TabData *tab = new TabData();
    tab->filePath = filePath;
    tab->rawData = m_rawData;
    tab->dataOffset = m_dataOffset;
    tab->pixelsPerRow = m_pixelsPerRow;
    tab->gain = m_gain;
    tab->transformMode = m_transformMode;
    tab->traceCount = m_traceCount;
    tab->timeRange = m_timeRange;
    tab->depthRange = m_depthRange;
    tab->signalPosition = m_signalPos;
    tab->hZoom = 1.0f;           // 新文件默认无缩放
    tab->header = m_header;
    tab->nsamp = m_nsamp;
    tab->headerRange = m_headerRange;
    tab->epsr = m_epsr;

    // Page widget
    tab->page = new QWidget();
    QHBoxLayout *pageLayout = new QHBoxLayout(tab->page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    // ScrollArea + ImageLabel
    tab->scrollArea = new QScrollArea();
    tab->scrollArea->setWidgetResizable(false);
    tab->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tab->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    tab->imageLabel = new ImageLabel();
    tab->scrollArea->setWidget(tab->imageLabel);
    tab->imageLabel->setImage(image);

    // Chart
    tab->chartView = new CustomChartView();
    tab->chartView->setRenderHint(QPainter::Antialiasing);
    tab->chartView->setStyleSheet("border: none; background-color: transparent;");
    tab->chartView->setMinimumWidth(200);
    tab->chartView->setMaximumWidth(300);

    tab->chartSeries = new QLineSeries();
    tab->chartSeries->setPen(QPen(Qt::black, 1));
    tab->chartView->chart()->addSeries(tab->chartSeries);
    tab->chartView->chart()->legend()->hide();
    tab->chartView->setLineSeries(tab->chartSeries);

    QValueAxis *axisX = new QValueAxis();
    axisX->setRange(-100, 100);
    axisX->setLabelFormat("%d");
    axisX->setTickCount(5);

    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, m_pixelsPerRow - 1);     // 每道波形 Y 轴 0..nsamp-1(随文件头采样点数)
    axisY->setTickCount(6);
    axisY->setLabelFormat("%d");
    axisY->setReverse(true);

    tab->chartView->chart()->setAxisX(axisX, tab->chartSeries);
    tab->chartView->chart()->setAxisY(axisY, tab->chartSeries);
    tab->chartView->chart()->setAnimationOptions(QChart::NoAnimation);
    tab->chartView->setVisible(m_showAscan);   // 新tab按当前显示模式(线扫描=仅B-SCAN)

    // Drag handle → update spinbox (auto mode: sync all handles)
    connect(tab->chartView, &CustomChartView::gainChanged, this, [this, tab](int idx, float val) {
        bool isAuto = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 0;
        // pointCount=1 → internally 2 handles, always sync both
        bool isSinglePoint = (tab->chartView && tab->chartView->lineCount() == 2);

        if (isAuto) {
            // 同步整体增益spinbox
            if (m_gainSpinBoxes[0]) {
                m_gainSpinBoxes[0]->blockSignals(true);
                m_gainSpinBoxes[0]->setValue(static_cast<double>(val));
                m_gainSpinBoxes[0]->blockSignals(false);
            }
            // 同步m_gain
            float linearGain = std::pow(10.0f, val / 20.0f);
            m_gain = linearGain;
            if (m_currentTab)
                m_currentTab->gain = m_gain;
            // 同步所有handle
            if (chartView) {
                for (int j = 0; j < 16; ++j)
                    chartView->setHandleX(j, val);
            }
        } else if (isSinglePoint) {
            // 点数为1：同步两个handle
            if (idx >= 0 && idx < 16 && m_gainSpinBoxes[idx + 1]) {
                m_gainSpinBoxes[idx + 1]->blockSignals(true);
                m_gainSpinBoxes[idx + 1]->setValue(static_cast<double>(val));
                m_gainSpinBoxes[idx + 1]->blockSignals(false);
            }
            if (chartView) {
                chartView->setHandleX(0, val);
                chartView->setHandleX(1, val);
            }
        } else {
            if (idx >= 0 && idx < 16 && m_gainSpinBoxes[idx + 1]) {
                m_gainSpinBoxes[idx + 1]->blockSignals(true);
                m_gainSpinBoxes[idx + 1]->setValue(static_cast<double>(val));
                m_gainSpinBoxes[idx + 1]->blockSignals(false);
            }
        }
        // Auto-expand gain range from ALL spinbox max value
        if (chartView) {
            bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
            float baseStep = isLinear ? 10.0f : 6.0f;
            float maxAbs = baseStep;
            if (m_gainSpinBoxes[0]) {
                float v = static_cast<float>(qAbs(m_gainSpinBoxes[0]->value()));
                if (v > maxAbs) maxAbs = v;
            }
            for (int i = 1; i <= 16; ++i) {
                if (m_gainSpinBoxes[i] && !m_gainSpinBoxes[i]->isHidden()) {
                    float v = static_cast<float>(qAbs(m_gainSpinBoxes[i]->value()));
                    if (v > maxAbs) maxAbs = v;
                }
            }
            float n = std::ceil(maxAbs / baseStep);
            if (n < 1.0f) n = 1.0f;
            float range = baseStep * n;
            if (isLinear)
                chartView->setGainRange(0.0f, range);
            else
                chartView->setGainRange(-range, range);
        }
        // Trigger QChart to recalculate plotArea for new label widths
        if (m_oneClickChart) {
            QMargins mg = m_oneClickChart->margins();
            m_oneClickChart->setMargins(QMargins(mg.left(), mg.top() + 1, mg.right(), mg.bottom()));
            m_oneClickChart->setMargins(mg);
        }
        updateChart(m_lastChartX);
    });

    // Rulers
    tab->topRuler = new HRulerWidget();
    tab->leftRuler = new VRulerWidget(VRulerWidget::Left);
    tab->rightRuler = new VRulerWidget(VRulerWidget::Right);
    tab->leftRuler->setLabel(QString::fromUtf8("时间标尺(ns)"));
    tab->rightRuler->setLabel(QString::fromUtf8("深度标尺(m)"));

    // Corners
    tab->topLeftCorner = new QWidget();
    tab->topLeftCorner->setFixedSize(60, 40);
    tab->topLeftCorner->setStyleSheet("background: #f5f5f5;");
    tab->topRightCorner = new QWidget();
    tab->topRightCorner->setFixedSize(60, 40);
    tab->topRightCorner->setStyleSheet("background: #f5f5f5;");

    // Image grid: rulers + scrollArea + extHScrollBar
    tab->imageGrid = new QGridLayout();
    tab->imageGrid->setSpacing(0);
    tab->imageGrid->setContentsMargins(0, 0, 0, 0);
    tab->imageGrid->addWidget(tab->topLeftCorner, 0, 0);
    tab->imageGrid->addWidget(tab->topRuler, 0, 1);
    tab->imageGrid->addWidget(tab->topRightCorner, 0, 2);
    tab->imageGrid->addWidget(tab->leftRuler, 1, 0);
    tab->imageGrid->addWidget(tab->scrollArea, 1, 1);
    tab->imageGrid->addWidget(tab->rightRuler, 1, 2);
    tab->imageGrid->setColumnStretch(1, 1);
    tab->imageGrid->setRowStretch(1, 1);

    // External horizontal scrollbar
    tab->extHScrollBar = new QScrollBar(Qt::Horizontal);
    tab->imageGrid->addWidget(tab->extHScrollBar, 2, 1);

    // Monitor viewport resize to auto-adjust imageLabel height
    tab->scrollArea->viewport()->installEventFilter(this);

    // v1.0.98: 滚动联动缩略图视口框(仅当前 tab)
    connect(tab->extHScrollBar, &QScrollBar::valueChanged, this, [this, tab](int) {
        if (m_currentTab == tab && m_markerThumb) updateMarkerThumb();
    });

    // v1.0.107: 数据块交互(拖动结束更新块几何; [保留]/[删除]仅标记状态, 确认裁剪才动数据)
    connect(tab->imageLabel, &ImageLabel::editRectChanged, this,
            [this, tab](int idx, const QRectF &r) {
        if (m_currentTab == tab && idx >= 0 && idx < tab->editBlocks.size()) {
            tab->editBlocks[idx].rectT = r;
            syncEditBlocksToView();   // 缩略图同步
            refreshSelectionInfo();
        }
    });
    connect(tab->imageLabel, &ImageLabel::editMarkKeepRequested, this,
            [this, tab](int idx) { if (m_currentTab == tab) markEditBlockKeep(idx); });
    connect(tab->imageLabel, &ImageLabel::editMarkDeleteRequested, this,
            [this, tab](int idx) { if (m_currentTab == tab) markEditBlockDelete(idx); });
    // v1.0.120: 异常拖动/调整同步回tab(BUG1: 位置不丢)
    connect(tab->imageLabel, &ImageLabel::anomalyMoved, this,
            [this, tab](int idx, const QRectF &rect, const QVector<QPointF> &poly) {
        if (m_currentTab != tab || idx < 0 || idx >= tab->anomalies.size()) return;
        tab->anomalies[idx].rect = rect;
        tab->anomalies[idx].poly = poly;
    });
    // v1.0.120: 多边形闭合
    connect(tab->imageLabel, &ImageLabel::anomalyPolyDone, this,
            [this, tab](int idx, const QVector<QPointF> &poly) {
        if (m_currentTab != tab || idx < 0 || idx >= tab->anomalies.size()) return;
        tab->anomalies[idx].poly = poly;
        tab->anomalies[idx].editing = false;   // 闭合即确认(实线)
        if (m_annoGroup) {
            QAbstractButton *btn = m_annoGroup->checkedButton();
            if (btn) btn->setChecked(false);   // 按钮自动取消
        }
        syncInterpOverlays();
        refreshAnomalyList();
    });
    // v1.0.120: 多边形未闭合被取消(丢弃)
    connect(tab->imageLabel, &ImageLabel::anomalyPolyAborted, this, [this, tab]() {
        if (m_currentTab != tab) return;
        for (int k = tab->anomalies.size() - 1; k >= 0; --k) {
            if (tab->anomalies[k].shape == 2 && tab->anomalies[k].poly.size() < 3) {
                tab->anomalies[k].shape = -1;
                tab->anomalies[k].poly.clear();
                tab->anomalies[k].rect = QRectF();
            }
        }
        syncInterpOverlays();
        refreshAnomalyList();
    });
    // v1.0.122: Enter/点击外部 确认异常(实线) — 同步editing=false到tab + 按钮弹回
    connect(tab->imageLabel, &ImageLabel::anomalyConfirmed, this,
            [this, tab](int idx) {
        if (m_currentTab != tab || idx < 0 || idx >= tab->anomalies.size()) return;
        tab->anomalies[idx].editing = false;
        // 形状按钮弹回(确认后不再选中)
        if (m_annoGroup) {
            QAbstractButton *btn = m_annoGroup->checkedButton();
            if (btn) btn->setChecked(false);
        }
        syncInterpOverlays();
    });
    // v1.0.129: 点击图上异常 → 列表自动选中 + 菜单同步 + ImageLabel选中更新
    connect(tab->imageLabel, &ImageLabel::anomalyClickedOnImage, this,
            [this, tab](int idx) {
        if (m_currentTab != tab || idx < 0 || idx >= tab->anomalies.size()) return;
        m_selectedAnomaly = idx;
        refreshAnomalyList();
        syncInterpOverlays();   // 关键: 更新ImageLabel选中(半透明+粗边框)
        if (m_annoGroup) {
            const int sh = tab->anomalies[idx].shape;
            if (sh >= 0) {
                QAbstractButton *btn = m_annoGroup->button(sh);
                if (btn) btn->setChecked(true);
            } else {
                QAbstractButton *btn = m_annoGroup->checkedButton();
                if (btn) btn->setChecked(false);
            }
        }
    });
    // v1.0.125: 右键"编辑" → 列表自动选中该项 + 进入编辑态
    connect(tab->imageLabel, &ImageLabel::anomalyEditRequested, this,
            [this, tab](int idx) {
        if (m_currentTab != tab || idx < 0 || idx >= tab->anomalies.size()) return;
        // 确认其他编辑态
        for (int k = 0; k < tab->anomalies.size(); ++k)
            if (k != idx) tab->anomalies[k].editing = false;
        tab->anomalies[idx].editing = true;
        m_selectedAnomaly = idx;   // 列表自动切换到当前异常
        refreshAnomalyList();
        syncInterpOverlays();
    });

    // Page layout: imageGrid + chartView
    pageLayout->addLayout(tab->imageGrid, 1);
    pageLayout->addWidget(tab->chartView);

    // Per-tab signal connections
    connect(tab->imageLabel, &ImageLabel::imageClicked, this, [this, tab](const QPoint &pos) {
        // Find which group owns this tab
        QTabWidget *ownerGroup = nullptr;
        for (auto *grp : m_tabGroups) {
            if (grp->indexOf(tab->page) >= 0) { ownerGroup = grp; break; }
        }

        // Activate this tab if it's not current
        if (m_currentTab != tab) {
            m_currentTab = tab;
            m_rawData = tab->rawData;
            m_dataOffset = tab->dataOffset;
            m_pixelsPerRow = tab->pixelsPerRow;
            m_gain = tab->gain;
            m_transformMode = tab->transformMode;
            m_traceCount = tab->traceCount;
            m_hZoom = tab->hZoom;
            m_timeRange = tab->timeRange;
            m_depthRange = tab->depthRange;
            scrollArea = tab->scrollArea;
            imageLabel = tab->imageLabel;
            chartView = tab->chartView;
            chartSeries = tab->chartSeries;
            m_btnApply->setText(tab->gainApplied ? "撤销" : "应用");
            m_btnApply->setEnabled(true);
            m_btnOK->setEnabled(true);
            m_btnCancel->setEnabled(true);
            if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
                QString fname = QFileInfo(tab->filePath).fileName();
                m_oneClickDlg->setWindowTitle(QString("一键处理 - %1").arg(fname));
            }
            // Switch the tab group's current tab
            if (ownerGroup) ownerGroup->setCurrentIndex(ownerGroup->indexOf(tab->page));
            updateWindowTitle();   // 点击图像激活该文件时同步窗口标题
        }
        // Always update group styles: clicking this window makes it active
        if (ownerGroup) updateGroupStyles(ownerGroup, m_tabGroups);
        // v1.0.108: 数据解译模式 — 拾取参考点/手动追踪/标注绘制(S4)
        if (m_currentTab == tab && ribbonTab && ribbonTab->currentIndex() == 3) {
            const int skip = tab->zeroApplied ? tab->zeroSkipRows : 0;
            const int drawRows = tab->pixelsPerRow - skip;
            // RADAN式自动追踪: 开始状态下点击=放参考点; ≥2个参考点时自动估算层点
            if (m_btnTrackStart && m_btnTrackStart->isChecked()) {
                tab->trackSeeds.append(QPointF(pos.x(), pos.y()));
                std::sort(tab->trackSeeds.begin(), tab->trackSeeds.end(),
                          [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
                if (tab->trackSeeds.size() >= 2)
                    autoTrackHorizon(selectedHorizon());   // 每放一个点自动追踪两参考点间
                else
                    syncInterpOverlays();   // 只有1个点先显示种子
                return;
            }
            if (m_btnManualTrack && m_btnManualTrack->isChecked()) {
                const int hIdx = selectedHorizon();
                if (hIdx >= 0 && hIdx < tab->radanLayers.size()) {
                    HorizonLayer &h = tab->radanLayers[hIdx];
                    h.points.append(QPointF(pos.x(), pos.y()));
                    std::sort(h.points.begin(), h.points.end(),
                              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
                    syncInterpOverlays();
                    commitInterp();
                }
                return;
            }
            Q_UNUSED(drawRows);
            // v1.0.116: 异常标注形状创建已移至形状按钮联动(anomalySetShape);
            // 图上点击不再创建异常, ImageLabel 直接处理编辑态拖动/调整
        }
        onImageClicked(pos);
    });

    connect(tab->imageLabel, &ImageLabel::transformSelected, this, [this, tab](int mode) {
        tab->transformMode = mode;
        if (m_currentTab == tab) {
            m_transformMode = mode;
            refreshImage();
        }
    });

    connect(tab->imageLabel, &ImageLabel::gainSelected, this, [this, tab](float gainDb) {
        tab->gain = pow(10.0f, gainDb / 20.0f);
        tab->transformMode = 0;
        if (m_currentTab == tab) {
            m_gain = tab->gain;
            m_transformMode = 0;
            refreshImage();
        }
    });

    connect(tab->extHScrollBar, &QScrollBar::valueChanged, this, [this, tab](int value) {
        QScrollBar *isb = tab->scrollArea->horizontalScrollBar();
        isb->setRange(tab->extHScrollBar->minimum(), tab->extHScrollBar->maximum());
        isb->setPageStep(tab->extHScrollBar->pageStep());
        isb->setValue(value);
    });
    connect(tab->extHScrollBar, &QScrollBar::valueChanged,
            tab->topRuler, &HRulerWidget::setOffset);

    m_tabs.append(tab);

    // Add to active tab group
    QString tabTitle = QFileInfo(filePath).fileName();
    int idx = m_activeTabGroup->addTab(tab->page, tabTitle);
    m_activeTabGroup->setCurrentIndex(idx);

    // Initialize chart handles with current point count and gain mode
    if (tab->chartView) {
        int pointCount = 1;
        QTreeWidgetItem *root = gainTree->invisibleRootItem()->child(0);
        if (root) {
            for (int c = 0; c < root->childCount(); ++c) {
                QTreeWidgetItem *child = root->child(c);
                for (int g = 0; g < child->childCount(); ++g) {
                    if (child->child(g)->text(0).contains(QString::fromUtf8("点数"))) {
                        QSpinBox *sb = qobject_cast<QSpinBox*>(gainTree->itemWidget(child->child(g), 1));
                        if (sb) pointCount = sb->value();
                    }
                }
            }
        }
        int actual = (pointCount == 1) ? 2 : pointCount;
        tab->chartView->setSampleCount(tab->pixelsPerRow);   // 增益手柄 Y 跨度按 nsamp
        tab->chartView->setLineCount(pointCount);

        bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
        float initVal = isLinear ? 1.0f : 0.0f;
        if (isLinear)
            tab->chartView->setGainRange(0.0f, 10.0f);
        else
            tab->chartView->setGainRange(-6.0f, 6.0f);
        for (int i = 0; i < actual; ++i)
            tab->chartView->setHandleX(i, initVal);
    }

    // Defer resize until layout is settled (viewport not sized yet during addTab)
    QTimer::singleShot(0, this, [this]() {
        updateRulers();
        resizeImageLabel();
    });

    // 新建 tab 立即成为当前 tab → 同步标题（防止 currentChanged 信号未触发的边界情况）
    tab->markers = readDzxMarkers(filePath);   // v1.0.98: 同名 DZX <MarkGroup> 标记
    // v1.0.108: 读 InterpGroup(异常标注)
    readDzxInterp(filePath, tab->horizons, tab->anomalies);
    // v1.0.113: 读 RADAN 原生 LayerGroup/LayerWayPt 层位(层位列表数据源)
    readDzxDLayers(filePath, tab->radanLayers);
    m_currentTab = tab;
    updateWindowTitle();

    return tab;
}

void MainWindow::switchToTab(int index)
{
    if (index < 0 || m_tabs.isEmpty()) {
        m_currentTab = nullptr;
        scrollArea = nullptr;
        imageLabel = nullptr;
        chartView = nullptr;
        chartSeries = nullptr;
        m_rawData.clear();
        m_dataOffset = 0;
        m_pixelsPerRow = 512;
        m_gain = 1.0f;
        m_transformMode = 0;
        m_traceCount = 0;
        m_wiggleMode = false;
        if (m_btnStack) {
            QSignalBlocker b(m_btnStack);
            m_btnStack->setChecked(false);
        }
        m_timeRange = 20.0;
        m_depthRange = 1.25;
        updateTraceRange();
        updateWindowTitle();
        setHeaderPanelVisible(false);   // v1.0.87 无文件自动收起右栏
        syncEditUiState();              // v1.0.98 无文件收起编辑面板
        return;
    }

    auto *srcGroup = qobject_cast<QTabWidget*>(sender());
    if (!srcGroup) return;
    m_activeTabGroup = srcGroup;

    QWidget *page = srcGroup->widget(index);
    TabData *tab = nullptr;
    for (auto *t : m_tabs) {
        if (t->page == page) { tab = t; break; }
    }
    if (!tab) return;

    // RADAN规律: 切换文件时把上一个文件的标记+解译数据一次性写入 DZX
    if (m_currentTab && m_currentTab != tab) {
        flushMarkersToDzx(m_currentTab);
        flushInterpToDzx(m_currentTab);
    }

    m_currentTab = tab;

    // Update group styles: active group has bold selected tab
    updateGroupStyles(srcGroup, m_tabGroups);

    m_rawData = tab->rawData;
    m_dataOffset = tab->dataOffset;
    m_pixelsPerRow = tab->pixelsPerRow;
    if (m_gainSampleEndItem) m_gainSampleEndItem->setText(1, QString::number(m_pixelsPerRow - 1));
    m_gain = tab->gain;
    m_transformMode = tab->transformMode;
    m_traceCount = tab->traceCount;
    m_hZoom = tab->hZoom;
    m_wiggleMode = tab->wiggleMode;
    if (m_btnStack) {
        QSignalBlocker b(m_btnStack);   // 同步 checked 不触发 clicked
        m_btnStack->setChecked(m_wiggleMode);
    }
    if (!m_wiggleMode && m_displayGroup) {
        // 非波列图tab: 按当前显示模式恢复对应按钮选中(程序化 setChecked 不发 clicked)
        if (auto *b = m_displayGroup->button(m_showAscan ? 1 : 0))
            b->setChecked(true);
    }
    tab->imageLabel->setCrosshairDark(m_wiggleMode);   // 切tab同步十字颜色
    m_timeRange = tab->timeRange;
    m_depthRange = tab->depthRange;
    updateTraceRange();
    if (m_headerPanel && m_headerPanel->isVisible())
        refreshHeaderPanel();   // v1.0.87 切文件刷新右栏字段
    syncEditUiState();          // v1.0.98 切文件同步编辑面板(缩放控件回填/无文件收起)
    syncInterpUiState();        // v1.0.108 切文件同步解译面板

    scrollArea = tab->scrollArea;
    imageLabel = tab->imageLabel;
    chartView = tab->chartView;
    chartSeries = tab->chartSeries;

    // 同步增益handle显示：仅在增益面板可见且处于增益页时显示
    bool gainActive = m_leftPanel && m_leftPanel->isVisible()
                      && m_leftStack->currentWidget() == m_gainPage;
    chartView->setGainVisible(gainActive);
    syncAscanVisibility();   // 切tab同步 A-SCAN 波形列显隐(显示模式||编辑中)

    // Sync button state
    m_btnApply->setText(tab->gainApplied ? "撤销" : "应用");
    m_btnApply->setEnabled(true);
    m_btnOK->setEnabled(true);
    m_btnCancel->setEnabled(true);

    // Update one-click dialog title to match active file
    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        QString fname = QFileInfo(tab->filePath).fileName();
        m_oneClickDlg->setWindowTitle(QString("一键处理 - %1").arg(fname));
    }

    // Defer resize until the tab page layout is settled
    QTimer::singleShot(0, this, [this]() {
        if (m_currentTab) {
            updateRulers();
            resizeImageLabel();
            refreshImage();   // 刷新图像(zeroApplied 等属性可能刚被设置)
        }
    });

    updateWindowTitle();
}

void MainWindow::closeTab(int index)
{
    auto *srcGroup = qobject_cast<QTabWidget*>(sender());
    if (!srcGroup) return;
    if (index < 0 || index >= srcGroup->count()) return;

    QWidget *page = srcGroup->widget(index);
    TabData *tab = nullptr;
    for (auto *t : m_tabs) {
        if (t->page == page) { tab = t; break; }
    }
    if (!tab) return;

    // RADAN规律: 关闭文件时把标记+解译数据一次性写入 DZX
    flushMarkersToDzx(tab);
    flushInterpToDzx(tab);

    m_tabs.removeOne(tab);
    srcGroup->removeTab(index);

    delete tab->page;
    delete tab;

    // If the group is now empty and not the original one, remove it
    if (srcGroup->count() == 0 && srcGroup != m_docTabWidget) {
        m_tabGroups.removeOne(srcGroup);
        delete srcGroup;
        m_activeTabGroup = m_docTabWidget;
        collapseEmptySplitters();
    }

    if (m_tabs.isEmpty()) {
        showWelcome();
    }
    updateWindowTitle();
}

void MainWindow::showWelcome()
{
    m_leftPanel->hide();
    m_docSplitter->hide();
    welcomeLabel->show();
    coordinateLabel->setText(QString::fromUtf8("道号: -  深度: -"));   // v1.0.87 状态栏常显
    coordinateLabel->setToolTip(QString());
    QTimer::singleShot(0, this, [this]() { repositionSwitchButton(); });  // 无文件→隐藏三角按钮
    QTimer::singleShot(0, this, [this]() { updateWelcomePixmap(); });  // 布局稳定后按比例铺满
    m_btnApply->setEnabled(false);
    m_btnOK->setEnabled(false);
    m_btnCancel->setEnabled(false);
}

void MainWindow::hideWelcome()
{
    welcomeLabel->hide();
    m_docSplitter->show();
}

void MainWindow::splitHorizontal(QTabWidget *srcGroup, int tabIdx)
{
    if (!srcGroup || tabIdx < 0 || tabIdx >= srcGroup->count()) return;

    // Get the page and title from source group
    QWidget *page = srcGroup->widget(tabIdx);
    QString title = srcGroup->tabText(tabIdx);
    srcGroup->removeTab(tabIdx);

    // Create new tab group
    QTabWidget *newGroup = new QTabWidget(this);
    newGroup->setTabsClosable(true);
    newGroup->setDocumentMode(true);
    newGroup->setStyleSheet(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { background: #e0e0e0; padding: 6px 16px; border: 1px solid #c0c0c0; min-width: 80px; }"
        "QTabBar::tab:selected { background: #ffffff; font-weight: bold; }"
    );
    newGroup->tabBar()->installEventFilter(this);

    connect(newGroup, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(newGroup, &QTabWidget::currentChanged, this, &MainWindow::switchToTab);

    newGroup->addTab(page, title);
    m_tabGroups.append(newGroup);

    m_docSplitter->addWidget(newGroup);

    // Stretch all groups equally so they auto-resize with the window
    for (int i = 0; i < m_docSplitter->count(); ++i)
        m_docSplitter->setStretchFactor(i, 1);

    // Distribute sizes equally among all groups
    int h = m_docSplitter->height();
    if (h < 100) h = 600;
    int n = m_docSplitter->count();
    QList<int> sizes;
    for (int i = 0; i < n; ++i)
        sizes << h / n;
    m_docSplitter->setSizes(sizes);

    newGroup->setCurrentIndex(0);
    m_activeTabGroup = newGroup;
}

void MainWindow::splitVertical(QTabWidget *srcGroup, int tabIdx)
{
    if (!srcGroup || tabIdx < 0 || tabIdx >= srcGroup->count()) return;

    // Get the page and title from source group
    QWidget *page = srcGroup->widget(tabIdx);
    QString title = srcGroup->tabText(tabIdx);
    srcGroup->removeTab(tabIdx);

    // Create new tab group (same setup as splitHorizontal)
    QTabWidget *newGroup = new QTabWidget(this);
    newGroup->setTabsClosable(true);
    newGroup->setDocumentMode(true);
    newGroup->setStyleSheet(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { background: #e0e0e0; padding: 6px 16px; border: 1px solid #c0c0c0; min-width: 80px; }"
        "QTabBar::tab:selected { background: #ffffff; font-weight: bold; }"
    );
    newGroup->tabBar()->installEventFilter(this);

    connect(newGroup, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(newGroup, &QTabWidget::currentChanged, this, &MainWindow::switchToTab);

    newGroup->addTab(page, title);
    m_tabGroups.append(newGroup);

    // Find srcGroup's parent splitter and its position. When a vertical
    // split is requested on a group already nested in a horizontal pair,
    // this correctly nests deeper instead of always touching the top splitter.
    QSplitter *parentSplitter = qobject_cast<QSplitter*>(srcGroup->parentWidget());
    if (!parentSplitter) parentSplitter = m_docSplitter;
    int pos = parentSplitter->indexOf(srcGroup);

    // Build a horizontal sub-splitter holding [srcGroup, newGroup] side by side.
    QSplitter *hSplitter = new QSplitter(Qt::Horizontal, this);
    hSplitter->setChildrenCollapsible(false);

    // Replace srcGroup's slot with the horizontal splitter, then drop the two
    // groups into it (replaceWidget reparents srcGroup out, so we re-add it).
    parentSplitter->replaceWidget(pos, hSplitter);
    hSplitter->addWidget(srcGroup);
    hSplitter->addWidget(newGroup);

    // Equal stretch + half/half sizes within the side-by-side pair
    hSplitter->setStretchFactor(0, 1);
    hSplitter->setStretchFactor(1, 1);
    int w = hSplitter->width();
    if (w < 100) w = 800;
    hSplitter->setSizes({ w / 2, w / 2 });

    newGroup->setCurrentIndex(0);
    m_activeTabGroup = newGroup;
}

void MainWindow::collapseEmptySplitters()
{
    // After a group is deleted (closeTab or drag-drop), a nested horizontal
    // sub-splitter may be left with 0 or 1 groups. Remove empties and promote
    // any single remaining child back up into its grandparent splitter so the
    // layout never keeps a redundant one-group splitter. Handles arbitrary depth.
    while (true) {
        auto subs = m_docSplitter->findChildren<QSplitter*>();
        QSplitter *victim = nullptr;
        for (QSplitter *s : subs) {
            if (s->count() <= 1) { victim = s; break; }
        }
        if (!victim) break;

        if (victim->count() == 0) {
            delete victim;
            continue;
        }
        // Single child: promote it into the grandparent, then drop the splitter.
        QWidget *only = victim->widget(0);
        QSplitter *grandparent = qobject_cast<QSplitter*>(victim->parentWidget());
        if (grandparent) {
            int idx = grandparent->indexOf(victim);
            grandparent->replaceWidget(idx, only);
        } else {
            only->setParent(nullptr);
        }
        delete victim;
    }
}

void MainWindow::moveTabToGroup(QTabWidget *srcGroup, int tabIdx, QTabWidget *dstGroup)
{
    if (!srcGroup || !dstGroup || srcGroup == dstGroup) return;
    if (tabIdx < 0 || tabIdx >= srcGroup->count()) return;

    QWidget *page = srcGroup->widget(tabIdx);
    QString title = srcGroup->tabText(tabIdx);
    srcGroup->removeTab(tabIdx);
    dstGroup->addTab(page, title);
    dstGroup->setCurrentIndex(dstGroup->count() - 1);

    // 源组移空(且非原始组)则删除并折叠残留空 splitter
    if (srcGroup->count() == 0 && srcGroup != m_docTabWidget) {
        m_tabGroups.removeOne(srcGroup);
        delete srcGroup;
        collapseEmptySplitters();
    }
    m_activeTabGroup = dstGroup;
    updateGroupStyles(dstGroup, m_tabGroups);
    updateWindowTitle();
}

// 激活指定 tab:在所有选项卡组中定位其所在组并选中(触发 switchToTab)
void MainWindow::activateTabData(TabData *tab)
{
    if (!tab) return;
    for (QTabWidget *g : m_tabGroups) {
        int idx = g->indexOf(tab->page);
        if (idx >= 0) {
            m_activeTabGroup = g;
            g->setCurrentIndex(idx);   // 触发 currentChanged → switchToTab
            return;
        }
    }
}

// 把切换三角按钮固定到文档区(m_docSplitter)的右上角 = 整个窗体最右、与TAB平行。
// 无论水平/垂直如何分组,m_docSplitter 始终铺满文档区,其右上角即窗体最右端。
// 只要有打开的文件就显示(welcome/无文件时隐藏)。
void MainWindow::repositionSwitchButton()
{
    if (!m_btnSwitchFile || !m_docSplitter) return;
    if (m_tabs.isEmpty()) { m_btnSwitchFile->hide(); return; }
    QWidget *cw = centralWidget();
    if (!cw) return;
    QPoint tr = m_docSplitter->mapTo(cw, QPoint(m_docSplitter->width(), 0));
    m_btnSwitchFile->move(qMax(0, tr.x() - m_btnSwitchFile->width()), tr.y());
    m_btnSwitchFile->raise();
    m_btnSwitchFile->show();   // 有文件即显示(不再依赖 isVisible 的瞬时状态)
}

// 下拉显示所有已加载文件:每行 = [缩略图] + [文件名],点击切换为当前活动窗体
void MainWindow::showFileSwitchDropdown()
{
    if (m_tabs.isEmpty()) return;
    const int THUMB_W = 72, THUMB_H = 54;

    QFrame *popup = new QFrame(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::Box);
    popup->setStyleSheet(
        "QFrame { background: #ffffff; }"
        "QPushButton { text-align: left; border: none; border-radius: 3px;"
        " padding: 4px 8px; font-size: 12px; color: #222; }"
        "QPushButton:hover { background: #dce7f5; }"
    );
    QVBoxLayout *vl = new QVBoxLayout(popup);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(2);

    for (TabData *tab : m_tabs) {
        QString name = QFileInfo(tab->filePath).fileName();
        // 缩略图:抓取该 tab 的图像当前显示,等比放大后居中裁剪到固定尺寸
        QPixmap thumb;
        if (tab->imageLabel) {
            QPixmap g = tab->imageLabel->grab();
            if (!g.isNull()) {
                QPixmap s = g.scaled(QSize(THUMB_W, THUMB_H),
                                     Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                int x = (s.width()  - THUMB_W) / 2;
                int y = (s.height() - THUMB_H) / 2;
                thumb = s.copy(qMax(0, x), qMax(0, y), THUMB_W, THUMB_H);
            }
        }
        QPushButton *row = new QPushButton(popup);
        row->setIcon(QIcon(thumb));
        row->setIconSize(QSize(THUMB_W, THUMB_H));
        row->setText(name);
        row->setToolTip(tab->filePath);
        if (tab == m_currentTab) {
            QFont f = row->font();
            f.setBold(true);
            row->setFont(f);
            row->setStyleSheet("color: #1a5fb4;");
        }
        TabData *t = tab;
        QObject::connect(row, &QPushButton::clicked, popup, [this, popup, t]() {
            popup->close();
            activateTabData(t);
        });
        vl->addWidget(row);
    }
    popup->setFixedWidth(300);
    popup->adjustSize();
    // 右对齐到按钮(按钮在标签栏最右端,避免下拉溢出屏幕右侧)
    QPoint p = m_btnSwitchFile->mapToGlobal(QPoint(m_btnSwitchFile->width(), m_btnSwitchFile->height()));
    popup->move(qMax(0, p.x() - popup->width()), p.y());
    popup->setFocus();
    popup->show();
}

void MainWindow::showAbout()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("关于"));
    dlg.setFixedSize(440, 230);
    QVBoxLayout *main = new QVBoxLayout(&dlg);
    main->setContentsMargins(24, 20, 24, 16);
    main->setSpacing(10);

    QHBoxLayout *top = new QHBoxLayout();
    top->setSpacing(16);
    QLabel *logo = new QLabel;
    logo->setPixmap(QIcon(":/icons/resources/icon_fileheader_64.png").pixmap(64, 64));
    logo->setFixedSize(64, 64);
    top->addWidget(logo, 0, Qt::AlignTop);

    QVBoxLayout *txt = new QVBoxLayout();
    txt->setSpacing(4);
    QLabel *name = new QLabel(QString::fromUtf8("劳雷"));
    QFont nf = name->font(); nf.setPointSize(15); nf.setBold(true); name->setFont(nf);
    QLabel *ver = new QLabel(QString::fromUtf8("版本 ") + APP_VERSION);
    QLabel *cpy = new QLabel(QString::fromUtf8("版权 © 2026 劳雷"));
    QLabel *url = new QLabel(QString::fromUtf8("<a href=\"https://www.laurel.com.cn\">https://www.laurel.com.cn</a>"));
    url->setOpenExternalLinks(true);
    url->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ver->setStyleSheet("color:#333;");
    cpy->setStyleSheet("color:#333;");
    url->setStyleSheet("color:#0066cc;");
    txt->addWidget(name); txt->addWidget(ver); txt->addWidget(cpy);
    txt->addSpacing(4); txt->addWidget(url);
    top->addLayout(txt);
    top->addStretch();
    main->addLayout(top);
    main->addStretch();

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton *close = new QPushButton(QString::fromUtf8("关闭"));
    close->setFixedSize(84, 30);
    btns->addWidget(close);
    main->addLayout(btns);
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void MainWindow::showUpgrade()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("升级"));
    dlg.setFixedSize(480, 320);
    QVBoxLayout *main = new QVBoxLayout(&dlg);
    main->setContentsMargins(24, 20, 24, 16);
    main->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("软件升级"));
    QFont tf = title->font(); tf.setPointSize(13); tf.setBold(true); title->setFont(tf);
    main->addWidget(title);

    QLabel *cur = new QLabel(QString::fromUtf8("当前版本:") + APP_VERSION);
    QLabel *latest = new QLabel(QString::fromUtf8("最新版本:--(尚未检查)"));
    latest->setStyleSheet("color:#666;");
    main->addWidget(cur);
    main->addWidget(latest);

    QProgressBar *bar = new QProgressBar;
    bar->setRange(0, 100); bar->setValue(0);
    main->addWidget(bar);

    QPlainTextEdit *notes = new QPlainTextEdit;
    notes->setReadOnly(true);
    notes->setPlainText(QString::fromUtf8("更新内容:\n(点击\"检查更新\"连接服务器查询最新版本)"));
    main->addWidget(notes);

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton *check = new QPushButton(QString::fromUtf8("检查更新"));
    QPushButton *upgrade = new QPushButton(QString::fromUtf8("立即升级后重启程序"));
    QPushButton *close = new QPushButton(QString::fromUtf8("关闭"));
    upgrade->setEnabled(false);
    check->setFixedSize(92, 30);
    upgrade->setFixedSize(150, 30);
    close->setFixedSize(92, 30);
    btns->addWidget(check); btns->addWidget(upgrade); btns->addWidget(close);
    main->addLayout(btns);

    QString latestVer;    // 检查更新后得到的服务端最新版本
    QString downloadUrl;  // 服务端返回的下载地址

    // 版本号比较 a.b.c ...,任一段大者为大
    auto verLess = [](const QString &a, const QString &b) -> bool {
        const QStringList la = a.split('.'), lb = b.split('.');
        int n = qMax(la.size(), lb.size());
        for (int i = 0; i < n; ++i) {
            int ia = (i < la.size()) ? la[i].toInt() : 0;
            int ib = (i < lb.size()) ? lb[i].toInt() : 0;
            if (ia != ib) return ia < ib;
        }
        return false;
    };

    // 检查更新: GET APP_UPDATE_URL 解析 JSON,与服务端版本比较
    QObject::connect(check, &QPushButton::clicked, [&]() {
        latest->setText(QString::fromUtf8("最新版本:查询中..."));
        latest->setStyleSheet("color:#666;");
        notes->setPlainText(QString::fromUtf8("正在连接升级服务器..."));
        bar->setRange(0, 100); bar->setValue(0);
        bar->setFormat(QString::fromUtf8("检查中"));
        upgrade->setEnabled(false);

        QNetworkRequest req{QUrl(QString::fromUtf8(APP_UPDATE_URL))};
        req.setHeader(QNetworkRequest::UserAgentHeader, QString("MyQtApp/") + APP_VERSION);
        req.setTransferTimeout(10000);
        QNetworkReply *r = m_net->get(req);
        QObject::connect(r, &QNetworkReply::finished, [&, r]() {
            bool ok = (r->error() == QNetworkReply::NoError);
            QByteArray data = ok ? r->readAll() : QByteArray();
            QString err = r->errorString();
            r->deleteLater();
            if (!ok) {
                latest->setText(QString::fromUtf8("最新版本:查询失败"));
                latest->setStyleSheet("color:#cc0000;");
                notes->setPlainText(QString::fromUtf8("无法连接升级服务器:\n") + err);
                bar->setFormat(QString::fromUtf8("失败"));
                return;
            }
            QJsonObject obj = QJsonDocument::fromJson(data).object();
            latestVer = obj.value("latestVersion").toString();
            downloadUrl = obj.value("downloadUrl").toString();
            QString rn = obj.value("releaseNotes").toString();
            latest->setText(QString::fromUtf8("最新版本:") + latestVer);
            if (verLess(QString::fromUtf8(APP_VERSION), latestVer)) {
                latest->setStyleSheet("color:#cc6600;");
                notes->setPlainText(QString::fromUtf8("发现新版本 ") + latestVer +
                                    QString::fromUtf8("!\n\n更新内容:\n") + rn);
                bar->setFormat(QString::fromUtf8("可升级"));
                upgrade->setEnabled(!downloadUrl.isEmpty());
            } else {
                latest->setStyleSheet("color:#008800;");
                notes->setPlainText(QString::fromUtf8("已是最新版本,无需升级。"));
                bar->setValue(100); bar->setFormat(QString::fromUtf8("已是最新"));
                upgrade->setEnabled(false);
            }
        });
    });

    // 立即升级: 用成员 QNetworkAccessManager 下载到临时文件,完成后生成"覆盖+重启"批处理并退出本程序
    QObject::connect(upgrade, &QPushButton::clicked, [&]() {
        if (downloadUrl.isEmpty()) return;
        check->setEnabled(false); upgrade->setEnabled(false);
        bar->setRange(0, 100); bar->setValue(0);
        bar->setFormat(QString::fromUtf8("下载中 %p%"));

        const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const QString newPath = QDir::tempPath() + "/MyQtApp_upgrade.exe";

        QNetworkRequest req{QUrl(downloadUrl)};
        req.setHeader(QNetworkRequest::UserAgentHeader, QString("MyQtApp/") + APP_VERSION);
        QNetworkReply *r = m_net->get(req);   // 成员 nam:生命周期不随对话框,避免销毁时崩溃
        QFile *f = new QFile(newPath);          // 无父对象,finished 里 deleteLater(磁盘文件保留供批处理 copy)
        if (!f->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            notes->setPlainText(QString::fromUtf8("无法写入临时文件:\n") + newPath);
            r->abort(); r->deleteLater(); f->deleteLater();
            check->setEnabled(true);
            return;
        }
        QObject::connect(r, &QNetworkReply::downloadProgress, [bar](qint64 got, qint64 tot) {
            if (tot > 0) bar->setValue(static_cast<int>(got * 100 / tot));
        });
        QObject::connect(r, &QNetworkReply::readyRead, [r, f]() {
            f->write(r->readAll());
        });
        QObject::connect(r, &QNetworkReply::finished, [&, r, f, appPath, newPath]() {
            f->write(r->readAll());
            f->close();
            bool ok = (r->error() == QNetworkReply::NoError);
            QString err = r->errorString();
            f->deleteLater();
            r->deleteLater();
            if (!ok) {
                QFile::remove(newPath);
                notes->setPlainText(QString::fromUtf8("下载失败:") + err);
                bar->setFormat(QString::fromUtf8("失败"));
                check->setEnabled(true);
                return;
            }
            bar->setValue(100); bar->setFormat(QString::fromUtf8("完成"));

            // 询问用户:立即重启 / 下次启动再用(避免下载太快、突然关闭被误以为闪退)
            QMessageBox choice(&dlg);
            choice.setWindowTitle(QString::fromUtf8("升级"));
            choice.setText(QString::fromUtf8("新版本已下载完成。是否立即重启程序以使用新版本?"));
            QPushButton *btnNow = choice.addButton(QString::fromUtf8("立即重启"), QMessageBox::AcceptRole);
            choice.addButton(QString::fromUtf8("下次启动再用"), QMessageBox::RejectRole);
            choice.exec();
            bool restartNow = (choice.clickedButton() == btnNow);

            if (restartNow) {
                // 覆盖+重启批处理:等本程序退出 → copy → start → 自删
                const QString batPath = QDir::tempPath() + "/gpr_updater.bat";
                QString bat = QString::fromUtf8(
                    "@echo off\r\n"
                    "setlocal enabledelayedexpansion\r\n"
                    "set \"APP=__APP__\"\r\n"
                    "set \"NEW=__NEW__\"\r\n"
                    "set \"PID=__PID__\"\r\n"
                    "set \"LOG=%TEMP%\\gpr_update.log\"\r\n"
                    ">\"%LOG%\" echo === gpr_updater %date% %time% restart\r\n"
                    ">>\"%LOG%\" echo APP=%APP% PID=%PID%\r\n"
                    "rem 1) 等本程序自行退出(诊断版带终端,最多等约10s)\r\n"
                    "set /a w=0\r\n"
                    ":waitpid\r\n"
                    "tasklist /fi \"PID eq %PID%\" 2>nul | find \"%PID%\" >nul\r\n"
                    "if errorlevel 1 goto dead\r\n"
                    "set /a w+=1\r\n"
                    "if !w! geq 10 goto forcekill\r\n"
                    "ping 127.0.0.1 -n 2 >nul\r\n"
                    "goto waitpid\r\n"
                    ":forcekill\r\n"
                    ">>\"%LOG%\" echo still_alive_force_kill_after_!w!\r\n"
                    "taskkill /PID %PID% /F >nul 2>&1\r\n"
                    "ping 127.0.0.1 -n 3 >nul\r\n"
                    ":dead\r\n"
                    "rem 2) 等 exe 解锁后覆盖(最多约30s)\r\n"
                    "set /a tries=0\r\n"
                    ":copyloop\r\n"
                    "copy /y \"%NEW%\" \"%APP%\" >nul 2>&1 && goto ok\r\n"
                    "set /a tries+=1\r\n"
                    "if !tries! geq 30 goto fail\r\n"
                    "ping 127.0.0.1 -n 2 >nul\r\n"
                    "goto copyloop\r\n"
                    ":ok\r\n"
                    ">>\"%LOG%\" echo OK_copied_after_%tries%_tries\r\n"
                    "del /f /q \"%NEW%\" >nul 2>&1\r\n"
                    "start \"\" \"%APP%\"\r\n"
                    "del /f /q \"%~f0\" 2>nul\r\n"
                    "exit /b\r\n"
                    ":fail\r\n"
                    ">>\"%LOG%\" echo FAIL_copy_after_30_tries\r\n"
                    "del /f /q \"%~f0\" 2>nul\r\n"
                    "exit /b\r\n"
                );
                bat.replace(QString::fromUtf8("__APP__"), appPath);
                bat.replace(QString::fromUtf8("__NEW__"), QDir::toNativeSeparators(newPath));
                bat.replace(QString::fromUtf8("__PID__"), QString::number(QCoreApplication::applicationPid()));
                QFile bf(batPath);
                if (bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    bf.write(bat.toLocal8Bit()); bf.close();
                }
                // 用 ShellExecuteW 启动批处理(完全独立进程,不共享诊断终端控制台)
                notes->setPlainText(QString::fromUtf8("正在关闭本程序、覆盖 exe 并重启..."));
                m_upgradeRestart = true;
#ifdef Q_OS_WIN
                {
                    QString cmd = QString("/c \"") + QDir::toNativeSeparators(batPath) + "\"";
                    std::wstring wcmd = cmd.toStdWString();
                    ShellExecuteW(NULL, L"open", L"cmd.exe", wcmd.c_str(), NULL, SW_HIDE);
                }
                ::Sleep(500);
                FreeConsole();
                ::ExitProcess(0);
#endif
                dlg.accept();
            } else {
                // 下次启动再用:记下待应用,本程序正常退出时由 closeEvent 写"仅覆盖"批处理
                m_pendingUpgradeNewPath = newPath;
                m_pendingUpgradeAppPath = appPath;
                notes->setPlainText(QString::fromUtf8("已下载。将在本程序退出后自动替换为最新版本,下次启动即生效。"));
                dlg.accept();   // 关闭升级对话框,本程序继续运行(不退出)
            }
        });
    });

    QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
    if (m_upgradeRestart) {            // 升级:批处理已启动,此处强制终止本进程以释放 exe
        m_upgradeRestart = false;
#ifdef Q_OS_WIN
        FreeConsole();                 // 关闭诊断终端
        // 立即终止进程(不依赖 quit() 的异步退出与清理——诊断终端的句柄可能令进程迟迟不退出,
        // 导致批处理覆盖 exe 失败、重启失败)。批处理已独立启动,会等待本 PID 退出后覆盖并重启。
        ::ExitProcess(0);
#endif
        QCoreApplication::quit();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // RADAN规律: 关闭程序时把所有文件的标记+解译数据一次性写入 DZX
    for (auto *t : m_tabs) {
        flushMarkersToDzx(t);
        flushInterpToDzx(t);
    }

    // 若有"下次启动再用"的待应用升级:启动"等退出→覆盖(不重启)→自删"批处理,
    // 本程序退出后自动替换 exe,无后台常驻进程。
    if (!m_pendingUpgradeNewPath.isEmpty()) {
        const QString batPath = QDir::tempPath() + "/gpr_updater.bat";
        QString bat = QString::fromUtf8(
            "@echo off\r\n"
            "setlocal enabledelayedexpansion\r\n"
            "set \"APP=__APP__\"\r\n"
            "set \"NEW=__NEW__\"\r\n"
            "set \"PID=__PID__\"\r\n"
            "set \"LOG=%TEMP%\\gpr_update.log\"\r\n"
            ">\"%LOG%\" echo === gpr_updater %date% %time% later\r\n"
            ">>\"%LOG%\" echo APP=%APP% PID=%PID%\r\n"
            "rem 1) 等本程序自行退出(诊断版带终端,最多等约10s)\r\n"
            "set /a w=0\r\n"
            ":waitpid\r\n"
            "tasklist /fi \"PID eq %PID%\" 2>nul | find \"%PID%\" >nul\r\n"
            "if errorlevel 1 goto dead\r\n"
            "set /a w+=1\r\n"
            "if !w! geq 10 goto forcekill\r\n"
            "ping 127.0.0.1 -n 2 >nul\r\n"
            "goto waitpid\r\n"
            ":forcekill\r\n"
            ">>\"%LOG%\" echo still_alive_force_kill_after_!w!\r\n"
            "taskkill /PID %PID% /F >nul 2>&1\r\n"
            "ping 127.0.0.1 -n 3 >nul\r\n"
            ":dead\r\n"
            "rem 2) 等 exe 解锁后覆盖(最多约30s)\r\n"
            "set /a tries=0\r\n"
            ":copyloop\r\n"
            "copy /y \"%NEW%\" \"%APP%\" >nul 2>&1 && goto ok\r\n"
            "set /a tries+=1\r\n"
            "if !tries! geq 30 goto fail\r\n"
            "ping 127.0.0.1 -n 2 >nul\r\n"
            "goto copyloop\r\n"
            ":ok\r\n"
            ">>\"%LOG%\" echo OK_copied_after_%tries%_tries\r\n"
            "del /f /q \"%NEW%\" >nul 2>&1\r\n"
            "del /f /q \"%~f0\" 2>nul\r\n"
            "exit /b\r\n"
            ":fail\r\n"
            ">>\"%LOG%\" echo FAIL_copy_after_30_tries\r\n"
            "del /f /q \"%~f0\" 2>nul\r\n"
            "exit /b\r\n"
        );
        bat.replace(QString::fromUtf8("__APP__"), m_pendingUpgradeAppPath);
        bat.replace(QString::fromUtf8("__NEW__"), QDir::toNativeSeparators(m_pendingUpgradeNewPath));
        bat.replace(QString::fromUtf8("__PID__"), QString::number(QCoreApplication::applicationPid()));
        QFile bf(batPath);
        if (bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            bf.write(bat.toLocal8Bit()); bf.close();
            QProcess::startDetached("cmd.exe", QStringList{"/c", batPath});
        }
        m_pendingUpgradeNewPath.clear();
        m_pendingUpgradeAppPath.clear();
#ifdef Q_OS_WIN
        FreeConsole();   // 关闭诊断终端,避免阻碍进程退出导致替换失败(批处理另有 taskkill 兜底)
#endif
    }
    QMainWindow::closeEvent(event);
}

// 图像显示模式联动: A-SCAN 波形列显隐 = 线扫描+波形模式 || 零点/增益编辑中(编辑需要波形)
void MainWindow::syncAscanVisibility()
{
    if (!chartView) return;
    const bool editing = m_leftPanel && m_leftPanel->isVisible() && m_leftStack
                         && (m_leftStack->currentWidget() == m_gainPage
                             || m_leftStack->currentWidget() == m_zeroPage);
    chartView->setVisible(m_showAscan || editing);
}

// v1.0.98 横向缩放: 保持视口中心道不跳变
void MainWindow::setHZoom(float zoom)
{
    if (!m_currentTab) return;
    zoom = qBound(1.0f, zoom, 10.0f);

    // 缩放前视口中心道号(普通: x/hZoom; wiggle: (x/hZoom/32)*2 对齐偶数道)
    QScrollBar *hb = m_currentTab->extHScrollBar;
    const int vw = m_currentTab->scrollArea->viewport()->width();
    const int centerT = m_wiggleMode
        ? qRound((hb->value() + vw / 2) / (double)m_hZoom / 32.0) * 2
        : qRound((hb->value() + vw / 2) / (double)m_hZoom);

    m_hZoom = zoom;
    m_currentTab->hZoom = zoom;
    resizeImageLabel();

    const int newX = m_wiggleMode
        ? qRound(((centerT / 2) * 32 + 16) * (double)zoom)
        : qRound(centerT * (double)zoom);
    hb->setValue(qMax(0, newX - vw / 2));

    if (m_hZoomSlider) { QSignalBlocker b1(m_hZoomSlider); m_hZoomSlider->setValue(qRound(zoom)); }
    if (m_hZoomSpin)   { QSignalBlocker b2(m_hZoomSpin);   m_hZoomSpin->setValue(qRound(zoom)); }
}

// 编辑面板与 350px 文件头右栏互斥: 编辑面板可见→收起文件头; 收起→按文件头按钮恢复
void MainWindow::syncRightRail()
{
    if (!m_editPanel) return;
    if (m_editPanel->isVisible()) {
        if (m_headerPanel && m_headerPanel->isVisible())
            setHeaderPanelVisible(false);
    } else {
        if (m_btnHeaderToggle && m_btnHeaderToggle->isChecked() && m_headerPanel
            && !m_headerPanel->isVisible())
            m_headerPanel->setVisible(true);
    }
}

// v1.0.98 编辑模块状态总闸: 面板显隐/页切换/控件回填(S3 标记面板、S5 矩形框逐步并入)
void MainWindow::syncEditUiState()
{
    if (m_syncingEditUi) return;
    m_syncingEditUi = true;

    // v1.0.100: 数据块页优先; 缩放页仅在标记模式+缩放选中时出现(缩放是标记模式的子开关)
    const bool blockOn = m_btnEditBlock && m_btnEditBlock->isChecked() && m_currentTab;
    const bool zoomOn = m_btnEditMarker && m_btnEditMarker->isChecked()
                        && m_btnHZoom && m_btnHZoom->isChecked() && m_currentTab;
    if (m_editPanel) {
        m_editPanel->setVisible(blockOn || zoomOn);
        if ((blockOn || zoomOn) && m_editStack) {
            m_editStack->setCurrentIndex(blockOn ? 0 : 1);
            if (m_editTitleLbl)
                m_editTitleLbl->setText(blockOn ? QString::fromUtf8("编辑数据块")
                                                : QString::fromUtf8("横向缩放"));
        }
        syncRightRail();
    }

    // 数据块(S5/107): 进入数据块模式无块时自动建默认块; 覆盖层+缩略图同步
    if (m_currentTab && m_btnEditBlock && m_btnEditBlock->isChecked() && !m_wiggleMode
        && m_currentTab->editBlocks.isEmpty()) {
        createNewEditBlock();   // 启动/进入即有一个默认块可拖动编辑
    } else if (m_currentTab) {
        syncEditBlocksToView();
    }
    refreshSelectionInfo();

    // 标记面板: 编辑标记/编辑数据块两种模式都显示(数据块参考图也含标记表+缩略图)
    const bool panelOn = ((m_btnEditMarker && m_btnEditMarker->isChecked())
                          || (m_btnEditBlock && m_btnEditBlock->isChecked())) && m_currentTab;
    if (m_markerPanel) m_markerPanel->setVisible(panelOn);
    if (m_currentTab && m_currentTab->imageLabel)
        m_currentTab->imageLabel->setMarkerOverlay(panelOn, m_currentTab->markers);
    if (panelOn) {
        refreshMarkerPanel();
        updateMarkerThumb();
    }

    // 缩放控件回填当前 tab
    if (m_currentTab && m_hZoomSlider) {
        QSignalBlocker b1(m_hZoomSlider), b2(m_hZoomSpin);
        m_hZoomSlider->setValue(qRound(m_currentTab->hZoom));
        if (m_hZoomSpin) m_hZoomSpin->setValue(qRound(m_currentTab->hZoom));
    }

    m_syncingEditUi = false;
}

// 选区几何信息4字段刷新(显示活动块; 无活动块显示"保留"块; 都无显示"-")
void MainWindow::refreshSelectionInfo()
{
    auto set = [](QLabel *l, const QString &s) { if (l) l->setText(s); };
    int idx = -1;
    if (m_currentTab && m_currentTab->imageLabel) {
        idx = m_currentTab->imageLabel->editActiveBlock();
        if (idx < 0 || idx >= m_currentTab->editBlocks.size()) {
            for (int i = 0; i < m_currentTab->editBlocks.size(); ++i)
                if (m_currentTab->editBlocks[i].state == 1) { idx = i; break; }
        }
    }
    if (!m_currentTab || idx < 0 || idx >= m_currentTab->editBlocks.size()) {
        set(m_selStartLbl, QStringLiteral("-"));
        set(m_selEndLbl, QStringLiteral("-"));
        set(m_selTimeLbl, QStringLiteral("-"));
        set(m_selSizeLbl, QStringLiteral("-"));
        return;
    }
    const QRectF r = m_currentTab->editBlocks[idx].rectT.normalized();
    const int t0 = qRound(r.left()), t1 = qRound(r.right());
    const int s0 = qRound(r.top()), s1 = qRound(r.bottom());
    const double dt = (m_currentTab->nsamp > 0)
                          ? m_currentTab->headerRange / m_currentTab->nsamp : 0.0;
    set(m_selStartLbl, QString::number(t0));
    set(m_selEndLbl, QString::number(t1));
    set(m_selTimeLbl, QString::number(s0 * dt, 'f', 1) + " - "
                          + QString::number(s1 * dt, 'f', 1) + " ns");
    set(m_selSizeLbl, QString("%1 x %2 px").arg(t1 - t0 + 1).arg(s1 - s0 + 1));
}

// tab->editBlocks → 主图覆盖层 + 缩略图
void MainWindow::syncEditBlocksToView()
{
    if (!m_currentTab) return;
    if (m_currentTab->imageLabel) {
        m_currentTab->imageLabel->setEditBlocks(m_currentTab->editBlocks,
                                                m_currentTab->activeEditBlock);
        m_currentTab->imageLabel->setEditBlocksVisible(
            m_btnEditBlock && m_btnEditBlock->isChecked() && !m_wiggleMode
            && !m_currentTab->editBlocks.isEmpty());
    }
    if (m_markerThumb) {
        QVector<QRectF> rs;
        for (const EditBlk &b : m_currentTab->editBlocks) rs.append(b.rectT);
        m_markerThumb->setBlocks(rs);
        m_markerThumb->setSampleCount(
            m_pixelsPerRow - (m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0));
    }
}

// 清空全部数据块(重置选区)
void MainWindow::clearEditBlocks()
{
    if (!m_currentTab) return;
    m_currentTab->editBlocks.clear();
    m_currentTab->activeEditBlock = -1;
    syncEditBlocksToView();
    refreshSelectionInfo();
}

// 新建数据块: 默认视口中心 20%道×50%采样, 与现有块重叠时向右找空位; 进块模式无块时也用它建默认块
void MainWindow::createNewEditBlock()
{
    if (!requireOpenFile()) return;
    if (m_wiggleMode) {
        QMessageBox::information(this, QString::fromUtf8("新建矩形框"),
            QString::fromUtf8("波列图模式下无法使用矩形框, 请先切换到线扫描或线扫描+波形。"));
        return;
    }
    TabData *tab = m_currentTab;
    const int maxT = qMax(0, tab->traceCount - 1);
    const int drawRows = m_pixelsPerRow - (tab->zeroApplied ? tab->zeroSkipRows : 0);
    const int maxS = qMax(0, drawRows - 1);
    const int spanT = qMax(1, tab->traceCount / 5);
    const int spanS = qMax(1, (maxS + 1) / 2);
    const int s0 = qMax(0, (maxS + 1 - spanS) / 2);
    const int s1 = qMin(maxS, s0 + spanS);

    auto overlapsAny = [&](int t0) {
        const QRectF cand(t0, s0, qMin(spanT, maxT - t0), s1 - s0);
        for (const EditBlk &b : tab->editBlocks)
            if (cand.intersects(b.rectT.normalized())) return true;
        return false;
    };
    // 候选位置: 视口中心起, 向右再向左步进找第一个不重叠位置
    const int vw = tab->scrollArea->viewport()->width();
    const int centerT = qBound(0, qRound((tab->extHScrollBar->value() + vw / 2.0)
                                         / (double)m_hZoom), maxT);
    int t0 = -1;
    const int step = qMax(1, spanT / 2);
    for (int d = 0; d <= maxT; d += step) {
        const int r = centerT + d - spanT / 2;
        if (r >= 0 && r + spanT <= maxT && !overlapsAny(r)) { t0 = r; break; }
        const int l = centerT - d - spanT / 2;
        if (d > 0 && l >= 0 && l + spanT <= maxT && !overlapsAny(l)) { t0 = l; break; }
    }
    if (t0 < 0) {
        QMessageBox::information(this, QString::fromUtf8("新建矩形框"),
            QString::fromUtf8("没有可与现有数据块错开的空间, 请先调整/删除现有块。"));
        return;
    }
    EditBlk blk;
    blk.rectT = QRectF(t0, s0, qMin(spanT, maxT - t0), s1 - s0);
    blk.state = 0;
    tab->editBlocks.append(blk);
    tab->activeEditBlock = tab->editBlocks.size() - 1;
    syncEditBlocksToView();
    refreshSelectionInfo();
}

// 标记保留(唯一): 已有其他保留块时弹提示且不改变
void MainWindow::markEditBlockKeep(int idx)
{
    if (!m_currentTab || idx < 0 || idx >= m_currentTab->editBlocks.size()) return;
    for (int i = 0; i < m_currentTab->editBlocks.size(); ++i) {
        if (i != idx && m_currentTab->editBlocks[i].state == 1) {
            QMessageBox::warning(this, QString::fromUtf8("数据块"),
                QString::fromUtf8("当前有多个数据块需要保留 只能保留一个"));
            return;
        }
    }
    m_currentTab->editBlocks[idx].state =
        (m_currentTab->editBlocks[idx].state == 1) ? 0 : 1;   // 再点一次取消标记
    m_currentTab->activeEditBlock = idx;
    syncEditBlocksToView();
    refreshSelectionInfo();
}

// 标记删除(可多块, 再点一次取消)
void MainWindow::markEditBlockDelete(int idx)
{
    if (!m_currentTab || idx < 0 || idx >= m_currentTab->editBlocks.size()) return;
    m_currentTab->editBlocks[idx].state =
        (m_currentTab->editBlocks[idx].state == 2) ? 0 : 2;
    m_currentTab->activeEditBlock = idx;
    syncEditBlocksToView();
    refreshSelectionInfo();
}

// 保留/确认裁剪: 把整幅数据裁剪为标记"保留"的块(内存手术; 落盘走"保存"→_P_NN.DZT)
void MainWindow::performCropSelection()
{
    if (!m_currentTab) return;
    int keepIdx = -1;
    for (int i = 0; i < m_currentTab->editBlocks.size(); ++i)
        if (m_currentTab->editBlocks[i].state == 1) { keepIdx = i; break; }
    if (keepIdx < 0 || !m_currentTab->imageLabel
        || !m_currentTab->imageLabel->editBlocksVisible()) {
        QMessageBox::information(this, QString::fromUtf8("确认裁剪"),
            QString::fromUtf8("请先在数据块上点[保留]标记要保留的区域, 再确认裁剪。"));
        return;
    }
    TabData *tab = m_currentTab;
    const QRectF rf = tab->editBlocks[keepIdx].rectT.normalized();
    const int oldSamp = m_pixelsPerRow;
    const int skip = tab->zeroApplied ? tab->zeroSkipRows : 0;
    const int t0 = qBound(0, qRound(rf.left()), qMax(0, tab->traceCount - 1));
    const int t1 = qBound(t0, qRound(rf.right()), qMax(0, tab->traceCount - 1));
    const int s0 = qBound(0, qRound(rf.top()), qMax(0, oldSamp - skip - 1));
    const int s1 = qBound(s0, qRound(rf.bottom()), qMax(0, oldSamp - skip - 1));
    const int newTraceCount = t1 - t0 + 1;
    const int newSamp = s1 - s0 + 1;

    if (newTraceCount >= tab->traceCount && newSamp >= oldSamp) {
        QMessageBox::information(this, QString::fromUtf8("确认裁剪"),
            QString::fromUtf8("选区已覆盖整幅数据, 无需裁剪。"));
        return;
    }
    if (QMessageBox::question(this, QString::fromUtf8("确认裁剪"),
            QString::fromUtf8("将把整幅数据裁剪为选区 %1 道 × %2 采样(不可撤销), 是否继续?")
                .arg(newTraceCount).arg(newSamp),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    // 1. 数据手术: 逐道拷贝采样区间 [s0, s1]
    auto cropData = [&](const QByteArray &src) -> QByteArray {
        QByteArray dst;
        dst.resize(newTraceCount * newSamp * 4);
        for (int t = 0; t < newTraceCount; ++t)
            memcpy(dst.data() + (qint64)t * newSamp * 4,
                   src.constData() + ((qint64)(t0 + t) * oldSamp + s0) * 4,
                   (size_t)newSamp * 4);
        return dst;
    };
    tab->rawData = cropData(tab->rawData);
    m_rawData = tab->rawData;
    if (!tab->originalRawData.isEmpty()
        && tab->originalRawData.size() == tab->traceCount * oldSamp * 4)
        tab->originalRawData = cropData(tab->originalRawData);

    // 2. 头字段: nsamp i16@4 / ntraces i32@20 / range f32@26(比例缩放保持 ns/采样)
    const float oldRange = tab->headerRange;
    const double dt = oldSamp > 0 ? oldRange / oldSamp : 0.0;
    if (tab->header.size() >= 30) {
        qint16 ns = (qint16)newSamp;
        memcpy(tab->header.data() + 4, &ns, 2);
        qint32 nt = (qint32)newTraceCount;
        memcpy(tab->header.data() + 20, &nt, 4);
        const float rg = oldRange * (float)newSamp / (float)oldSamp;
        memcpy(tab->header.data() + 26, &rg, 4);
    }
    m_header = tab->header;

    // 3. 字段同步(tab 与 MainWindow 成员)
    tab->nsamp = newSamp;
    tab->pixelsPerRow = newSamp;
    m_pixelsPerRow = newSamp;
    tab->headerRange = oldRange * (float)newSamp / (float)oldSamp;
    m_headerRange = tab->headerRange;
    tab->signalPosition = qMax(0.0f, tab->signalPosition - (float)(s0 * dt));
    m_signalPos = tab->signalPosition;
    tab->zeroApplied = false;
    tab->zeroSkipRows = 0;
    tab->traceCount = newTraceCount;
    m_traceCount = newTraceCount;
    tab->dataRev++;

    // 4. 标记平移(界外剔除, 随裁剪持久化)
    QVector<int> mk2;
    for (int t : tab->markers)
        if (t >= t0 && t <= t1) mk2.append(t - t0);
    tab->markers = mk2;
    writeDzxMarkers(tab->filePath, tab->markers);

    // 5. 图表同步
    if (tab->chartView) {
        tab->chartView->setSampleCount(newSamp);
        QValueAxis *axisY = qobject_cast<QValueAxis *>(tab->chartView->chart()->axisY(tab->chartSeries));
        if (axisY) axisY->setRange(0, newSamp - 1);
    }
    if (m_gainSampleEndItem) m_gainSampleEndItem->setText(1, QString::number(newSamp - 1));
    m_lastChartX = qBound(0, m_lastChartX, qMax(0, newTraceCount - 1));
    if (m_transformMode == 3) m_transformMode = 0;   // FFT 分支写死512采样, 归零防越界

    // 6. 视图全面刷新
    tab->editBlocks.clear();
    tab->activeEditBlock = -1;
    m_thumbKey.clear();   // 缩略图缓存失效
    refreshImage();
    updateRulers();
    resizeImageLabel();
    updateTraceRange();
    if (m_headerPanel && m_headerPanel->isVisible()) refreshHeaderPanel();
    syncEditBlocksToView();
    refreshSelectionInfo();
    refreshMarkerPanel();
    updateChart(m_lastChartX);
    QMessageBox::information(this, QString::fromUtf8("确认裁剪"),
        QString::fromUtf8("裁剪完成: %1 道 × %2 采样。保存后将生成新 _P 文件。")
            .arg(newTraceCount).arg(newSamp));
}

// 保存路径头补丁: 按当前 tab 实际值写头(裁剪后防"旧头+新数据"损坏文件)
void MainWindow::patchDztHeaderForTab(QByteArray &header)
{
    if (!m_currentTab || header.size() < 30) return;
    const qint16 ns = (qint16)m_currentTab->nsamp;
    memcpy(header.data() + 4, &ns, 2);
    const qint32 nt = (qint32)m_currentTab->traceCount;
    memcpy(header.data() + 20, &nt, 4);
    const float rg = m_currentTab->headerRange;
    memcpy(header.data() + 26, &rg, 4);
}

// v1.0.98 右侧 256px 编辑属性面板: 页0=编辑数据块(新建矩形框/选区几何/重置/确认裁剪) 页1=横向缩放
void MainWindow::createEditPanel()
{
    m_editPanel = new QWidget(this);
    m_editPanel->setFixedWidth(256);
    m_editPanel->setStyleSheet("#gprEditPanel { background: #f8f9ff; }");

    QHBoxLayout *shell = new QHBoxLayout(m_editPanel);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    // 左缘 1px 描边: 实体色条(QSS border 在普通 QWidget 不可靠)
    QWidget *leftEdge = new QWidget(m_editPanel);
    leftEdge->setFixedWidth(1);
    leftEdge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    leftEdge->setStyleSheet("background: #c3c6d6;");
    shell->addWidget(leftEdge);
    QWidget *inner = new QWidget(m_editPanel);
    QVBoxLayout *outer = new QVBoxLayout(inner);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- 标题栏 40px: 标题(随页切换) + ✕ ---
    QWidget *head = new QWidget(m_editPanel);
    head->setFixedHeight(40);
    head->setStyleSheet("background: #eff4ff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 4, 0);
    hl->setSpacing(8);
    m_editTitleLbl = new QLabel(QString::fromUtf8("编辑数据块"), head);
    m_editTitleLbl->setStyleSheet("font-size: 11px; font-weight: bold; color: #121c2a;"
                                  " letter-spacing: 1px; border: none; background: transparent;");
    hl->addWidget(m_editTitleLbl);
    hl->addStretch(1);
    QToolButton *closeBtn = new QToolButton(head);
    if (MatIcon::ready())
        closeBtn->setIcon(MatIcon::icon(QStringLiteral("close"), QColor(0x73, 0x77, 0x85), QColor(),
                                        QColor(0x12, 0x1c, 0x2a), 16));
    closeBtn->setIconSize(QSize(16, 16));
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QToolButton { border: none; border-radius: 2px; background: transparent; }"
        "QToolButton:hover { background: #dee9fc; }");
    connect(closeBtn, &QToolButton::clicked, this, [this]() {
        if (m_btnEditBlock) m_btnEditBlock->setChecked(false);
        if (m_btnHZoom) m_btnHZoom->setChecked(false);
        syncEditUiState();
    });
    hl->addWidget(closeBtn);
    outer->addWidget(head);

    m_editStack = new QStackedWidget(m_editPanel);

    // ---- 页0: 编辑数据块 ----
    {
        QWidget *page = new QWidget;
        QVBoxLayout *pl = new QVBoxLayout(page);
        pl->setContentsMargins(16, 16, 16, 16);
        pl->setSpacing(12);

        m_btnNewRect = new QPushButton(QString::fromUtf8("  新建矩形框"), page);
        if (MatIcon::ready())
            m_btnNewRect->setIcon(MatIcon::icon(QStringLiteral("add_box"), Qt::white));
        m_btnNewRect->setCursor(Qt::PointingHandCursor);
        m_btnNewRect->setStyleSheet(
            "QPushButton { background: #0048af; color: #ffffff; border: none; border-radius: 4px;"
            " padding: 8px; font-size: 14px; font-weight: bold; text-align: center; }"
            "QPushButton:hover { background: #1e60d5; }");
        pl->addWidget(m_btnNewRect);

        QLabel *geoCap = new QLabel(QString::fromUtf8("选区几何信息"), page);
        geoCap->setStyleSheet("color: #424654; font-size: 11px; font-weight: bold;"
                              " letter-spacing: 1px; border: none; background: transparent;");
        pl->addWidget(geoCap);

        QGridLayout *grid = new QGridLayout;
        grid->setSpacing(6);
        auto geoRow = [this, &grid](int row, const QString &key, QLabel **valLbl) {
            QLabel *k = new QLabel(key);
            k->setStyleSheet("color: #424654; font-size: 12px; border: none; background: transparent;");
            *valLbl = new QLabel("-");
            (*valLbl)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            (*valLbl)->setStyleSheet("color: #121c2a; font-size: 12px; border: none; background: transparent;");
            if (MatIcon::ready()) (*valLbl)->setFont(MatIcon::monoFont(12));
            grid->addWidget(k, row, 0);
            grid->addWidget(*valLbl, row, 1);
        };
        geoRow(0, QString::fromUtf8("起始道号:"), &m_selStartLbl);
        geoRow(1, QString::fromUtf8("终止道号:"), &m_selEndLbl);
        geoRow(2, QString::fromUtf8("时间范围:"), &m_selTimeLbl);
        geoRow(3, QString::fromUtf8("切片尺寸:"), &m_selSizeLbl);
        grid->setColumnStretch(1, 1);
        pl->addLayout(grid);

        pl->addStretch(1);

        m_btnResetRect = new QPushButton(QString::fromUtf8("  重置选区"), page);
        if (MatIcon::ready())
            m_btnResetRect->setIcon(MatIcon::icon(QStringLiteral("restart_alt"), QColor(0x12, 0x1c, 0x2a)));
        m_btnResetRect->setCursor(Qt::PointingHandCursor);
        m_btnResetRect->setStyleSheet(
            "QPushButton { background: #ffffff; color: #121c2a; border: 1px solid #c3c6d6;"
            " border-radius: 4px; padding: 7px; font-size: 13px; }"
            "QPushButton:hover { background: #dee9fc; }");
        pl->addWidget(m_btnResetRect);

        m_btnCrop = new QPushButton(QString::fromUtf8("确认裁剪"), page);
        m_btnCrop->setCursor(Qt::PointingHandCursor);
        m_btnCrop->setStyleSheet(
            "QPushButton { background: #0048af; color: #ffffff; border: none; border-radius: 4px;"
            " padding: 8px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background: #1e60d5; }");
        pl->addWidget(m_btnCrop);

        // v1.0.107: 数据块按钮接线(新建=新块自动错位; 重置=清空; 确认裁剪=裁到保留块)
        connect(m_btnNewRect, &QPushButton::clicked, this, [this]() { createNewEditBlock(); });
        connect(m_btnResetRect, &QPushButton::clicked, this, [this]() { clearEditBlocks(); });
        connect(m_btnCrop, &QPushButton::clicked, this, [this]() { performCropSelection(); });

        m_editStack->addWidget(page);
    }

    // ---- 页1: 横向缩放 ----
    {
        QWidget *page = new QWidget;
        QVBoxLayout *pl = new QVBoxLayout(page);
        pl->setContentsMargins(16, 16, 16, 16);
        pl->setSpacing(12);

        QLabel *cap = new QLabel(QString::fromUtf8("横向缩放控制"), page);
        cap->setStyleSheet("color: #424654; font-size: 11px; font-weight: bold;"
                           " letter-spacing: 1px; border: none; background: transparent;");
        pl->addWidget(cap);

        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(8);
        QLabel *lo = new QLabel("1x");
        QLabel *hi = new QLabel("10x");
        for (QLabel *l : { lo, hi }) {
            l->setStyleSheet("color: #424654; font-size: 12px; border: none; background: transparent;");
            if (MatIcon::ready()) l->setFont(MatIcon::monoFont(12));
        }
        m_hZoomSlider = new QSlider(Qt::Horizontal, page);
        m_hZoomSlider->setRange(1, 10);
        m_hZoomSlider->setValue(1);
        m_hZoomSlider->setStyleSheet(
            "QSlider::groove:horizontal { height: 4px; background: #c3c6d6; border-radius: 2px; }"
            "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0;"
            " border-radius: 7px; background: #0048af; }"
            "QSlider::sub-page:horizontal { background: #7ea6e8; border-radius: 2px; }");
        m_hZoomSpin = new QSpinBox(page);
        m_hZoomSpin->setRange(1, 10);
        m_hZoomSpin->setValue(1);
        m_hZoomSpin->setSuffix("x");
        m_hZoomSpin->setFixedWidth(56);
        if (MatIcon::ready()) m_hZoomSpin->setFont(MatIcon::monoFont(12));
        row->addWidget(lo);
        row->addWidget(m_hZoomSlider, 1);
        row->addWidget(m_hZoomSpin);
        row->addWidget(hi);
        pl->addLayout(row);

        QPushButton *btnResetZoom = new QPushButton(QString::fromUtf8("  重置缩放"), page);
        if (MatIcon::ready())
            btnResetZoom->setIcon(MatIcon::icon(QStringLiteral("restart_alt"), QColor(0x12, 0x1c, 0x2a)));
        btnResetZoom->setCursor(Qt::PointingHandCursor);
        btnResetZoom->setStyleSheet(
            "QPushButton { background: #dee9fc; color: #121c2a; border: 1px solid #c3c6d6;"
            " border-radius: 4px; padding: 6px; font-size: 13px; }"
            "QPushButton:hover { background: #d9e3f6; }");
        pl->addWidget(btnResetZoom);

        pl->addStretch(1);
        m_editStack->addWidget(page);

        connect(m_hZoomSlider, &QSlider::valueChanged, this, [this](int v) {
            if (!m_currentTab) return;
            QSignalBlocker b(m_hZoomSpin);
            m_hZoomSpin->setValue(v);
            setHZoom(float(v));
        });
        connect(m_hZoomSpin, &QSpinBox::valueChanged, this, [this](int v) {
            if (!m_currentTab) return;
            QSignalBlocker b(m_hZoomSlider);
            m_hZoomSlider->setValue(v);
            setHZoom(float(v));
        });
        connect(btnResetZoom, &QPushButton::clicked, this, [this]() { setHZoom(1.0f); });
    }

    outer->addWidget(m_editStack, 1);
    shell->addWidget(inner, 1);
}

// ---- v1.0.98 MarkerThumbWidget: 底图 + 红标记线 + 蓝视口框 ----

MarkerThumbWidget::MarkerThumbWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(40);
    setMouseTracking(true);   // hover 视口框变光标
}

void MarkerThumbWidget::setSource(const QImage &img) { m_thumb = img; update(); }
void MarkerThumbWidget::setMarkers(const QVector<int> &traces) { m_markers = traces; update(); }
void MarkerThumbWidget::setTraceCount(int n) { m_traceCount = n; update(); }
void MarkerThumbWidget::setSampleCount(int n) { m_sampleCount = n; update(); }
void MarkerThumbWidget::setBlocks(const QVector<QRectF> &blocksTS)
{
    m_blocks = blocksTS;
    update();
}
void MarkerThumbWidget::setViewportRange(double x0Frac, double x1Frac)
{
    m_vpX0 = qBound(0.0, x0Frac, 1.0);
    m_vpX1 = qBound(m_vpX0, x1Frac, 1.0);
    update();
}

void MarkerThumbWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xd9, 0xe3, 0xf6));
    const QRect inner = rect().adjusted(1, 1, -1, -1);
    if (inner.width() < 4 || inner.height() < 4) return;
    if (!m_thumb.isNull())
        p.drawImage(inner, m_thumb);

    // 视口指示框(蓝)
    const int x0 = 1 + qRound(m_vpX0 * inner.width());
    const int x1 = 1 + qRound(m_vpX1 * inner.width());
    p.setPen(QPen(QColor(0, 0x48, 0xaf), 2));
    p.setBrush(QColor(0, 0x48, 0xaf, 26));
    p.drawRect(QRect(QPoint(x0, 1), QPoint(qMax(x0 + 2, x1), height() - 1)));

    // 标记线(红)
    p.setPen(QPen(QColor(0xb3, 0x27, 0x2d), 2));
    for (int t : m_markers) {
        if (m_traceCount <= 1) break;
        const int x = 1 + qRound((double)t / (m_traceCount - 1) * inner.width());
        p.drawLine(x, 1, x, height() - 1);
    }

    // 数据块(蓝): trace→x, sample→y 等比映射
    if (m_traceCount > 1 && m_sampleCount > 1) {
        p.setPen(QPen(QColor(0x00, 0x48, 0xaf), 1));
        p.setBrush(Qt::NoBrush);
        for (const QRectF &b : m_blocks) {
            const QRectF bn = b.normalized();
            const int x1 = 1 + qRound(bn.left() / (m_traceCount - 1) * inner.width());
            const int x2 = 1 + qRound(bn.right() / (m_traceCount - 1) * inner.width());
            const int y1 = 1 + qRound(bn.top() / (m_sampleCount - 1) * inner.height());
            const int y2 = 1 + qRound(bn.bottom() / (m_sampleCount - 1) * inner.height());
            p.drawRect(QRect(QPoint(qMin(x1, x2), qMin(y1, y2)),
                             QPoint(qMax(x1, x2), qMax(y1, y2))));
        }
    }

    // 外框(留白后的小窗边界)
    p.setPen(QPen(QColor(0xc3, 0xc6, 0xd6), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MarkerThumbWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && width() > 0) {
        const double frac = (double)event->pos().x() / width();
        if (frac >= m_vpX0 - 0.01 && frac <= m_vpX1 + 0.01) {
            // 按住视口框拖动 → 主图视窗跟随
            m_vpDrag = true;
            m_vpGrabOffset = frac - (m_vpX0 + m_vpX1) / 2.0;
        } else {
            emit viewportJumpRequested(qBound(0.0, frac, 1.0));
        }
    }
    QWidget::mousePressEvent(event);
}

void MarkerThumbWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (width() <= 0) { QWidget::mouseMoveEvent(event); return; }
    const double frac = qBound(0.0, (double)event->pos().x() / width(), 1.0);
    if (m_vpDrag && (event->buttons() & Qt::LeftButton)) {
        emit viewportDragRequested(qBound(0.0, frac - m_vpGrabOffset, 1.0));
    } else if (!(event->buttons() & Qt::LeftButton)) {
        const bool inBox = frac >= m_vpX0 && frac <= m_vpX1;
        setCursor(inBox ? Qt::SizeAllCursor : Qt::PointingHandCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void MarkerThumbWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_vpDrag = false;
    QWidget::mouseReleaseEvent(event);
}

// 从同名 DZX 读 <unitsPerScan>(米/道) — 标记距离列兜底(DZT spm 实测可能为 0)
double MainWindow::readDzxUnitsPerScan(const QString &dztPath)
{
    QFileInfo fi(dztPath);
    QFile f(fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX");
    if (!f.open(QIODevice::ReadOnly)) return 0.0;
    QXmlStreamReader r(&f);
    bool inUnits = false;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            if (r.name() == QStringLiteral("unitsPerScan")) inUnits = true;
        } else if (tok == QXmlStreamReader::Characters && inUnits) {
            bool ok = false;
            const double v = r.text().toDouble(&ok);
            if (ok && v > 0) return v;
        } else if (tok == QXmlStreamReader::EndElement) {
            inUnits = false;
        }
    }
    return 0.0;
}

// 读 DZX Profile/WayPt(RADAN原生): 仅取有 name 子元素的条目(用户标记, 排除边界 distance 点);
// 按 scan 排序, 序号 1/2/3... 由显示层分配
QVector<int> MainWindow::readDzxMarkers(const QString &dztPath)
{
    QVector<int> markers;
    QFileInfo fi(dztPath);
    QFile f(fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX");
    if (!f.open(QIODevice::ReadOnly)) return markers;
    QXmlStreamReader r(&f);
    bool inProfile = false, inWayPt = false, hasName = false;
    int curScan = -1;
    bool hasScan = false;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tok = r.readNext();
        const QString name = r.name().toString();
        if (tok == QXmlStreamReader::StartElement) {
            if (name == QStringLiteral("Profile")) inProfile = true;
            else if (inProfile && name == QStringLiteral("WayPt")) {
                inWayPt = true; hasName = false; hasScan = false; curScan = -1;
            } else if (inWayPt && name == QStringLiteral("scan")) {
                // 读完 scan 文本
                if (r.readNext() == QXmlStreamReader::Characters) {
                    bool ok = false;
                    const int v = r.text().toInt(&ok);
                    if (ok && v >= 0) { curScan = v; hasScan = true; }
                }
            } else if (inWayPt && name == QStringLiteral("name")) {
                hasName = true;   // 有 name 子元素 = 用户标记(非边界点)
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            if (name == QStringLiteral("WayPt") && inWayPt) {
                if (hasName && hasScan) markers.append(curScan);
                inWayPt = false;
            } else if (name == QStringLiteral("Profile")) inProfile = false;
        }
    }
    std::sort(markers.begin(), markers.end());
    return markers;
}

// 标记写回 DZX: RADAN原生 WayPt 格式(在 Profile 内插入, 同时补 WayPtNameProperties)
bool MainWindow::writeDzxMarkers(const QString &dztPath, const QVector<int> &markers)
{
    QFileInfo fi(dztPath);
    const QString dzxPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX";

    QByteArray content;
    QFile f(dzxPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        content = f.readAll();
        f.close();
    }
    if (content.isEmpty()) {
        // 新建最小 DZX(含 Profile 容器)
        if (!f.open(QIODevice::WriteOnly)) return false;
        QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
                         "<DZX xmlns=\"www.geophysical.com/DZX/1.02\">\r\n"
                         "  <WayPtNameProperties>\r\n    <name></name>\r\n"
                         "    <readOnly>0</readOnly><hide>0</hide><onePerFile>0</onePerFile><paired>0</paired>\r\n"
                         "  </WayPtNameProperties>\r\n"
                         "  <File>\r\n    <Profile>\r\n";
        for (int i = 0; i < markers.size(); ++i) {
            xml += QStringLiteral("      <WayPt>\r\n        <scan>%1</scan>\r\n"
                                  "        <mark>Combo</mark>\r\n"
                                  "        <name>MARK%2</name>\r\n      </WayPt>\r\n")
                       .arg(markers[i])
                       .arg(i + 1, 2, 10, QChar('0'))
                       .toUtf8();
            xml += QStringLiteral("  <WayPtNameProperties>\r\n    <name>MARK%1</name>\r\n"
                                  "    <readOnly>0</readOnly><hide>0</hide>"
                                  "<onePerFile>0</onePerFile><paired>0</paired>\r\n"
                                  "  </WayPtNameProperties>\r\n")
                       .arg(i + 1, 2, 10, QChar('0'))
                       .toUtf8();
        }
        xml += "    </Profile>\r\n  </File>\r\n</DZX>\r\n";
        f.write(xml);
        f.close();
        syncDzxMtimeToDzt(dztPath, dzxPath);
        return true;
    }

    // ===== 已有 DZX: 文本级手术 =====
    // 1. 删 Profile 内所有有 name 的 WayPt(用户标记), 保留边界点(只有 scan+distance)
    {
        // 逐个找 <WayPt>...</WayPt> 区段, 有 <name> 的删除
        int s = 0;
        while ((s = content.indexOf("<WayPt>", s)) >= 0) {
            const int e = content.indexOf("</WayPt>", s);
            if (e < 0) break;
            const QByteArray seg = content.mid(s, e - s + 8);
            if (seg.contains("<name>")) {
                // 含换行回车的完整 WayPt 块(含前导空白)
                int ls = s;
                while (ls > 0 && (content[ls-1] == ' ' || content[ls-1] == '\t')) ls--;
                if (ls > 0 && content[ls-1] == '\n') ls--;
                if (ls > 0 && content[ls-1] == '\r') ls--;
                const int le = e + 8;
                if (le < content.size() && content[le] == '\r') {
                    content.remove(ls, le + 1 - ls);
                } else if (le < content.size() && content[le] == '\n') {
                    content.remove(ls, le + 1 - ls);
                } else {
                    content.remove(ls, le - ls);
                }
                // 不推进 s(删了可能紧跟下一块)
            } else {
                s = e + 8;
            }
        }
    }

    // 2. 删已有 MARK 命名的 WayPtNameProperties(重建)
    {
        int s = 0;
        while ((s = content.indexOf("<WayPtNameProperties>", s)) >= 0) {
            const int e = content.indexOf("</WayPtNameProperties>", s);
            if (e < 0) break;
            const QByteArray seg = content.mid(s, e - s + 22);
            if (seg.contains("MARK")) {
                int ls = s;
                while (ls > 0 && (content[ls-1] == ' ' || content[ls-1] == '\t')) ls--;
                if (ls > 0 && content[ls-1] == '\n') ls--;
                if (ls > 0 && content[ls-1] == '\r') ls--;
                const int le = e + 22;
                if (le < content.size() && content[le] == '\r') {
                    content.remove(ls, le + 1 - ls);
                } else if (le < content.size() && content[le] == '\n') {
                    content.remove(ls, le + 1 - ls);
                } else {
                    content.remove(ls, le - ls);
                }
            } else {
                s = e + 22;
            }
        }
    }

    // 3. 在 </Profile> 前插入新 WayPt 块
    {
        QString wpts;
        for (int i = 0; i < markers.size(); ++i) {
            wpts += QStringLiteral("      <WayPt>\r\n        <scan>%1</scan>\r\n"
                                   "        <mark>Combo</mark>\r\n"
                                   "        <name>MARK%2</name>\r\n      </WayPt>\r\n")
                        .arg(markers[i])
                        .arg(i + 1, 2, 10, QChar('0'));
        }
        const int e = content.lastIndexOf("</Profile>");
        if (e >= 0) content.insert(e, wpts.toUtf8());
    }

    // 4. 在 <File> 前插入 WayPtNameProperties 块(与 Profile 中标记一一对应)
    {
        QString props;
        for (int i = 0; i < markers.size(); ++i) {
            props += QStringLiteral("  <WayPtNameProperties>\r\n    <name>MARK%1</name>\r\n"
                                    "    <readOnly>0</readOnly>\r\n    <hide>0</hide>\r\n"
                                    "    <onePerFile>0</onePerFile>\r\n    <paired>0</paired>\r\n"
                                    "  </WayPtNameProperties>\r\n")
                         .arg(i + 1, 2, 10, QChar('0'));
        }
        const int e = content.indexOf("<File>");
        if (e >= 0) content.insert(e, props.toUtf8());
    }

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content);
    f.close();

    if (!content.contains("<BinaryData"))
        syncDzxMtimeToDzt(dztPath, dzxPath);
    return true;
}

// Windows: 把 DZX 时间戳设为与 DZT 完全一致(QFile 无此 API, 借 Win32)
bool MainWindow::syncDzxMtimeToDzt(const QString &dztPath, const QString &dzxPath)
{
#ifdef Q_OS_WIN
    bool ok = false;
    HANDLE hd = CreateFileW((const wchar_t *)dztPath.utf16(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hd != INVALID_HANDLE_VALUE) {
        HANDLE hx = CreateFileW((const wchar_t *)dzxPath.utf16(), GENERIC_WRITE, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hx != INVALID_HANDLE_VALUE) {
            FILETIME ct, at, wt;
            if (GetFileTime(hd, &ct, &at, &wt))
                ok = SetFileTime(hx, &ct, &at, &wt) != FALSE;
            CloseHandle(hx);
        }
        CloseHandle(hd);
    }
    return ok;
#else
    Q_UNUSED(dztPath); Q_UNUSED(dzxPath);
    return false;
#endif
}

// ---- v1.0.108 InterpGroup(层位曲线+异常标注) DZX 读写 ----

// 读 DZX <InterpGroup>: Horizon(name/color/width/visible/dashed)+Pt(scan,samp);
//                       Anomaly(shape/name/remark/color/font/fontSize)+Rect(t0,s0,t1,s1)或Pt多边形
bool MainWindow::readDzxInterp(const QString &dztPath,
                               QVector<HorizonLayer> &horizons, QVector<AnomalyMark> &anomalies)
{
    horizons.clear();
    anomalies.clear();
    QFileInfo fi(dztPath);
    QFile f(fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX");
    if (!f.open(QIODevice::ReadOnly)) return false;
    QXmlStreamReader r(&f);
    enum { None, InGroup, InHorizon, InAnomaly } zone = None;
    QString curElem;
    HorizonLayer h;
    AnomalyMark a;
    bool haveH = false, haveA = false;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tok = r.readNext();
        const QString name = r.name().toString();
        if (tok == QXmlStreamReader::StartElement) {
            curElem = name;
            if (name == QStringLiteral("InterpGroup")) zone = InGroup;
            else if (zone == InGroup && name == QStringLiteral("Horizon")) {
                h = HorizonLayer();
                h.name = r.attributes().value(QStringLiteral("name")).toString();
                h.color.setNamedColor(r.attributes().value(QStringLiteral("color")).toString());
                h.lineWidth = r.attributes().value(QStringLiteral("width")).toInt();
                if (h.lineWidth < 1 || h.lineWidth > 5) h.lineWidth = 2;
                h.visible = r.attributes().value(QStringLiteral("visible")).toInt() != 0;
                h.dashed = r.attributes().value(QStringLiteral("dashed")).toInt() != 0;
                zone = InHorizon; haveH = true;
            } else if (zone == InGroup && name == QStringLiteral("Anomaly")) {
                a = AnomalyMark();
                a.shape = r.attributes().value(QStringLiteral("shape")).toInt();
                a.name = r.attributes().value(QStringLiteral("name")).toString();
                a.remark = r.attributes().value(QStringLiteral("remark")).toString();
                a.color.setNamedColor(r.attributes().value(QStringLiteral("color")).toString());
                a.fontFamily = r.attributes().value(QStringLiteral("font")).toString();
                a.fontSize = r.attributes().value(QStringLiteral("fontSize")).toInt();
                if (a.fontSize <= 0) a.fontSize = 12;
                zone = InAnomaly; haveA = true;
            } else if (zone == InHorizon && name == QStringLiteral("Pt")) {
                h.points.append(QPointF(-1, -1));   // 占位, scan/samp 文本填充
            } else if (zone == InAnomaly && name == QStringLiteral("Rect")) {
                const QStringList p = r.readElementText().split(',');
                if (p.size() == 4)
                    a.rect = QRectF(p[0].toDouble(), p[1].toDouble(),
                                    p[2].toDouble() - p[0].toDouble(),
                                    p[3].toDouble() - p[1].toDouble());
            } else if (zone == InAnomaly && name == QStringLiteral("Pt")) {
                a.poly.append(QPointF(-1, -1));
            }
        } else if (tok == QXmlStreamReader::Characters
                   && !r.text().toString().trimmed().isEmpty()) {
            const double v = r.text().toString().trimmed().toDouble();
            if (curElem == QStringLiteral("scan") || curElem == QStringLiteral("samp")) {
                const bool isScan = (curElem == QStringLiteral("scan"));
                if (zone == InHorizon && !h.points.isEmpty())
                    h.points.last().rx() = isScan ? v : h.points.last().x(),
                    h.points.last() = QPointF(isScan ? v : h.points.last().x(),
                                              !isScan ? v : h.points.last().y());
                else if (zone == InAnomaly && !a.poly.isEmpty())
                    a.poly.last() = QPointF(isScan ? v : a.poly.last().x(),
                                            !isScan ? v : a.poly.last().y());
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            if (name == QStringLiteral("Horizon") && zone == InHorizon) {
                horizons.append(h); haveH = false; zone = InGroup;
            } else if (name == QStringLiteral("Anomaly") && zone == InAnomaly) {
                if (a.name != QStringLiteral("__poly_pending__")) anomalies.append(a);
                haveA = false; zone = InGroup;
            } else if (name == QStringLiteral("InterpGroup")) {
                zone = None;
            }
        }
    }
    return true;
}

// 写 DZX <InterpGroup> (文本级手术, 同 MarkGroup 模式)
bool MainWindow::writeDzxInterp(const QString &dztPath,
                                const QVector<HorizonLayer> &horizons,
                                const QVector<AnomalyMark> &anomalies)
{
    QFileInfo fi(dztPath);
    const QString dzxPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX";

    QString block = QStringLiteral("  <InterpGroup>\r\n");
    for (const HorizonLayer &h : horizons) {
        block += QStringLiteral("    <Horizon name=\"%1\" color=\"%2\" width=\"%3\" visible=\"%4\" dashed=\"%5\">\r\n")
                     .arg(h.name.toHtmlEscaped(), h.color.name())
                     .arg(h.lineWidth).arg(h.visible ? 1 : 0).arg(h.dashed ? 1 : 0);
        for (const QPointF &p : h.points)
            block += QStringLiteral("      <Pt><scan>%1</scan><samp>%2</samp></Pt>\r\n")
                         .arg(qRound(p.x())).arg(qRound(p.y()));
        block += QStringLiteral("    </Horizon>\r\n");
    }
    for (const AnomalyMark &a : anomalies) {
        if (a.name == QStringLiteral("__poly_pending__")) continue;
        block += QStringLiteral("    <Anomaly shape=\"%1\" name=\"%2\" remark=\"%3\" color=\"%4\" font=\"%5\" fontSize=\"%6\">\r\n")
                     .arg(a.shape).arg(a.name.toHtmlEscaped(), a.remark.toHtmlEscaped(),
                                       a.color.name(), a.fontFamily.toHtmlEscaped())
                     .arg(a.fontSize);
        if (a.shape == 2) {
            for (const QPointF &p : a.poly)
                block += QStringLiteral("      <Pt><scan>%1</scan><samp>%2</samp></Pt>\r\n")
                             .arg(qRound(p.x())).arg(qRound(p.y()));
        } else {
            const QRectF rn = a.rect.normalized();
            block += QStringLiteral("      <Rect>%1,%2,%3,%4</Rect>\r\n")
                         .arg(qRound(rn.left())).arg(qRound(rn.top()))
                         .arg(qRound(rn.right())).arg(qRound(rn.bottom()));
        }
        block += QStringLiteral("    </Anomaly>\r\n");
    }
    block += QStringLiteral("  </InterpGroup>\r\n");

    QByteArray content;
    QFile f(dzxPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        content = f.readAll();
        f.close();
    }
    if (!content.isEmpty()) {
        const QByteArray blockUtf8 = block.toUtf8();
        const int s = content.indexOf("<InterpGroup>");
        if (s >= 0) {
            const int e = content.indexOf("</InterpGroup>", s);
            if (e < 0) return false;
            content.replace(s, e + 14 - s, blockUtf8);
        } else {
            const int e = content.lastIndexOf("</DZX>");
            if (e < 0) return false;
            content.insert(e, blockUtf8);
        }
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(content);
        f.close();
    } else {
        if (!f.open(QIODevice::WriteOnly)) return false;
        QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
                         "<DZX xmlns=\"www.geophysical.com/DZX/1.02\">\r\n";
        xml += block.toUtf8();
        xml += "</DZX>\r\n";
        f.write(xml);
        f.close();
    }
    if (!content.contains("<BinaryData"))
        syncDzxMtimeToDzt(dztPath, dzxPath);
    return true;
}

// 解译数据内存提交(仅刷新显示, 不写 DZX — RADAN规律)
void MainWindow::commitInterp()
{
    if (!m_currentTab) return;
    syncInterpOverlays();
}

// RADAN规律: 关闭/切换文件时一次性把层位(LayerGroup)+异常(InterpGroup)写入 DZX
void MainWindow::flushInterpToDzx(TabData *tab)
{
    if (!tab || tab->filePath.isEmpty()) return;
    writeDzxDLayers(tab->filePath, tab->radanLayers);
    writeDzxInterp(tab->filePath, tab->horizons, tab->anomalies);
}

// 读取 DZX <LayerGroup>/<LayerWayPt> — RADAN 原生层位点(scanSampChanProp="scan,samp,chan,prop")
// 颜色固定映射: 0黄 1红 2绿 3蓝 4棕 5黑 6白 (≥7灰兜底); 只读展示不进面板
bool MainWindow::readDzxDLayers(const QString &dztPath, QVector<HorizonLayer> &radanLayers)
{
    radanLayers.clear();
    QFileInfo fi(dztPath);
    QFile f(fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX");
    if (!f.open(QIODevice::ReadOnly)) return false;

    static const QColor layerColors[] = {
        QColor(0xff, 0xff, 0x00),   // 0 黄
        QColor(0xff, 0x00, 0x00),   // 1 红
        QColor(0x00, 0xff, 0x00),   // 2 绿
        QColor(0x00, 0x00, 0xff),   // 3 蓝
        QColor(0xa0, 0x52, 0x2d),   // 4 棕
        QColor(0x00, 0x00, 0x00),   // 5 黑
        QColor(0xff, 0xff, 0xff),   // 6 白
        QColor(0x80, 0x80, 0x80),   // ≥7 灰
    };

    QXmlStreamReader r(&f);
    bool inGroup = false;
    HorizonLayer cur;
    int layerIdx = 0;
    QString curElem;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tok = r.readNext();
        const QString name = r.name().toString();
        if (tok == QXmlStreamReader::StartElement) {
            if (name == QStringLiteral("LayerGroup")) {
                inGroup = true;
                cur = HorizonLayer();
                cur.name = QStringLiteral("第 %1 层").arg(radanLayers.size() + 1);
                cur.visible = true;
                cur.lineWidth = 5;
            } else if (inGroup && name == QStringLiteral("LayerWayPt")) {
                // 读子元素 scanSampChanProp
                while (!r.atEnd()) {
                    const auto t2 = r.readNext();
                    if (t2 == QXmlStreamReader::StartElement
                        && r.name() == QStringLiteral("scanSampChanProp")) {
                        const QStringList parts = r.readElementText().split(',');
                        if (parts.size() >= 2)
                            cur.points.append(QPointF(parts[0].toDouble(), parts[1].toDouble()));
                    } else if (t2 == QXmlStreamReader::EndElement
                               && r.name() == QStringLiteral("LayerWayPt")) {
                        break;
                    }
                }
            } else {
                curElem = name;
            }
        } else if (tok == QXmlStreamReader::Characters && inGroup && !r.text().toString().trimmed().isEmpty()) {
            const QString txt = r.text().toString().trimmed();
            if (curElem == QStringLiteral("groupName")) {
                cur.name = txt;
            } else if (curElem == QStringLiteral("layerNum")) {
                layerIdx = txt.toInt();
            } else if (curElem == QStringLiteral("display")) {
                cur.visible = txt.toInt() != 0;
            } else if (curElem == QStringLiteral("sizePx")) {
                cur.lineWidth = qBound(3, txt.toInt(), 10);
            }
        } else if (tok == QXmlStreamReader::EndElement && name == QStringLiteral("LayerGroup") && inGroup) {
            const int ci = qBound(0, layerIdx, 7);
            cur.color = layerColors[ci];
            cur.layerNum = layerIdx;
            if (cur.name.isEmpty() || cur.name.startsWith(QStringLiteral("第 ")))
                cur.name = QStringLiteral("第%1层").arg(radanLayers.size() + 1);
            radanLayers.append(cur);
            inGroup = false;
        }
    }
    return true;
}

// 写回 DZX LayerGroup(文本级手术: 删旧全部 LayerGroup → 在 TargetGroup/DataCollection 前插新)
bool MainWindow::writeDzxDLayers(const QString &dztPath, const QVector<HorizonLayer> &radanLayers)
{
    QFileInfo fi(dztPath);
    const QString dzxPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX";

    QByteArray content;
    QFile f(dzxPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        content = f.readAll();
        f.close();
    }
    if (content.isEmpty()) return false;

    // 1. 删除所有 <LayerGroup>...</LayerGroup> 区段
    {
        int s = 0;
        while ((s = content.indexOf("<LayerGroup>")) >= 0) {
            const int e = content.indexOf("</LayerGroup>", s);
            if (e < 0) break;
            const int le = e + 13;
            int ls = s;
            while (ls > 0 && (content[ls-1] == ' ' || content[ls-1] == '\t')) ls--;
            if (ls > 0 && content[ls-1] == '\n') ls--;
            if (ls > 0 && content[ls-1] == '\r') ls--;
            if (le < content.size() && content[le] == '\r') content.remove(ls, le + 1 - ls);
            else if (le < content.size() && content[le] == '\n') content.remove(ls, le + 1 - ls);
            else content.remove(ls, le - ls);
        }
    }

    // 2. 构建新 LayerGroup 块
    QString block;
    for (const HorizonLayer &h : radanLayers) {
        block += QStringLiteral("  <LayerGroup>\r\n");
        block += QStringLiteral("    <groupName>%1</groupName>\r\n").arg(h.name.toHtmlEscaped());
        block += QStringLiteral("    <display>%1</display>\r\n").arg(h.visible ? 1 : 0);
        block += QStringLiteral("    <color>%1</color>\r\n").arg(h.layerNum + 1);
        block += QStringLiteral("    <sizePx>%1</sizePx>\r\n").arg(h.lineWidth);
        block += QStringLiteral("    <outline>0</outline>\r\n");
        block += QStringLiteral("    <readOnly>0</readOnly>\r\n");
        block += QStringLiteral("    <diameter>0.0000000</diameter>\r\n");
        block += QStringLiteral("    <link>1</link>\r\n");
        block += QStringLiteral("    <pickType>0</pickType>\r\n");
        block += QStringLiteral("    <layerNum>%1</layerNum>\r\n").arg(h.layerNum);
        block += QStringLiteral("    <velMethod>0</velMethod>\r\n");
        block += QStringLiteral("    <lockVelocity>0</lockVelocity>\r\n");
        block += QStringLiteral("    <defaultVelocity>0.1060000</defaultVelocity>\r\n");
        for (const QPointF &p : h.points) {
            block += QStringLiteral("    <LayerWayPt>\r\n"
                                    "      <scanSampChanProp>%1,%2,0,0</scanSampChanProp>\r\n"
                                    "    </LayerWayPt>\r\n")
                         .arg(qRound(p.x())).arg(qRound(p.y()));
        }
        block += QStringLiteral("  </LayerGroup>\r\n");
    }

    // 3. 插入到 <TargetGroup> 或 <DataCollection> 或 </DZX> 之前
    int pos = content.indexOf("<TargetGroup>");
    if (pos < 0) pos = content.indexOf("<DataCollection>");
    if (pos < 0) pos = content.lastIndexOf("</DZX>");
    if (pos < 0) return false;
    content.insert(pos, block.toUtf8());

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content);
    f.close();
    if (!content.contains("<BinaryData"))
        syncDzxMtimeToDzt(dztPath, dzxPath);
    return true;
}

// 米/道: DZT spm>0 → 1/spm; 否则 DZX unitsPerScan; 都无 → 0
double MainWindow::markerSpacingM()
{
    if (!m_currentTab) return 0.0;
    DztHeaderInfo info;
    if (readDztHeaderInfo(info) && info.spm > 0) return 1.0 / info.spm;
    return readDzxUnitsPerScan(m_currentTab->filePath);
}

// 底部标记面板: 左40%标记表(3列+插删按钮) + 右60%雷达缩略图
void MainWindow::createMarkerPanel()
{
    m_markerPanel = new QWidget(this);
    m_markerPanel->setStyleSheet("#gprMarkerPanel { background: #f8f9ff; }");
    m_markerPanel->setFixedHeight(220);   // 初始, resizeEvent 按 35% 调整

    QVBoxLayout *panelLay = new QVBoxLayout(m_markerPanel);
    panelLay->setContentsMargins(0, 0, 0, 0);
    panelLay->setSpacing(0);
    // 顶部 4px 浅蓝粗边(HTML border-t-4): 实体色条 — QSS border 在普通 QWidget 不可靠
    QWidget *topBand = new QWidget(m_markerPanel);
    topBand->setFixedHeight(4);
    topBand->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topBand->setStyleSheet("background: #d9e3f6;");
    panelLay->addWidget(topBand);

    QWidget *panelBody = new QWidget(m_markerPanel);
    QHBoxLayout *lay = new QHBoxLayout(panelBody);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    panelLay->addWidget(panelBody, 1);

    // ---- 左 40%: 标记表 ----
    QWidget *left = new QWidget(panelBody);
    left->setStyleSheet("background: #ffffff;");
    QVBoxLayout *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);

    QWidget *lhead = new QWidget(left);
    lhead->setFixedHeight(32);
    lhead->setStyleSheet("background: #eff4ff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *lh = new QHBoxLayout(lhead);
    lh->setContentsMargins(12, 0, 8, 0);
    lh->setSpacing(4);
    QLabel *capL = new QLabel(QString::fromUtf8("标记表"), lhead);
    capL->setStyleSheet("font-size: 14px; font-weight: bold; color: #121c2a; border: none; background: transparent;");
    lh->addWidget(capL);
    lh->addStretch(1);

    auto smallBtn = [this](const QString &glyph, const QString &text, const QString &fg) -> QPushButton * {
        QPushButton *b = new QPushButton(QString::fromUtf8(" ") + text);
        if (MatIcon::ready())
            b->setIcon(MatIcon::icon(glyph, QColor(fg)));
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton { background: #ffffff; border: 1px solid #c3c6d6; border-radius: 3px;"
            " padding: 2px 10px; font-size: 12px; color: %1; }"
            "QPushButton:hover { background: #dee9fc; }").arg(fg));
        return b;
    };
    QPushButton *btnIns = smallBtn(QStringLiteral("add"), QString::fromUtf8("插入标记"), "#121c2a");
    QPushButton *btnDel = smallBtn(QStringLiteral("remove"), QString::fromUtf8("删除标记"), "#ba1a1a");
    lh->addWidget(btnIns);
    lh->addWidget(btnDel);
    ll->addWidget(lhead);

    m_markerTable = new QTableWidget(left);
    m_markerTable->setColumnCount(3);
    m_markerTable->setHorizontalHeaderLabels(QStringList()
        << QString::fromUtf8("序号") << QString::fromUtf8("道号") << QString::fromUtf8("距离 (m)"));
    m_markerTable->verticalHeader()->hide();
    m_markerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_markerTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_markerTable->setStyleSheet(
        "QTableWidget { border: none; background: #ffffff; gridline-color: #e6eeff; font-size: 12px; }"
        "QTableWidget::item { padding: 2px 6px; }"
        "QTableWidget::item:selected { background: #dee9fc; color: #121c2a; }"
        "QHeaderView::section { background: #eff4ff; color: #424654; border: none;"
        " border-bottom: 1px solid #c3c6d6; padding: 4px 6px; font-size: 11px; font-weight: bold; }");
    m_markerTable->horizontalHeader()->setStretchLastSection(true);
    m_markerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_markerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ll->addWidget(m_markerTable, 1);

    // ---- 右 60%: 雷达缩略图 ----
    QWidget *right = new QWidget(m_markerPanel);
    right->setStyleSheet("background: #eff4ff;");
    QVBoxLayout *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);

    QWidget *rhead = new QWidget(right);
    rhead->setFixedHeight(32);
    rhead->setStyleSheet("background: #eff4ff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *rh = new QHBoxLayout(rhead);
    rh->setContentsMargins(12, 0, 8, 0);
    QLabel *capR = new QLabel(QString::fromUtf8("雷达缩略图"), rhead);
    capR->setStyleSheet("font-size: 14px; font-weight: bold; color: #121c2a; border: none; background: transparent;");
    rh->addWidget(capR);
    rl->addWidget(rhead);

    QWidget *rbody = new QWidget(right);
    QVBoxLayout *rbl = new QVBoxLayout(rbody);
    rbl->setContentsMargins(12, 8, 12, 8);     // 左右12px; 上下由弹性间距控制
    rbl->addStretch(2);                        // 上侧留白 20%
    m_markerThumb = new MarkerThumbWidget(rbody);
    m_markerThumb->setFixedHeight(120);        // 横向长条高度(定值, 不再被弹性压到最小40px)
    // 浮影效果(悬浮感)
    auto *thumbShadow = new QGraphicsDropShadowEffect(m_markerThumb);
    thumbShadow->setBlurRadius(12);
    thumbShadow->setOffset(0, 2);
    thumbShadow->setColor(QColor(18, 28, 42, 60));
    m_markerThumb->setGraphicsEffect(thumbShadow);
    rbl->addWidget(m_markerThumb);
    rbl->addStretch(1);                        // 下侧留白 10%
    rl->addWidget(rbody, 1);

    lay->addWidget(left, 40);
    // 标记表与缩略图之间 1px 竖分隔线(HTML border-r border-outline-variant): 实体色条
    QWidget *vline = new QWidget(panelBody);
    vline->setFixedWidth(1);
    vline->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    vline->setStyleSheet("background: #c3c6d6;");
    lay->addWidget(vline);
    lay->addWidget(right, 60);

    // 缩略图点击 → 主图滚动到该处; 按住视口框拖动 → 主图视窗跟随
    connect(m_markerThumb, &MarkerThumbWidget::viewportJumpRequested, this, [this](double frac) {
        if (!m_currentTab) return;
        const int total = m_currentTab->imageLabel->width();
        const int vw = m_currentTab->scrollArea->viewport()->width();
        m_currentTab->extHScrollBar->setValue(qMax(0, qRound(frac * total - vw / 2.0)));
    });
    connect(m_markerThumb, &MarkerThumbWidget::viewportDragRequested, this, [this](double centerFrac) {
        if (!m_currentTab) return;
        const int total = m_currentTab->imageLabel->width();
        const int vw = m_currentTab->scrollArea->viewport()->width();
        m_currentTab->extHScrollBar->setValue(qMax(0, qRound(centerFrac * total - vw / 2.0)));
    });
    // 插入/删除
    connect(btnIns, &QPushButton::clicked, this, [this]() { insertMarkerRow(); });
    connect(btnDel, &QPushButton::clicked, this, [this]() { deleteMarkerRow(); });
    // 道号列编辑(双击) — RADAN规律: 仅更新内存, 关闭/切换时才写 DZX
    connect(m_markerTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
        if (m_fillingMarkers || !it || it->column() != 1 || !m_currentTab) return;
        bool ok = false;
        const int trace = it->text().toInt(&ok);
        if (!ok) { refreshMarkerPanel(); return; }
        const int clamped = qBound(0, trace, qMax(0, m_currentTab->traceCount - 1));
        const int row = it->row();
        if (row >= 0 && row < m_currentTab->markers.size())
            m_currentTab->markers[row] = clamped;
        commitMarkers();   // 排序+刷新显示(不写DZX)
    });

    m_markerPanel->hide();
}

// 标记表 + 主图覆盖层 + 缩略图标记线 全刷新
void MainWindow::refreshMarkerPanel()
{
    if (!m_markerTable) return;
    const QVector<int> mk = m_currentTab ? m_currentTab->markers : QVector<int>();
    const double mPerScan = markerSpacingM();

    m_fillingMarkers = true;
    m_markerTable->setRowCount(mk.size());
    for (int i = 0; i < mk.size(); ++i) {
        auto ro = []() {
            QTableWidgetItem *c = new QTableWidgetItem;
            c->setFlags(c->flags() & ~Qt::ItemIsEditable);
            return c;
        };
        QTableWidgetItem *c0 = ro(); c0->setText(QString::number(i + 1));
        QTableWidgetItem *c1 = new QTableWidgetItem(QString::number(mk[i]));   // 可编辑
        QTableWidgetItem *c2 = ro();
        c2->setText(mPerScan > 0 ? QString::number(mk[i] * mPerScan, 'f', 2) : QStringLiteral("-"));
        for (auto *c : { c0, c1, c2 }) {
            if (MatIcon::ready()) c->setFont(MatIcon::monoFont(12));
            c->setTextAlignment(Qt::AlignCenter);
        }
        m_markerTable->setItem(i, 0, c0);
        m_markerTable->setItem(i, 1, c1);
        m_markerTable->setItem(i, 2, c2);
    }
    m_fillingMarkers = false;

    if (m_currentTab && m_currentTab->imageLabel)
        m_currentTab->imageLabel->setMarkerOverlay(
            (m_btnEditMarker && m_btnEditMarker->isChecked())
                || (m_btnEditBlock && m_btnEditBlock->isChecked()),   // 数据块模式红线也保持
            mk);
    if (m_markerThumb) {
        m_markerThumb->setMarkers(mk);
        m_markerThumb->setTraceCount(m_currentTab ? m_currentTab->traceCount : 0);
    }
}

// 缩略图重建(带缓存) + 视口指示框
void MainWindow::updateMarkerThumb()
{
    if (!m_markerThumb || !m_currentTab) return;
    const int w = qMax(4, m_markerThumb->width());
    const int h = qMax(4, m_markerThumb->height());
    const QString key = QString::number(quintptr(m_currentTab)) + "_" +
        QString::number(m_currentTab->dataRev) + "_" + QString::number(w) + "x" +
        QString::number(h) + "_" + QString::number(m_paletteIndex) + "_" +
        QString::number(m_colorTransformIndex);
    if (key != m_thumbKey || m_thumbCache.isNull()) {
        m_thumbCache = buildBscanThumbnail(w, h);
        m_thumbKey = key;
    }
    m_markerThumb->setSource(m_thumbCache);

    QScrollBar *hb = m_currentTab->extHScrollBar;
    const int vw = m_currentTab->scrollArea->viewport()->width();
    const int total = qMax(1, hb->maximum() + vw);
    m_markerThumb->setViewportRange((double)hb->value() / total,
                                    (double)(hb->value() + vw) / total);
}

// 插入标记: 选中行上方插入(两行之间取前后道号均值), 未选中追加表尾
void MainWindow::insertMarkerRow()
{
    if (!requireOpenFile()) return;
    TabData *tab = m_currentTab;
    const int maxT = qMax(0, tab->traceCount - 1);
    const int row = m_markerTable ? m_markerTable->currentRow() : -1;
    int newTrace;
    if (tab->markers.isEmpty()) {
        newTrace = tab->traceCount / 2;                       // 空表 → 中间
    } else if (row >= 0 && row < tab->markers.size()) {
        if (row == 0)
            newTrace = qMax(0, tab->markers.first() - 1);     // 首行上方 → 首-1
        else
            newTrace = qBound(0, (tab->markers[row - 1] + tab->markers[row] + 1) / 2, maxT);  // 均值
    } else {
        newTrace = qMin(maxT, tab->markers.last() + 1);       // 表尾 → last+1
    }
    tab->markers.insert(qBound(0, (row >= 0 ? row : tab->markers.size()), tab->markers.size()),
                        newTrace);
    // RADAN规律: 操作全在内存, 关闭/切换文件时才写 DZX; 此处仅刷新显示
    std::sort(tab->markers.begin(), tab->markers.end());
    refreshMarkerPanel();
    updateMarkerThumb();
}

// 删除标记: 删除选中行(无选中删最后一行)
void MainWindow::deleteMarkerRow()
{
    if (!requireOpenFile() || m_currentTab->markers.isEmpty()) return;
    const int row = m_markerTable ? m_markerTable->currentRow() : -1;
    const int idx = (row >= 0 && row < m_currentTab->markers.size())
                        ? row : m_currentTab->markers.size() - 1;
    m_currentTab->markers.remove(idx);
    // RADAN规律: 仅内存操作+显示刷新
    refreshMarkerPanel();
    updateMarkerThumb();
}

// RADAN规律: 关闭/切换文件时一次性把标记写入 DZX(WayPt格式)
void MainWindow::flushMarkersToDzx(TabData *tab)
{
    if (!tab || tab->filePath.isEmpty()) return;
    std::sort(tab->markers.begin(), tab->markers.end());
    if (!writeDzxMarkers(tab->filePath, tab->markers)) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            QMessageBox::warning(nullptr, QString::fromUtf8("标记保存"),
                QString::fromUtf8("标记写入 DZX 失败(文件可能被占用/只读), 本次修改仅在内存中生效。"));
        }
    }
}

// 标记提交: 排序+显示刷新(纯内存, 不写DZX)
void MainWindow::commitMarkers()
{
    if (!m_currentTab) return;
    std::sort(m_currentTab->markers.begin(), m_currentTab->markers.end());
    refreshMarkerPanel();
    updateMarkerThumb();
}

// ---- v1.0.108 数据解译叠加 ----

void ImageLabel::setInterpOverlays(const QVector<HorizonLayer> &horizons,
                                   const QVector<AnomalyMark> &anomalies,
                                   const QVector<QPointF> &seeds,
                                   double mPerSample, int selectedAnomaly)
{
    m_horizons = horizons;
    m_anomalies = anomalies;
    m_seeds = seeds;
    m_interpMPerSample = mPerSample;
    m_interpSelectedAnomaly = selectedAnomaly;
    update();
}

void ImageLabel::setRadanLayers(const QVector<HorizonLayer> &layers)
{
    m_radanLayers = layers;
    update();
}

// 射线法: 点是否在多边形(trace/sample域转为widget域再判断)
bool ImageLabel::polyContains(const QVector<QPointF> &poly, const QPoint &pos) const
{
    if (poly.size() < 3) return false;
    QVector<QPointF> wpts;
    for (const QPointF &p : poly)
        wpts.append(QPointF(traceToWidgetX(qRound(p.x())), sampleToWidgetY(qRound(p.y()))));
    bool inside = false;
    for (int i = 0, j = wpts.size() - 1; i < wpts.size(); j = i++) {
        if (((wpts[i].y() > pos.y()) != (wpts[j].y() > pos.y()))
            && (pos.x() < (wpts[j].x() - wpts[i].x()) * (pos.y() - wpts[i].y())
                         / (wpts[j].y() - wpts[i].y()) + wpts[i].x()))
            inside = !inside;
    }
    return inside;
}

int ImageLabel::editingAnomalyIndex() const
{
    for (int i = 0; i < m_anomalies.size(); ++i)
        if (m_anomalies[i].editing && m_anomalies[i].shape >= 0) return i;
    return -1;
}

bool ImageLabel::hasInterpOverlay() const
{
    for (const HorizonLayer &h : m_radanLayers)
        if (h.visible && !h.points.isEmpty()) return true;
    for (const HorizonLayer &h : m_horizons)
        if (h.visible && !h.points.isEmpty()) return true;
    if (!m_anomalies.isEmpty()) return true;
    if (!m_seeds.isEmpty()) return true;
    return false;
}

// 解译叠加绘制: 层位折线+左缘名称chip → 异常形状+标签 → 追踪种子十字
void ImageLabel::drawInterpOverlay()
{
    QPainter p(this);

    // RADAN 原生层位点(彩色实心小圆点, 只读, 不画连线/chip)
    for (const HorizonLayer &rl : m_radanLayers) {
        if (!rl.visible || rl.points.isEmpty()) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(rl.color);
        const int r = qMax(2, rl.lineWidth / 2);   // 半径 = sizePx/2
        for (const QPointF &pt : rl.points) {
            const int x = traceToWidgetX(qRound(pt.x()));
            const int y = sampleToWidgetY(qRound(pt.y()));
            p.drawEllipse(QPoint(x, y), r, r);
        }
        p.setBrush(Qt::NoBrush);
    }

    // 层位曲线 + 左缘名称 chip
    for (const HorizonLayer &h : m_horizons) {
        if (!h.visible) continue;
        QPen pen(h.color, h.lineWidth, h.dashed ? Qt::DashLine : Qt::SolidLine);
        p.setPen(pen);
        if (h.points.size() == 1) {
            const QPoint c(traceToWidgetX(qRound(h.points[0].x())),
                           sampleToWidgetY(qRound(h.points[0].y())));
            p.drawEllipse(c, 3, 3);
        } else if (h.points.size() >= 2) {
            QPainterPath path;
            path.moveTo(traceToWidgetX(qRound(h.points.first().x())),
                        sampleToWidgetY(qRound(h.points.first().y())));
            for (int i = 1; i < h.points.size(); ++i)
                path.lineTo(traceToWidgetX(qRound(h.points[i].x())),
                            sampleToWidgetY(qRound(h.points[i].y())));
            p.drawPath(path);
        } else {
            continue;   // 无点也画名称chip(便于识别层)
        }
        // 左缘层名 chip: 黑底彩边小标签
        const int chipY = h.points.isEmpty() ? 20
            : qBound(10, sampleToWidgetY(qRound(h.points.first().y())) - 8, height() - 22);
        const QFont f = MatIcon::monoFont(11);
        p.setFont(f);
        const int tw = QFontMetrics(f).horizontalAdvance(h.name) + 10;
        QRect chip(QPoint(2, chipY), QSize(tw, 16));
        p.setPen(QPen(h.color, 1));
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRoundedRect(chip, 2, 2);
        p.setPen(h.color);
        p.drawText(chip, Qt::AlignCenter, h.name);
        p.setBrush(Qt::NoBrush);
    }

    // v1.0.120: 启动/停止流动虚线定时器(有编辑态或多边形绘制时)
    bool needAnim = m_polyDrawing;
    for (const AnomalyMark &am : m_anomalies)
        if (am.editing) { needAnim = true; break; }
    if (needAnim && m_dashTimer && !m_dashTimer->isActive())
        m_dashTimer->start();
    else if (!needAnim && m_dashTimer && m_dashTimer->isActive())
        m_dashTimer->stop();

    // 多边形绘制模式: 流动小圆圈光标 + 已落顶点 + dot line 连接 + 预览线
    if (m_polyDrawing) {
        QPen apen(QColor(0xff, 0xa5, 0x00), 2, Qt::DashLine);
        apen.setDashOffset(m_dashOffset);
        p.setPen(apen);
        // 光标小圆圈
        const int cx = m_polyCursor.x();
        const int cy = m_polyCursor.y();
        p.drawEllipse(QPoint(cx, cy), 6, 6);
        // 已落顶点 + dot line 连接
        if (!m_polyPoints.isEmpty()) {
            // 首顶点大圆(闭合目标)
            const QPointF &fp = m_polyPoints.first();
            const int fx = traceToWidgetX(qRound(fp.x()));
            const int fy = sampleToWidgetY(qRound(fp.y()));
            p.drawEllipse(QPoint(fx, fy), 8, 8);
            // 顶点+连线
            QPainterPath path;
            path.moveTo(fx, fy);
            for (int k = 1; k < m_polyPoints.size(); ++k) {
                const int px = traceToWidgetX(qRound(m_polyPoints[k].x()));
                const int py = sampleToWidgetY(qRound(m_polyPoints[k].y()));
                path.lineTo(px, py);
                p.drawEllipse(QPoint(px, py), 4, 4);
            }
            path.lineTo(cx, cy);   // 预览线到光标
            p.drawPath(path);
        }
    }

    // 异常标注: 平时=实线+透明; 选中=实线+半透明填充; 编辑=虚线+手柄
    for (int i = 0; i < m_anomalies.size(); ++i) {
        const AnomalyMark &a = m_anomalies[i];
        if (a.shape < 0) continue;   // 未选形状
        if (a.editing && a.shape == 2 && a.poly.isEmpty()) continue;   // 多边形空=绘制中
        const bool editing = a.editing;
        const bool selected = (i == m_interpSelectedAnomaly);
        QPen pen(a.color, selected ? 3 : 2, Qt::SolidLine);   // 选中=粗边框
        if (editing) {
            pen.setStyle(Qt::DashLine);
            pen.setDashOffset(m_dashOffset);
        }
        p.setPen(pen);
        // 选中=半透明填充(提高alpha确保可见); 未选中=无填充
        if (selected) {
            QColor fc = a.color;
            fc.setAlpha(100);   // ~39%不透明度 — 明显可见但图像仍穿透
            p.setBrush(fc);
        } else {
            p.setBrush(Qt::NoBrush);
        }
        QString depthLbl;
        if (m_interpMPerSample > 0) {
            const double s0 = (a.shape == 2 && !a.poly.isEmpty())
                                  ? a.poly.first().y() : a.rect.normalized().top();
            depthLbl = QString::fromUtf8(" [深度: %1m]").arg(s0 * m_interpMPerSample, 0, 'f', 2);
        }

        QRect shapeRect;
        if (a.shape == 0) {          // 圆(外接框画椭圆)
            shapeRect = rectFromRectT(a.rect.normalized());
            p.drawEllipse(shapeRect);
        } else if (a.shape == 1) {   // 矩形
            shapeRect = rectFromRectT(a.rect.normalized());
            p.drawRect(shapeRect);
        } else if (a.shape == 2) {   // 闭合多边形
            if (a.poly.size() >= 3) {
                QPainterPath path;
                path.moveTo(traceToWidgetX(qRound(a.poly[0].x())),
                            sampleToWidgetY(qRound(a.poly[0].y())));
                for (int k = 1; k < a.poly.size(); ++k)
                    path.lineTo(traceToWidgetX(qRound(a.poly[k].x())),
                                sampleToWidgetY(qRound(a.poly[k].y())));
                path.closeSubpath();
                p.drawPath(path);
            }
        } else {                      // 文本框
            shapeRect = rectFromRectT(a.rect.normalized());
            p.drawRect(shapeRect);
            QFont tf(a.fontFamily);
            tf.setPixelSize(a.fontSize);
            p.setFont(tf);
            p.setPen(a.color);
            p.drawText(shapeRect.adjusted(3, 1, -3, -1), Qt::AlignTop | Qt::AlignLeft, a.name);
        }

        // 编辑态: 手柄(圆/矩/文本=8角边, 多边形=顶点)
        if (editing) {
            p.setBrush(Qt::white);
            p.setPen(QPen(a.color, 1));
            if (a.shape == 2 && a.poly.size() >= 3) {
                for (const QPointF &pt : a.poly) {
                    const int x = traceToWidgetX(qRound(pt.x()));
                    const int y = sampleToWidgetY(qRound(pt.y()));
                    p.drawRect(QRect(x - 4, y - 4, 8, 8));
                }
            } else if (shapeRect.width() > 0) {
                const QPoint cs[8] = {
                    shapeRect.topLeft(), QPoint(shapeRect.center().x(), shapeRect.top()),
                    shapeRect.topRight(), QPoint(shapeRect.right(), shapeRect.center().y()),
                    shapeRect.bottomRight(), QPoint(shapeRect.center().x(), shapeRect.bottom()),
                    shapeRect.bottomLeft(), QPoint(shapeRect.left(), shapeRect.center().y())};
                for (const QPoint &c : cs)
                    p.drawRect(QRect(c.x() - 4, c.y() - 4, 8, 8));
            }
            p.setBrush(Qt::NoBrush);
        }

        // 标签(名称+深度): 气泡盒置于形状上方; 文本批注自身即文字不再加气泡
        if (a.shape != 3) {
            QRectF geoF = a.rect.normalized();
            if (a.shape == 2 && !a.poly.isEmpty()) {
                geoF = QRectF(a.poly.first(), a.poly.first());
                for (const QPointF &pt : a.poly)
                    geoF = geoF.united(QRectF(pt, pt));
            }
            const QRect r = rectFromRectT(geoF);
            const QString lbl = a.name + depthLbl;
            const QFont lf = MatIcon::monoFont(11);
            p.setFont(lf);
            const int lw = QFontMetrics(lf).horizontalAdvance(lbl) + 10;
            int ly = r.top() - 20;
            if (ly < 1) ly = r.top() + 3;
            QRect box(QPoint(qBound(1, r.left(), qMax(1, width() - lw - 2)), ly),
                      QSize(lw, 17));
            p.setPen(QPen(a.color, 1));
            p.setBrush(QColor(255, 255, 255, 200));   // 白色半透明标签(替代黑色)
            p.drawRoundedRect(box, 2, 2);
            p.setPen(a.color);
            p.drawText(box, Qt::AlignCenter, lbl);
            p.setBrush(Qt::NoBrush);
        }
    }

    // 追踪参考点种子: 层色小十字
    if (!m_seeds.isEmpty()) {
        QPen pen(QColor(0xff, 0xff, 0x00), 2);
        p.setPen(pen);
        for (const QPointF &s : m_seeds) {
            const int x = traceToWidgetX(qRound(s.x()));
            const int y = sampleToWidgetY(qRound(s.y()));
            p.drawLine(x - 5, y, x + 5, y);
            p.drawLine(x, y - 5, x, y + 5);
        }
    }
}

// ---- v1.0.108 数据解译面板与状态 ----

// 默认两层位(用户指定: 暂时只有两个层可供选择)
static HorizonLayer makeDefaultHorizon(int idx)
{
    HorizonLayer h;
    if (idx == 0) {
        h.name = QString::fromUtf8("路基顶面");
        h.color = QColor(0x00, 0xff, 0xff);
        h.dashed = true;
    } else {
        h.name = QString::fromUtf8("基底原土层");
        h.color = QColor(0xff, 0x00, 0xff);
        h.dashed = false;
    }
    h.visible = true;
    h.lineWidth = 2;
    return h;
}

// 采样点→米: 用 headerRange/nsamp 与介电常数换算深度; 简化用 depthRange/drawRows
double MainWindow::interpMPerSample() const
{
    if (!m_currentTab || m_pixelsPerRow <= 0) return 0.0;
    const int skip = m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0;
    const int drawRows = m_pixelsPerRow - skip;
    if (drawRows <= 0) return 0.0;
    return m_depthRange / drawRows;
}

int MainWindow::selectedHorizon() const
{
    if (!m_horizonTree || !m_currentTab) return 0;
    const int idx = m_horizonTree->indexOfTopLevelItem(m_horizonTree->currentItem());
    return (idx >= 0 && idx < m_currentTab->radanLayers.size()) ? idx : 0;
}

// tab 解译数据 → 主图叠加(层位列表=radanLayers: 画彩色圆点+追踪折线)
void MainWindow::syncInterpOverlays()
{
    if (!m_currentTab || !m_currentTab->imageLabel) return;
    m_currentTab->imageLabel->setRadanLayers(m_currentTab->radanLayers);
    m_currentTab->imageLabel->setInterpOverlays(
        m_currentTab->radanLayers, m_currentTab->anomalies, m_currentTab->trackSeeds,
        interpMPerSample(), m_selectedAnomaly);
}

// 右侧 320px 解译与管理面板: 层位列表 + 追踪控制 + 异常标注列表 (按 数据解译-追踪异常.html)
void MainWindow::createInterpPanel()
{
    m_interpPanel = new QWidget(this);
    m_interpPanel->setFixedWidth(320);
    m_interpPanel->setStyleSheet("#gprInterpPanel { background: #f8f9ff; }");

    QHBoxLayout *shell = new QHBoxLayout(m_interpPanel);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    QWidget *leftEdge = new QWidget(m_interpPanel);
    leftEdge->setFixedWidth(1);
    leftEdge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    leftEdge->setStyleSheet("background: #c3c6d6;");
    shell->addWidget(leftEdge);
    QWidget *inner = new QWidget(m_interpPanel);
    QVBoxLayout *outer = new QVBoxLayout(inner);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- 头条 40px: [tune] 解译与管理面板 ---
    QWidget *head = new QWidget(inner);
    head->setFixedHeight(40);
    head->setStyleSheet("background: #eff4ff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    QLabel *hIcon = new QLabel(head);
    if (MatIcon::ready())
        hIcon->setPixmap(MatIcon::pixmap(QStringLiteral("tune"), QColor(0x42, 0x46, 0x54), 18,
                                         0.0, devicePixelRatioF()));
    hl->addWidget(hIcon);
    QLabel *hTitle = new QLabel(QString::fromUtf8("解译与管理面板"), head);
    hTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #121c2a;"
                          " border: none; background: transparent;");
    hl->addWidget(hTitle);
    hl->addStretch(1);
    outer->addWidget(head);

    QScrollArea *scroll = new QScrollArea(inner);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    QWidget *body = new QWidget;
    body->setStyleSheet("background: #f8f9ff;");
    QVBoxLayout *bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(12);

    // ==== Section 1: 层位列表 ====
    QFrame *hzBox = new QFrame(body);
    hzBox->setStyleSheet("QFrame { border: 1px solid #c3c6d6; border-radius: 2px; background: #ffffff; }");
    QVBoxLayout *hzL = new QVBoxLayout(hzBox);
    hzL->setContentsMargins(0, 0, 0, 0);
    hzL->setSpacing(0);
    QWidget *hzHead = new QWidget(hzBox);
    hzHead->setStyleSheet("background: #e6eeff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *hzHL = new QHBoxLayout(hzHead);
    hzHL->setContentsMargins(8, 3, 8, 3);
    QLabel *hzCap = new QLabel(QString::fromUtf8("层位列表"), hzHead);
    hzCap->setStyleSheet("font-size: 11px; font-weight: bold; color: #121c2a; border: none;"
                         " background: transparent;");
    hzHL->addWidget(hzCap);
    hzHL->addStretch(1);
    QToolButton *hzAdd = new QToolButton(hzHead);
    if (MatIcon::ready())
        hzAdd->setIcon(MatIcon::icon(QStringLiteral("add"), QColor(0x00, 0x48, 0xaf)));
    hzAdd->setToolTip(QString::fromUtf8("新增层位"));
    hzAdd->setCursor(Qt::PointingHandCursor);
    hzAdd->setStyleSheet("QToolButton { border: none; border-radius: 2px; }"
                         "QToolButton:hover { background: #dee9fc; }");
    connect(hzAdd, &QToolButton::clicked, this, [this]() { addHorizonLayer(); });
    hzHL->addWidget(hzAdd);
    hzL->addWidget(hzHead);

    m_horizonTree = new QTreeWidget(hzBox);
    m_horizonTree->setHeaderHidden(true);
    m_horizonTree->setRootIsDecorated(false);
    m_horizonTree->setStyleSheet(
        "QTreeWidget { border: none; background: #ffffff; font-size: 12px; }"
        "QTreeWidget::item { padding: 2px 4px; border: none; }");
    hzL->addWidget(m_horizonTree);
    bl->addWidget(hzBox);

    // ==== Section 2: 追踪控制 ====
    QFrame *tkBox = new QFrame(body);
    tkBox->setStyleSheet("QFrame { border: 1px solid #c3c6d6; border-radius: 2px; background: #ffffff; }");
    QVBoxLayout *tkL = new QVBoxLayout(tkBox);
    tkL->setContentsMargins(0, 0, 0, 0);
    tkL->setSpacing(0);
    QWidget *tkHead = new QWidget(tkBox);
    tkHead->setStyleSheet("background: #e6eeff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *tkHL = new QHBoxLayout(tkHead);
    tkHL->setContentsMargins(8, 3, 8, 3);
    QLabel *tkCap = new QLabel(QString::fromUtf8("追踪控制"), tkHead);
    tkCap->setStyleSheet("font-size: 11px; font-weight: bold; color: #121c2a; border: none;"
                         " background: transparent;");
    tkHL->addWidget(tkCap);
    tkL->addWidget(tkHead);

    QWidget *tkBody = new QWidget(tkBox);
    QGridLayout *tkGrid = new QGridLayout(tkBody);
    tkGrid->setContentsMargins(8, 8, 8, 8);
    tkGrid->setSpacing(6);

    // RADAN式: 开始(checkable)=进入追踪模式→图上点击放参考点→算法自动估算两参考点间层点
    m_btnTrackStart = new QPushButton(QString::fromUtf8("开始"), tkBody);
    m_btnTrackStart->setCheckable(true);
    m_btnTrackStart->setCursor(Qt::PointingHandCursor);
    m_btnTrackStart->setStyleSheet(
        "QPushButton { background: #dee9fc; color: #424654; border: 1px solid #c3c6d6;"
        " border-radius: 3px; padding: 6px; font-size: 13px; }"
        "QPushButton:hover { background: #d9e3f6; }"
        "QPushButton:checked { background: #0048af; color: #ffffff; border-color: #0048af; font-weight: bold; }");
    tkGrid->addWidget(m_btnTrackStart, 0, 0);

    QPushButton *btnTrackPause = new QPushButton(QString::fromUtf8("暂停"), tkBody);
    btnTrackPause->setEnabled(false);
    btnTrackPause->setToolTip(QStringLiteral("等客户确认"));
    btnTrackPause->setStyleSheet(
        "QPushButton { background: #dee9fc; color: #424654; border: 1px solid #c3c6d6;"
        " border-radius: 3px; padding: 6px; font-size: 13px; }");
    tkGrid->addWidget(btnTrackPause, 0, 1);

    m_btnTrackStop = new QPushButton(QString::fromUtf8("停止"), tkBody);
    m_btnTrackStop->setCursor(Qt::PointingHandCursor);
    m_btnTrackStop->setStyleSheet(
        "QPushButton { background: #ffffff; color: #ba1a1a; border: 1px solid #ba1a1a;"
        " border-radius: 3px; padding: 6px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(186,26,26,0.08); }");
    tkGrid->addWidget(m_btnTrackStop, 1, 0, 1, 2);
    tkL->addWidget(tkBody);
    bl->addWidget(tkBox);

    // ==== Section 3: 异常标注列表 ====
    QFrame *anBox = new QFrame(body);
    anBox->setStyleSheet("QFrame { border: 1px solid #c3c6d6; border-radius: 2px; background: #ffffff; }");
    QVBoxLayout *anL = new QVBoxLayout(anBox);
    anL->setContentsMargins(0, 0, 0, 0);
    anL->setSpacing(0);
    QWidget *anHead = new QWidget(anBox);
    anHead->setStyleSheet("background: #e6eeff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *anHL = new QHBoxLayout(anHead);
    anHL->setContentsMargins(8, 3, 4, 3);
    QLabel *anCap = new QLabel(QString::fromUtf8("异常标注列表"), anHead);
    anCap->setStyleSheet("font-size: 11px; font-weight: bold; color: #121c2a; border: none;"
                         " background: transparent;");
    anHL->addWidget(anCap);
    anHL->addStretch(1);
    QToolButton *anAdd = new QToolButton(anHead);
    if (MatIcon::ready())
        anAdd->setIcon(MatIcon::icon(QStringLiteral("add"), QColor(0x00, 0x48, 0xaf)));
    anAdd->setToolTip(QString::fromUtf8("新增异常标注"));
    anAdd->setCursor(Qt::PointingHandCursor);
    anAdd->setStyleSheet("QToolButton { border: none; border-radius: 2px; }"
                         "QToolButton:hover { background: #dee9fc; }");
    connect(anAdd, &QToolButton::clicked, this, [this]() { addAnomalyItem(); });
    anHL->addWidget(anAdd);
    anL->addWidget(anHead);

    m_anomalyList = new QListWidget(anBox);
    m_anomalyList->setStyleSheet(
        "QListWidget { border: none; background: #ffffff; font-size: 12px; }"
        "QListWidget::item { padding: 0px; border-bottom: 1px solid #e6eeff; }");
    m_anomalyList->setMinimumHeight(120);
    anL->addWidget(m_anomalyList);

    // v1.0.130: 列表选中 → 更新行背景(不rebuild, 保留双击编辑状态) + 菜单同步 + ImageLabel选中
    connect(m_anomalyList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_currentTab) return;
        m_selectedAnomaly = row;
        // 直接更新行背景色(不调用refreshAnomalyList, 避免销毁正在编辑的QLineEdit)
        for (int i = 0; i < m_anomalyList->count(); ++i) {
            QWidget *w = m_anomalyList->itemWidget(m_anomalyList->item(i));
            if (w)
                w->setStyleSheet(i == row ? "background: #dee9fc;" : "background: #ffffff;");
        }
        syncInterpOverlays();
        if (m_annoGroup && row >= 0 && row < m_currentTab->anomalies.size()) {
            const int sh = m_currentTab->anomalies[row].shape;
            if (sh >= 0) {
                QAbstractButton *btn = m_annoGroup->button(sh);
                if (btn) btn->setChecked(true);
            } else {
                QAbstractButton *btn = m_annoGroup->checkedButton();
                if (btn) btn->setChecked(false);
            }
        }
    });
    bl->addWidget(anBox, 1);

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
    shell->addWidget(inner, 1);

    // 层位列表行选中 → 图上叠加刷新 + 选中浅蓝/未选白色
    connect(m_horizonTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
        for (int i = 0; i < m_horizonTree->topLevelItemCount(); ++i) {
            QWidget *w = m_horizonTree->itemWidget(m_horizonTree->topLevelItem(i), 0);
            if (w) {
                w->setStyleSheet(
                    m_horizonTree->topLevelItem(i) == cur
                        ? "background: #dee9fc;"
                        : "background: #ffffff;");
            }
        }
        syncInterpOverlays();
    });
    // RADAN式追踪: 开始(checkable)=进入追踪模式; 停止=退出+清种子+恢复原始
    connect(m_btnTrackStart, &QPushButton::toggled, this, [this](bool checked) {
        if (checked && !requireOpenFile()) {
            m_btnTrackStart->setChecked(false);
            return;
        }
        if (!checked && m_currentTab)
            clearTrackSeeds();   // 取消开始 = 结束本次追踪
    });
    connect(m_btnTrackStop, &QPushButton::clicked, this, [this]() {
        if (m_btnTrackStart) m_btnTrackStart->setChecked(false);   // 恢复灰
        if (m_currentTab) clearTrackSeeds();
    });
}

// 新增层位: 编号递增或补缺(找第一个空缺 layerNum); 名"第N层"; 颜色=7色映射[N%7]循环
void MainWindow::addHorizonLayer()
{
    if (!m_currentTab) return;
    static const QColor cls[7] = {
        QColor(0xff,0xff,0x00), QColor(0xff,0x00,0x00), QColor(0x00,0xff,0x00),
        QColor(0x00,0x00,0xff), QColor(0xa0,0x52,0x2d), QColor(0x00,0x00,0x00),
        QColor(0xff,0xff,0xff) };

    // 找第一个空缺编号
    int newNum = m_currentTab->radanLayers.size();
    {
        QSet<int> used;
        for (const HorizonLayer &h : m_currentTab->radanLayers)
            used.insert(h.layerNum);
        for (int n = 0; n <= m_currentTab->radanLayers.size(); ++n) {
            if (!used.contains(n)) { newNum = n; break; }
        }
    }

    HorizonLayer h;
    h.layerNum = newNum;
    h.name = QStringLiteral("第%1层").arg(newNum + 1);
    h.color = cls[newNum % 7];
    h.visible = true;
    h.lineWidth = 5;
    m_currentTab->radanLayers.append(h);
    // 排序: 按 layerNum 升序排列(补缺的插到正确位置)
    std::sort(m_currentTab->radanLayers.begin(), m_currentTab->radanLayers.end(),
              [](const HorizonLayer &a, const HorizonLayer &b) { return a.layerNum < b.layerNum; });
    refreshHorizonList();
    syncInterpOverlays();
}

// 层位列表刷新: 数据源=radanLayers(DZX LayerGroup); 眼睛(黑=显/灰=隐)/色块/名称可编辑/粗细滑条1-10/垃圾桶删除
void MainWindow::refreshHorizonList()
{
    if (!m_horizonTree || !m_currentTab) return;
    m_horizonTree->blockSignals(true);
    m_horizonTree->clear();
    for (int i = 0; i < m_currentTab->radanLayers.size(); ++i) {
        HorizonLayer &h = m_currentTab->radanLayers[i];
        QTreeWidgetItem *item = new QTreeWidgetItem(m_horizonTree);
        item->setData(0, Qt::UserRole, i);
        m_horizonTree->addTopLevelItem(item);

        // 几何命中判定: 所有子控件鼠标穿透, 单击=选中该层, 双击=按位置分派(眼睛=显隐/名称=编辑/垃圾桶=删除)
        ListRowWidget *row = new ListRowWidget(m_horizonTree);
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 2, 4, 2);
        rl->setSpacing(6);

        QToolButton *eye = new QToolButton(row);
        eye->setAttribute(Qt::WA_TransparentForMouseEvents);
        if (MatIcon::ready())
            eye->setIcon(MatIcon::icon(h.visible ? QStringLiteral("visibility")
                                                 : QStringLiteral("visibility_off"),
                                       h.visible ? QColor(0x1a, 0x1a, 0x1a)
                                                 : QColor(0xb0, 0xb4, 0xc0)));
        eye->setToolTip(QString::fromUtf8("双击显示/隐藏"));
        rl->addWidget(eye);

        QLabel *swatch = new QLabel(row);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background: %1; border: 1px solid #c3c6d6; border-radius: 2px;")
                                  .arg(h.color.name()));
        rl->addWidget(swatch);

        QLineEdit *nameEd = new QLineEdit(h.name, row);
        nameEd->setStyleSheet("QLineEdit { border: none; background: transparent; font-size: 12px;"
                              " color: #121c2a; padding: 0; }"
                              "QLineEdit:focus { border: 1px solid #0048af; border-radius: 2px; }");
        nameEd->setFixedWidth(110);
        relockLineEditEdit(nameEd);          // 只读+穿透(单击可选中该行)
        connect(nameEd, &QLineEdit::editingFinished, this, [this, i, nameEd]() {
            if (!m_currentTab || i >= m_currentTab->radanLayers.size()) return;
            const QString t = nameEd->text().trimmed();
            if (!t.isEmpty()) {
                m_currentTab->radanLayers[i].name = t;
                syncInterpOverlays();
            }
            relockLineEditEdit(nameEd);      // 恢复只读+穿透
        });
        rl->addWidget(nameEd, 1);

        // 滑条保持可交互(拖动改粗细); 按下时同步选中该行
        QSlider *wSlider = new QSlider(Qt::Horizontal, row);
        wSlider->setRange(1, 10);
        wSlider->setValue(h.lineWidth);
        wSlider->setFixedWidth(60);
        wSlider->setToolTip(QString::fromUtf8("圆点大小: %1px").arg(h.lineWidth));
        wSlider->setStyleSheet(
            "QSlider::groove:horizontal { height: 3px; background: #c3c6d6; border-radius: 1px; }"
            "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -4px 0;"
            " border-radius: 5px; background: #0048af; }");
        connect(wSlider, &QSlider::sliderPressed, this, [this, item]() {
            m_horizonTree->setCurrentItem(item);   // 操作滑条=选中该层
        });
        connect(wSlider, &QSlider::valueChanged, this, [this, i](int v) {
            if (!m_currentTab || i >= m_currentTab->radanLayers.size()) return;
            m_currentTab->radanLayers[i].lineWidth = v;
            syncInterpOverlays();
        });
        rl->addWidget(wSlider);

        // 垃圾桶: 双击删除该层
        QToolButton *del = new QToolButton(row);
        del->setAttribute(Qt::WA_TransparentForMouseEvents);
        if (MatIcon::ready())
            del->setIcon(MatIcon::icon(QStringLiteral("delete"), QColor(0xba, 0x1a, 0x1a)));
        del->setToolTip(QString::fromUtf8("双击删除该层"));
        rl->addWidget(del);

        // 单击任意位置=选中该层(触发 currentItemChanged → 全链路同步)
        row->clicked = [this, item]() {
            m_horizonTree->setCurrentItem(item);
        };
        // 双击: 按子控件几何分派(名称/空白=改名, 眼睛=显隐, 垃圾桶=删除)
        row->doubleClicked = [this, i, eye, nameEd, del](const QPoint &pos) {
            if (!m_currentTab || i >= m_currentTab->radanLayers.size()) return;
            if (del->geometry().contains(pos)) {
                m_currentTab->radanLayers.remove(i);
                refreshHorizonList();
                syncInterpOverlays();
            } else if (eye->geometry().contains(pos)) {
                m_currentTab->radanLayers[i].visible = !m_currentTab->radanLayers[i].visible;
                refreshHorizonList();
                syncInterpOverlays();
            } else {
                unlockLineEditEdit(nameEd);
            }
        };

        m_horizonTree->setItemWidget(item, 0, row);
    }
    m_horizonTree->blockSignals(false);
    if (m_horizonTree->topLevelItemCount() > 0)
        m_horizonTree->setCurrentItem(m_horizonTree->topLevelItem(selectedHorizon()));
}

// 新增异常标注: 默认圆形, 自动选中; 名"异常标注N"(N=补缺编号)
void MainWindow::addAnomalyItem()
{
    if (!m_currentTab) return;
    QSet<int> used;
    for (const AnomalyMark &a : m_currentTab->anomalies) {
        if (a.name.startsWith(QStringLiteral("异常标注"))) {
            const int n = a.name.mid(QString::fromUtf8("异常标注").length()).toInt();
            if (n > 0) used.insert(n);
        }
    }
    int newN = 1;
    while (used.contains(newN)) ++newN;

    AnomalyMark a;
    a.shape = -1;   // 无默认形状 — 用户点形状按钮后才赋形
    a.name = QStringLiteral("异常标注%1").arg(newN);
    a.color = QColor(0xff, 0xff, 0x00);
    // 补缺编号插入到顺序位置(第一个编号>newN的项之前), 保持列表按编号有序
    int insertIdx = m_currentTab->anomalies.size();
    for (int k = 0; k < m_currentTab->anomalies.size(); ++k) {
        const AnomalyMark &cur = m_currentTab->anomalies[k];
        if (cur.name.startsWith(QStringLiteral("异常标注"))) {
            const int m = cur.name.mid(QString::fromUtf8("异常标注").length()).toInt();
            if (m > newN) { insertIdx = k; break; }
        }
    }
    m_currentTab->anomalies.insert(insertIdx, a);
    m_selectedAnomaly = insertIdx;
    refreshAnomalyList();
    syncInterpOverlays();
}

// 选中异常设形状+进入编辑态(虚线+手柄)
// 编辑状态切换形状: 保留当前位置参数, 几何自动转换(rect↔poly)
void MainWindow::anomalySetShape(int idx, int shapeId)
{
    if (!m_currentTab || idx < 0 || idx >= m_currentTab->anomalies.size()) return;
    AnomalyMark &a = m_currentTab->anomalies[idx];
    static const QColor shapeColors[4] = {
        QColor(0xff, 0xff, 0x00), QColor(0xff, 0x00, 0x00),
        QColor(0xff, 0xa5, 0x00), QColor(0x00, 0xd4, 0xff) };

    // 同一时间只允许一个编辑态: 其他异常全部确认(变实线)
    for (int k = 0; k < m_currentTab->anomalies.size(); ++k)
        if (k != idx) m_currentTab->anomalies[k].editing = false;

    // 保存现有几何
    const QRectF oldRect = a.rect;
    const QVector<QPointF> oldPoly = a.poly;

    a.shape = shapeId;
    a.color = shapeColors[shapeId];
    a.editing = true;

    if (shapeId == 2) {
        // → 多边形: 之前有多边形顶点则恢复原形状; 否则启动绘制模式
        if (oldPoly.size() >= 3) {
            a.poly = oldPoly;   // 恢复原多边形(往返切换不丢失)
            a.rect = QRectF();
            if (m_currentTab->imageLabel)
                m_currentTab->imageLabel->stopPolyDrawing();
        } else {
            a.poly.clear();
            a.rect = QRectF();
            if (m_currentTab->imageLabel)
                m_currentTab->imageLabel->startPolyDrawing();
        }
    } else {
        // → 圆/矩/文本: 用当前位置; 不清空poly(切回多边形恢复)
        if (m_currentTab->imageLabel)
            m_currentTab->imageLabel->stopPolyDrawing();
        if (!oldRect.isNull() && oldRect.width() >= 5 && oldRect.height() >= 5) {
            a.rect = oldRect;   // 已有有效rect: 保留
        } else if (oldPoly.size() >= 3) {
            // 从多边形转来: 用多边形中心+外接尺寸(确保新形状可见)
            double cx = 0, cy = 0;
            QRectF br = QRectF(oldPoly.first(), oldPoly.first());
            for (const QPointF &p : oldPoly) {
                cx += p.x(); cy += p.y();
                br = br.united(QRectF(p, p));
            }
            cx /= oldPoly.size(); cy /= oldPoly.size();
            const double w = qMax(br.width(), 30.0);    // 最小宽度30道
            const double h = qMax(br.height(), 20.0);   // 最小高度20采样
            a.rect = QRectF(cx - w / 2, cy - h / 2, w, h);
        } else {
            TabData *tab = m_currentTab;
            const int centerT = tab->traceCount / 2;
            const int centerS = (tab->nsamp - (tab->zeroApplied ? tab->zeroSkipRows : 0)) / 2;
            a.rect = QRectF(centerT - 30, centerS - 20, 60, 40);
        }
    }
    syncInterpOverlays();
    refreshAnomalyList();
}

// 异常确认(虚线→实线); 未闭合多边形(<3点)丢弃
void MainWindow::anomalyConfirmShape(int idx)
{
    if (!m_currentTab || idx < 0 || idx >= m_currentTab->anomalies.size()) return;
    AnomalyMark &a = m_currentTab->anomalies[idx];
    if (m_currentTab->imageLabel)
        m_currentTab->imageLabel->stopPolyDrawing();
    if (a.shape == 2 && a.poly.size() < 3) {
        // 未闭合多边形: 不保留
        a.shape = -1;
        a.poly.clear();
        a.rect = QRectF();
    } else {
        a.editing = false;
    }
    syncInterpOverlays();
    refreshAnomalyList();
}

// 异常列表刷新: 每行 = [形状图标] [名称QLineEdit] [色点] [备注QLineEdit] [垃圾桶]
void MainWindow::refreshAnomalyList()
{
    if (!m_anomalyList || !m_currentTab) return;
    m_anomalyList->blockSignals(true);
    m_anomalyList->clear();
    static const char *shapeGlyph[4] = { "radio_button_unchecked", "check_box_outline_blank",
                                         "pentagon", "title" };
    static const char *shapeEmpty = "help";   // 未选形状
    for (int i = 0; i < m_currentTab->anomalies.size(); ++i) {
        AnomalyMark &a = m_currentTab->anomalies[i];
        // 几何命中判定: 所有子控件鼠标穿透, 单击=选中该异常, 双击=按位置分派(名称/备注/空白=编辑, 垃圾桶=删除)
        ListRowWidget *row = new ListRowWidget(m_anomalyList);
        row->setStyleSheet(i == m_selectedAnomaly
                               ? "background: #dee9fc;"
                               : "background: #ffffff;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 3, 2, 3);
        rl->setSpacing(4);

        // 形状图标
        QLabel *icon = new QLabel;
        if (MatIcon::ready())
            icon->setPixmap(MatIcon::pixmap(
                QString::fromLatin1(a.shape >= 0 && a.shape <= 3 ? shapeGlyph[a.shape] : shapeEmpty),
                QColor(0x00, 0x48, 0xaf), 14, 0.0, devicePixelRatioF()));
        icon->setFixedSize(16, 16);
        rl->addWidget(icon);

        // 名称(双击编辑)
        QLineEdit *nameEd = new QLineEdit(a.name, row);
        nameEd->setStyleSheet("QLineEdit { border: none; background: transparent; font-size: 12px;"
                              " font-weight: bold; color: #121c2a; padding: 0; }"
                              "QLineEdit:focus { border: 1px solid #0048af; border-radius: 2px; }");
        nameEd->setFixedWidth(90);
        relockLineEditEdit(nameEd);          // 只读+穿透(单击可选中该行)
        connect(nameEd, &QLineEdit::editingFinished, this, [this, i, nameEd]() {
            if (!m_currentTab || i >= m_currentTab->anomalies.size()) return;
            const QString t = nameEd->text().trimmed();
            if (!t.isEmpty()) {
                m_currentTab->anomalies[i].name = t;
                syncInterpOverlays();
            }
            relockLineEditEdit(nameEd);      // 恢复只读+穿透
        });
        rl->addWidget(nameEd);

        // 色点
        QLabel *dot = new QLabel;
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString("background: %1; border: 1px solid #c3c6d6; border-radius: 5px;")
                               .arg(a.color.name()));
        rl->addWidget(dot);

        // 备注(双击编辑)
        QLineEdit *remEd = new QLineEdit(a.remark, row);
        remEd->setPlaceholderText(QString::fromUtf8("备注..."));
        remEd->setStyleSheet("QLineEdit { border: none; background: transparent; font-size: 11px;"
                             " color: #424654; padding: 0; }"
                             "QLineEdit:focus { border: 1px solid #0048af; border-radius: 2px; }");
        relockLineEditEdit(remEd);
        connect(remEd, &QLineEdit::editingFinished, this, [this, i, remEd]() {
            if (!m_currentTab || i >= m_currentTab->anomalies.size()) return;
            m_currentTab->anomalies[i].remark = remEd->text().trimmed();
            relockLineEditEdit(remEd);
        });
        rl->addWidget(remEd, 1);

        // 垃圾桶(双击删除)
        QToolButton *del = new QToolButton(row);
        del->setAttribute(Qt::WA_TransparentForMouseEvents);
        if (MatIcon::ready())
            del->setIcon(MatIcon::icon(QStringLiteral("delete"), QColor(0xba, 0x1a, 0x1a)));
        del->setToolTip(QString::fromUtf8("双击删除"));
        rl->addWidget(del);

        // 单击任意位置=选中该异常(触发 currentRowChanged → 背景/菜单/ImageLabel全链路同步)
        row->clicked = [this, i]() {
            if (m_anomalyList->currentRow() != i)
                m_anomalyList->setCurrentRow(i);
        };
        // 双击: 按子控件几何分派(备注区=编辑备注, 垃圾桶=删除, 其余=编辑名称)
        row->doubleClicked = [this, i, nameEd, remEd, del](const QPoint &pos) {
            if (!m_currentTab || i >= m_currentTab->anomalies.size()) return;
            if (del->geometry().contains(pos)) {
                m_currentTab->anomalies.remove(i);
                m_selectedAnomaly = -1;
                refreshAnomalyList();
                syncInterpOverlays();
            } else if (remEd->geometry().contains(pos)) {
                unlockLineEditEdit(remEd);
            } else {
                unlockLineEditEdit(nameEd);
            }
        };

        QListWidgetItem *it = new QListWidgetItem;
        it->setSizeHint(QSize(0, 32));
        m_anomalyList->addItem(it);
        m_anomalyList->setItemWidget(it, row);
    }
    // 选中当前项(视觉高亮) — 保持在 blockSignals 内, 防止 currentRowChanged→refreshAnomalyList 无限递归崩溃
    if (m_selectedAnomaly >= 0 && m_selectedAnomaly < m_anomalyList->count())
        m_anomalyList->setCurrentRow(m_selectedAnomaly);
    m_anomalyList->blockSignals(false);
}

// 数据解译状态总闸: 进出数据解译页/切tab; 面板显隐+按钮复位+列表刷新
void MainWindow::syncInterpUiState()
{
    const bool on = m_currentTab != nullptr
                    && ribbonTab && ribbonTab->currentIndex() == 3;   // 3 = 数据解译页
    if (m_interpPanel) {
        m_interpPanel->setVisible(on);
        if (on) {
            if (m_currentTab && m_currentTab->radanLayers.isEmpty()) {
                // 无DZX层位数据: 建7个空层(第1~7层, 固定色)
                static const QColor cls[7] = {
                    QColor(0xff,0xff,0x00), QColor(0xff,0x00,0x00), QColor(0x00,0xff,0x00),
                    QColor(0x00,0x00,0xff), QColor(0xa0,0x52,0x2d), QColor(0x00,0x00,0x00),
                    QColor(0xff,0xff,0xff) };
                for (int i = 0; i < 7; ++i) {
                    HorizonLayer h;
                    h.name = QStringLiteral("第%1层").arg(i + 1);
                    h.color = cls[i];
                    h.lineWidth = 5;
                    h.layerNum = i;
                    m_currentTab->radanLayers.append(h);
                }
            }
            refreshHorizonList();
            refreshAnomalyList();
            syncInterpOverlays();
        }
    }
    if (!on) {
        // 离开数据解译页: 模式按钮复位 + 清叠加
        QAbstractButton *btns[] = { m_btnAutoTrack, m_btnManualTrack, m_btnAnoCircle,
                                    m_btnAnoRect, m_btnAnoPoly, m_btnAnoText };
        for (QAbstractButton *b : btns)
            if (b) b->setChecked(false);
        // 离开时闭合未完成的多边形
        for (int k = 0; m_currentTab && k < m_currentTab->anomalies.size(); ++k) {
            if (m_currentTab->anomalies[k].name == QStringLiteral("__poly_pending__")) {
                if (m_currentTab->anomalies[k].poly.size() >= 3)
                    m_currentTab->anomalies[k].name = QString::fromUtf8("异常%1")
                        .arg(m_currentTab->anomalies.size(), 2, 10, QChar('0'));
                else
                    m_currentTab->anomalies.remove(k--);
            }
        }
        if (m_currentTab && m_currentTab->imageLabel)
            m_currentTab->imageLabel->setInterpOverlays(
                QVector<HorizonLayer>(), QVector<AnomalyMark>(), QVector<QPointF>(), 0.0, -1);
        if (m_currentTab) m_currentTab->trackSeeds.clear();
    }
}

// 停止: 清参考点种子 + 开始按钮恢复灰
void MainWindow::clearTrackSeeds()
{
    if (!m_currentTab) return;
    m_currentTab->trackSeeds.clear();
    syncInterpOverlays();
}

// RADAN式峰值跟随: 两两参考点之间追踪 + 首末参考点向外延伸; ≥2个种子时每次放置新点后调用
void MainWindow::autoTrackHorizon(int layerIdx)
{
    if (!m_currentTab || layerIdx < 0 || layerIdx >= m_currentTab->radanLayers.size()) return;
    if (m_rawData.isEmpty() || m_pixelsPerRow <= 0) return;
    const int skip = m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0;
    const int drawRows = m_pixelsPerRow - skip;
    if (drawRows <= 0) return;

    HorizonLayer &h = m_currentTab->radanLayers[layerIdx];
    QVector<QPointF> seeds = m_currentTab->trackSeeds;   // 已按 trace 排序
    if (seeds.size() < 2) return;

    const int W = 10;   // 搜索窗口(±采样)
    QVector<QPointF> tracked;

    auto follow = [&](int fromT, int toT, int startS) {
        // 从 fromT 向 toT 逐步追踪(支持正向/反向)
        const int step = (toT >= fromT) ? 1 : -1;
        int prevS = qBound(0, startS, drawRows - 1);
        tracked.append(QPointF(fromT, prevS));
        for (int t = fromT + step; (step > 0 ? t <= toT : t >= toT); t += step) {
            int bestS = prevS, bestV = -1;
            for (int ds = -W; ds <= W; ++ds) {
                const int s = qBound(0, prevS + ds, drawRows - 1);
                const qint32 v = qAbs(getPixelValue(t, s));
                if (v > bestV) { bestV = v; bestS = s; }
            }
            tracked.append(QPointF(t, bestS));
            prevS = bestS;
        }
    };

    // 1. 首参考点向左延伸到 trace 0
    follow(qRound(seeds.first().x()), 0, qRound(seeds.first().y()));
    // 2. 相邻参考点对之间追踪
    for (int k = 0; k + 1 < seeds.size(); ++k)
        follow(qRound(seeds[k].x()), qRound(seeds[k + 1].x()), qRound(seeds[k].y()));
    // 3. 末参考点向右延伸到 traceCount-1
    follow(qRound(seeds.last().x()), m_traceCount - 1, qRound(seeds.last().y()));

    // 合并: 追踪点 + 现有点 按 trace 排序去重(同道取后写)
    h.points += tracked;
    std::sort(h.points.begin(), h.points.end(),
              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
    QVector<QPointF> merged;
    for (const QPointF &p : h.points) {
        if (!merged.isEmpty() && qRound(merged.last().x()) == qRound(p.x()))
            merged.last() = p;
        else
            merged.append(p);
    }
    h.points = merged;
    syncInterpOverlays();   // 显示追踪结果(种子保留供继续加点)
    m_thumbKey.clear();
    refreshImage();
}

// 天线型号(DZT 头 offset 0x62/98)→ 中心频率(MHz)对照表(GSSI)。仅内部使用,不显示。
// 后续 PROCESS/深度换算等可据此判断天线频段;型号需持续补充。
static int antennaFreqMHz(const QString &type)
{
    QString t = type.trimmed();
    if (t == "3207") return 100;
    if (t == "3101") return 900;
    return 0;   // 未知型号
}

void MainWindow::showFileHeader()
{
    // 主页"文件头"按钮 toggle: 开=解析并显示右侧栏; 关/无文件=收起
    if (!m_btnHeaderToggle || !m_btnHeaderToggle->isChecked()) {
        setHeaderPanelVisible(false);
        return;
    }
    if (!requireOpenFile()) {
        setHeaderPanelVisible(false);
        return;
    }
    refreshHeaderPanel();
    setHeaderPanelVisible(true);
}

// 读当前文件 DZT 头(1024字节) → 右栏 8 字段所需子集
bool MainWindow::readDztHeaderInfo(DztHeaderInfo &out)
{
    if (!m_currentTab) return false;
    QFile file(m_currentTab->filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray hdr = file.read(1024);
    file.close();
    if (hdr.size() < 128) return false;

    auto rdShort = [&hdr](int off) -> qint16 {
        return static_cast<qint16>(
            (static_cast<quint8>(hdr[off+1]) << 8) |
            static_cast<quint8>(hdr[off]));
    };
    auto rdFloat = [&hdr](int off) -> float {
        float val;
        memcpy(&val, hdr.constData() + off, 4);
        return val;
    };
    // decode tagRFDate (4 bytes at offset) → "Mon,DD YYYY,HH:MM:SS" 或 "00:00:00"(值为0时)
    auto rdDate = [&hdr](int off) -> QString {
        const char *months[] = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        quint32 val = static_cast<quint8>(hdr[off])
                    | (static_cast<quint8>(hdr[off+1]) << 8)
                    | (static_cast<quint8>(hdr[off+2]) << 16)
                    | (static_cast<quint8>(hdr[off+3]) << 24);
        if (val == 0) return QString("00:00:00");  // 未设置
        int sec2  = val & 0x1F;
        int min   = (val >> 5) & 0x3F;
        int hour  = (val >> 11) & 0x1F;
        int day   = (val >> 16) & 0x1F;
        int month = (val >> 21) & 0xF;
        int year  = ((val >> 25) & 0x7F) + 1980;
        QString monStr = (month >= 1 && month <= 12) ? months[month] : "???";
        return QString("%1,%2 %3,%4:%5:%6")
            .arg(monStr)
            .arg(day, 2, 10, QChar('0'))
            .arg(year)
            .arg(hour, 2, 10, QChar('0'))
            .arg(min, 2, 10, QChar('0'))
            .arg(sec2 * 2, 2, 10, QChar('0'));
    };

    out.fileName = QString::fromLatin1(hdr.mid(114, 12)).trimmed();
    out.fileName = out.fileName.left(out.fileName.indexOf(QLatin1Char('\0')));
    out.createDate = rdDate(32);
    out.nsamp = rdShort(4);     // 采样点数 (offset 4)
    out.range = rdFloat(26);    // 记录长度 ns (offset 26)
    out.epsr = rdFloat(54);     // 介电常数 (offset 54)
    out.spm = rdFloat(14);      // 扫描/米 (offset 14), 道间距=1/spm
    out.antName = QString::fromLatin1(hdr.mid(98, 14)).trimmed();
    out.antName = out.antName.left(out.antName.indexOf(QLatin1Char('\0')));
    return true;
}

// v1.0.87 右侧 350px 文件头属性栏(严格按 主页-文件头.png: 标题栏40px + 8行两列表格)
void MainWindow::createHeaderPanel()
{
    m_headerPanel = new QWidget(this);
    m_headerPanel->setFixedWidth(350);
    m_headerPanel->setStyleSheet("background: #f8f9ff;");

    QVBoxLayout *outer = new QVBoxLayout(m_headerPanel);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- 标题栏 40px: [info] 文件头属性 + ✕ ---
    QWidget *head = new QWidget(m_headerPanel);
    head->setFixedHeight(40);
    head->setStyleSheet("background: #eff4ff; border-bottom: 1px solid #c3c6d6;");
    QHBoxLayout *hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 4, 0);
    hl->setSpacing(8);
    QLabel *hIcon = new QLabel;
    hIcon->setStyleSheet("border: none; background: transparent;");
    if (MatIcon::ready())
        hIcon->setPixmap(MatIcon::pixmap(QStringLiteral("info"), QColor(0x12, 0x1c, 0x2a), 16, 0.0, devicePixelRatioF()));
    hl->addWidget(hIcon);
    QLabel *hTitle = new QLabel(QString::fromUtf8("文件头属性"));
    hTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #121c2a;"
                          " letter-spacing: 1px; border: none; background: transparent;");
    hl->addWidget(hTitle);
    hl->addStretch(1);
    QToolButton *closeBtn = new QToolButton;
    if (MatIcon::ready())
        closeBtn->setIcon(MatIcon::icon(QStringLiteral("close"), QColor(0x73, 0x77, 0x85), QColor(),
                                        QColor(0x12, 0x1c, 0x2a), 16));
    closeBtn->setIconSize(QSize(16, 16));
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QToolButton { border: none; border-radius: 2px; background: transparent; }"
        "QToolButton:hover { background: #dee9fc; }");
    connect(closeBtn, &QToolButton::clicked, this, [this]() { setHeaderPanelVisible(false); });
    hl->addWidget(closeBtn);
    outer->addWidget(head);

    // --- 8 行两列表格(键列白底12px灰 / 值列浅底等宽12px, 行间细分隔线) ---
    QWidget *body = new QWidget;
    body->setStyleSheet("background: #f8f9ff;");
    QVBoxLayout *bl = new QVBoxLayout(body);
    bl->setContentsMargins(8, 8, 8, 8);
    QFrame *table = new QFrame;
    table->setStyleSheet("QFrame { border: 1px solid #c3c6d6; border-radius: 2px; background: #ffffff; }");
    QGridLayout *grid = new QGridLayout(table);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    const QString keys[8] = {
        QString::fromUtf8("文件名"), QString::fromUtf8("天线频率"), QString::fromUtf8("采样点数"),
        QString::fromUtf8("总道数"), QString::fromUtf8("时窗"), QString::fromUtf8("介电常数"),
        QString::fromUtf8("采集日期"), QString::fromUtf8("道间距")};
    for (int i = 0; i < 8; ++i) {
        const QString rowBorder = (i < 7) ? QStringLiteral(" border-bottom: 1px solid #c3c6d6;") : QString();
        QLabel *k = new QLabel(keys[i]);
        k->setStyleSheet("background: #ffffff; color: #424654; font-size: 12px;"
                         " border: none; padding: 6px 8px;" + rowBorder);
        QLabel *v = new QLabel("-");
        v->setStyleSheet("background: #f8f9ff; color: #121c2a; font-size: 12px;"
                         " border: none; border-left: 1px solid #c3c6d6; padding: 6px 8px;" + rowBorder);
        if (MatIcon::ready())
            v->setFont(MatIcon::monoFont(12));   // JetBrains Mono 等宽数值
        v->setWordWrap(true);
        k->setMinimumHeight(28);
        v->setMinimumHeight(28);
        grid->addWidget(k, i, 0);
        grid->addWidget(v, i, 1);
        m_headerValueLabels.append(v);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    bl->addWidget(table);
    bl->addStretch(1);

    QScrollArea *scroll = new QScrollArea(m_headerPanel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    m_headerPanel->hide();
}

// 开关右栏 + 同步主页"文件头"按钮 + 重定位悬浮的文件切换三角按钮
void MainWindow::setHeaderPanelVisible(bool visible)
{
    if (!m_headerPanel) return;
    m_headerPanel->setVisible(visible);
    if (m_btnHeaderToggle) {
        QSignalBlocker blocker(m_btnHeaderToggle);
        m_btnHeaderToggle->setChecked(visible);
    }
    // 右栏显隐改变 docSplitter 几何 → 延迟重定位(照抄 showWelcome 的 singleShot 模式)
    QTimer::singleShot(0, this, [this]() { repositionSwitchButton(); });
}

// 解析当前 DZT 头填充 8 个值单元格
void MainWindow::refreshHeaderPanel()
{
    DztHeaderInfo info;
    const bool ok = readDztHeaderInfo(info);
    auto setV = [this](int i, const QString &s) {
        if (i >= 0 && i < m_headerValueLabels.size()) m_headerValueLabels[i]->setText(s);
    };
    if (!ok) {
        for (int i = 0; i < 8; ++i) setV(i, "-");
        return;
    }
    const int mhz = antennaFreqMHz(info.antName);
    setV(0, QFileInfo(m_currentTab->filePath).fileName());
    setV(1, mhz > 0 ? QString("%1MHz").arg(mhz)
                    : (info.antName.isEmpty() ? QStringLiteral("-") : info.antName));
    setV(2, QString::number(info.nsamp));
    setV(3, QString::number(m_traceCount));
    setV(4, QString::number(info.range, 'f', 0) + "ns");
    setV(5, QString::number(info.epsr, 'f', 1));
    setV(6, info.createDate);
    setV(7, info.spm > 0 ? QString::number(1.0 / info.spm, 'f', 4) + "m" : "-");
}

// --- File operations ---

void MainWindow::openDztFile(const QString &filePath)
{
    if (filePath.isEmpty()) return;

    QImage image = loadDZTFile(filePath);
    if (image.isNull()) {
        QMessageBox::warning(this, "Error", "Failed to load DZT file:\n" + filePath);
        return;
    }

    coordinateLabel->setText(QString::fromUtf8("道号: -  深度: -"));   // v1.0.87 状态栏常显
    coordinateLabel->setToolTip(QString());

    if (m_tabs.isEmpty()) hideWelcome();
    QTimer::singleShot(0, this, [this]() { repositionSwitchButton(); });  // 有文件→显示三角按钮

    if (!m_tabGroups.contains(m_activeTabGroup))
        m_activeTabGroup = m_docTabWidget;

    // 尝试读取同名 DZX,有则自动处理后在新 tab 显示
    QFileInfo fi(filePath);
    QString dzxPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".DZX";
    if (QFile::exists(dzxPath)) {
        // 日期检查:DZX 和 DZT 修改时间必须相同,否则不处理
        QDateTime dztTime = QFileInfo(filePath).lastModified();
        QDateTime dzxTime = QFileInfo(dzxPath).lastModified();
        if (dztTime != dzxTime) {
            createTab(filePath, image);
            return;
        }
        // 解析 DZX 处理参数
        QList<DzxProcess> processes;
        if (!parseDzxProcesses(dzxPath, processes)) {
            // DZX 解析失败:静默,直接正常打开
            createTab(filePath, image);
            return;
        }
        // 先创建原始文件 tab
        createTab(filePath, image);
        // 备份原始数据,应用处理,保存 Proc 文件,在新 tab 打开处理后结果
        QByteArray origData = m_rawData;
        applyDzxProcessing(dzxPath);
        saveProcessedWithDzx(filePath, processes);
        // restore m_rawData for the original tab (saveProcessedWithDzx may have reloaded)
        m_rawData = origData;
        return;
    }

    createTab(filePath, image);
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open DZT File", "",
        "DZT Files (*.dzt);;All Files (*)");

    if (fileName.isEmpty()) return;
    openDztFile(fileName);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept the drag only if it carries at least one .dzt file
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            QString path = url.toLocalFile();
            if (!path.isEmpty() &&
                QFileInfo(path).suffix().compare("dzt", Qt::CaseInsensitive) == 0) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    bool opened = false;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        if (QFileInfo(path).suffix().compare("dzt", Qt::CaseInsensitive) != 0) continue;
        openDztFile(path);   // supports dropping multiple files -> one tab each
        opened = true;
    }

    if (opened) event->acceptProposedAction();
    else event->ignore();
}

void MainWindow::onImageClicked(const QPoint &pos)
{
    updateCoordinateLabel(pos.x(), pos.y());

    // Update digital filter dialog spectrum if open
    if (m_filterDlg && m_filterDlg->isVisible()) {
        updateFilterSpectrum(pos.x());
    }

    // Update one-click process reference chart if open
    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }
}

void MainWindow::updateCoordinateLabel(int x, int y)
{
    if (m_rawData.isEmpty()) return;
    if (x < 0 || y < 0) return;

    // 堆积图模式下 x 是图像像素(0..numWiggles*32),需换算为道号(步长2,每槽32列)
    int traceNo = m_wiggleMode ? (x / 32) * 2 : x;

    qint32 pixelValue = getPixelValue(traceNo, y);
    // 双程走时/深度:按当前左/右标尺范围随采样点 y 线性换算
    double twt   = m_pixelsPerRow ? y * m_timeRange  / m_pixelsPerRow : 0.0;   // 双程走时 ns
    double depth = m_pixelsPerRow ? y * m_depthRange / m_pixelsPerRow : 0.0;   // 深度 m(按右RANGE换算)
    int amp = pixelValue / 256;                                                // 振幅÷256(整数,保留符号)

    // v1.0.87 状态栏按设计稿只显示 道号+深度,其余 3 项放 tooltip
    coordinateLabel->setText(QString::fromUtf8("道号: %1  深度: %2 m").arg(traceNo).arg(depth, 0, 'f', 2));
    coordinateLabel->setToolTip(QString::fromUtf8("采样点数: %1 | 双程走时: %2 ns | 振幅: %3")
                                    .arg(y)
                                    .arg(twt, 0, 'f', 2)
                                    .arg(amp));

    updateChart(traceNo);
}

void MainWindow::updateChart(int xValue)
{
    if (m_rawData.isEmpty() || !chartSeries) return;

    m_lastChartX = xValue;
    chartSeries->clear();

    const int maxPoints = m_pixelsPerRow;   // 波形Y轴始终 0..nsamp-1(不变)
    qint32 minVal = 0, maxVal = 0;
    float yscale = chartView ? chartView->yScale() : 1.0f;

    bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
    bool isZeroMode = (yscale != 1.0f);

    // 波形固定按采样点 0..511 显示,不再用 sigPos 裁剪/切时间模式
    float sigPos = m_currentTab ? m_currentTab->signalPosition : 0.0f;
    (void)sigPos;
    int sigPad = 0;

    // Zero-point manual overrides
    double zeroOff = 0.0;
    int zeroPad = 0;
    if (isZeroMode) {
        if (m_zeroRangePctSpin)
            zeroOff = -m_zeroRangePctSpin->value() * 0.2;
        if (m_zeroOffsetSpin && m_zeroOffsetSpin->value() > 0)
            zeroPad = qRound(maxPoints * m_zeroOffsetSpin->value() / 20.0);
    }

    int totalOff = sigPad + zeroPad;   // sigPad=0,仅零点预览的 zeroPad 生效

    // Use original data when gain is already applied, so chart preview is always vs original
    const QByteArray &chartRawData = (m_currentTab && m_currentTab->gainApplied)
                                     ? m_currentTab->originalRawData : m_rawData;

    for (int i = 0; i < maxPoints; ++i) {
        qint32 displayValue = 0;
        int srcY = totalOff + i;
        if (srcY < maxPoints) {
            int dataIdx = (xValue * m_pixelsPerRow + srcY) * 4;
            qint32 pixelValue = 0;
            if (dataIdx + 4 <= chartRawData.size()) {
                pixelValue = static_cast<qint32>(
                    (static_cast<quint8>(chartRawData[dataIdx + 3]) << 24) |
                    (static_cast<quint8>(chartRawData[dataIdx + 2]) << 16) |
                    (static_cast<quint8>(chartRawData[dataIdx + 1]) << 8) |
                    static_cast<quint8>(chartRawData[dataIdx]));
            }
            float rawGain = (chartView) ? chartView->interpolatedGain(srcY) : m_gain;
            float rowGainLinear = isLinear ? rawGain : std::pow(10.0f, rawGain / 20.0f);
            float dv = rowGainLinear * static_cast<float>(pixelValue);
            if (dv > 8388607.0f) dv = 8388607.0f;
            if (dv < -8388608.0f) dv = -8388608.0f;
            displayValue = static_cast<qint32>(dv);
        }
        qreal yCoord;
        if (isZeroMode)
            yCoord = zeroOff + static_cast<qreal>(i) * (20.0 / 511.0);
        else
            yCoord = static_cast<qreal>(i);     // 普通模式:采样点 0..511
        chartSeries->append(static_cast<qreal>(displayValue), yCoord);
        if (i == 0 || displayValue < minVal) minVal = displayValue;
        if (i == 0 || displayValue > maxVal) maxVal = displayValue;
    }

    QValueAxis *axisX = qobject_cast<QValueAxis*>(chartView->chart()->axisX(chartSeries));
    axisX->setRange(-256*256*256/2, 256*256*256/2);

    // Set Y axis range
    QValueAxis *axisY = qobject_cast<QValueAxis*>(chartView->chart()->axisY(chartSeries));
    if (axisY) {
        if (isZeroMode) {
            axisY->setRange(zeroOff, 20.0 + zeroOff);
            axisY->setLabelFormat("%.1f");
        } else {
            axisY->setRange(0, m_pixelsPerRow - 1);     // 普通模式:Y 轴 0..nsamp-1(不变)
            axisY->setLabelFormat("%d");
        }
    }
}

qint32 MainWindow::getPixelValue(int x, int y)
{
    if (m_rawData.isEmpty()) return 0;

    const int bytesPerPixel = 4;
    int dataIdx = (x * m_pixelsPerRow + y) * bytesPerPixel;

    if (dataIdx + 4 > m_rawData.size()) return 0;

    qint32 pixelValue = static_cast<qint32>(
        (static_cast<quint8>(m_rawData[dataIdx + 3]) << 24) |
        (static_cast<quint8>(m_rawData[dataIdx + 2]) << 16) |
        (static_cast<quint8>(m_rawData[dataIdx + 1]) << 8) |
        (static_cast<quint8>(m_rawData[dataIdx]))
    );

    return pixelValue;
}

// 堆积图(wiggle)渲染:每个堆积占 32 列,依次显示道号 0/2/4/6...(步长 2)
// 黑色波形线画在白色背景上,幅度按全局最大绝对值归一化到 ±16 像素(槽宽 32 的一半)
QImage MainWindow::renderWiggleImage(int traceCount, int drawRows, int skipRows)
{
    const int slotW = 32;                              // 每个堆积占 32 列
    const int halfW  = slotW / 2;                      // 中心线两侧各 16 像素
    const int step   = 2;                              // 道号步长:0,2,4,...
    int numWiggles = (traceCount + step - 1) / step;   // 需要画的堆积条数
    if (numWiggles < 1) numWiggles = 1;

    int imgW = numWiggles * slotW;
    int imgH = qMax(1, drawRows);
    QImage image(imgW, imgH, QImage::Format_RGB32);
    image.fill(Qt::white);

    // 显示增益:与普通模式一致(已应用增益则不再叠加)
    float displayGain = (m_currentTab && m_currentTab->gainApplied) ? 1.0f : m_gain;

    // 第一遍:统计所有堆积所有样本的全局最大绝对值(用于归一化,保留各道相对幅度)
    double globalMax = 0.0;
    for (int k = 0; k < numWiggles; ++k) {
        int t = k * step;
        if (t >= traceCount) break;
        for (int y = 0; y < drawRows; ++y) {
            double dv = displayGain * static_cast<double>(getPixelValue(t, y + skipRows));
            if (dv > 8388607.0) dv = 8388607.0;
            if (dv < -8388608.0) dv = -8388608.0;
            double a = std::fabs(dv);
            if (a > globalMax) globalMax = a;
        }
    }
    double scale = (globalMax > 0.0) ? (halfW / globalMax) : 0.0;

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(Qt::black, 1);
    p.setPen(pen);

    for (int k = 0; k < numWiggles; ++k) {
        int t = k * step;
        if (t >= traceCount) break;
        int centerX = k * slotW + halfW;

        // 构造波形折线点
        QVector<QPointF> pts;
        pts.reserve(drawRows);
        for (int y = 0; y < drawRows; ++y) {
            double dv = displayGain * static_cast<double>(getPixelValue(t, y + skipRows));
            if (dv > 8388607.0) dv = 8388607.0;
            if (dv < -8388608.0) dv = -8388608.0;
            double defl = dv * scale;
            if (defl > halfW) defl = halfW;
            if (defl < -halfW) defl = -halfW;
            pts.append(QPointF(centerX + defl, y));
        }
        if (pts.size() >= 2)
            p.drawPolyline(pts.constData(), pts.size());
    }
    p.end();

    return image.convertToFormat(QImage::Format_RGB32);
}

QImage MainWindow::loadDZTFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "open file failed.");
        return QImage();
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    const qint64 dataOffset = 0x20000;
    const int bytesPerPixel = 4;

    m_dataOffset = dataOffset;

    // Read signal position (rhf_position) at offset 22
    file.seek(22);
    m_signalPos = 0.0f;
    file.read(reinterpret_cast<char*>(&m_signalPos), 4);
    qDebug() << "loadDZTFile: signalPos=" << m_signalPos;

    // Preserve full header + parse record length / dielectric / samples-per-scan
    file.seek(0);
    m_header = file.read(1024);
    m_headerRange = 20.0f;
    m_epsr = 1.0f;
    m_nsamp = 512;
    if (m_header.size() >= 56) {
        memcpy(&m_headerRange, m_header.constData() + 26, 4);   // rhf_range (ns) 记录长度
        memcpy(&m_epsr, m_header.constData() + 54, 4);          // epsr 介电常数
        m_nsamp = (static_cast<quint8>(m_header[5]) << 8) | static_cast<quint8>(m_header[4]);  // nsamp (int16 LE)
    }
    // 采样点数取自文件头(256/512 等),不再写死 512 —— 决定 B-SCAN 高度、数据索引、波形 Y 轴
    const int pixelsPerRow = (m_nsamp > 0) ? m_nsamp : 512;
    m_pixelsPerRow = pixelsPerRow;

    if (!file.seek(dataOffset)) {
        QMessageBox::warning(this, "Error", "open file failed.");
        return QImage();
    }

    m_rawData = file.readAll();
    int dataSize = m_rawData.size();
    int totalPixels = dataSize / bytesPerPixel;
    int rows = totalPixels / pixelsPerRow;
    int sigPad = 0;   // 始终显示全部 512 采样点(不再按 sigPos 裁剪)
    int drawRows = pixelsPerRow - sigPad;
    qDebug() << "dataSize = " << dataSize << "rows = " << rows << "sigPad = " << sigPad << "drawRows = " << drawRows;

    QImage image(rows, drawRows, QImage::Format_RGB32);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < drawRows; ++x) {
            int srcX = x + sigPad;
            int dataIdx = (y * pixelsPerRow + srcX) * bytesPerPixel;
            if (dataIdx + 4 > dataSize) continue;

            qint32 pixelValue;
            if (srcX == 0 || srcX == 1) {
                pixelValue = 0;
            } else {
                pixelValue = static_cast<qint32>(
                    (static_cast<quint8>(m_rawData[dataIdx + 3]) << 24) |
                    (static_cast<quint8>(m_rawData[dataIdx + 2]) << 16) |
                    (static_cast<quint8>(m_rawData[dataIdx + 1]) << 8) |
                    (static_cast<quint8>(m_rawData[dataIdx]))
                );
            }

            int lutIdx = pixelValue / (256 * 256) + 128;
            if (lutIdx < 0) lutIdx = 0;
            if (lutIdx > 255) lutIdx = 255;
            image.setPixel(y, x, m_lut[lutIdx]);
        }
    }
    return image;
}

// ==================== DZX 自动处理 ====================

// uuencode 解码(与 DZX_format.md 一致)
static QByteArray uuDecode(const QString &text)
{
    QString s = text;
    for (auto &p : {QPair<QString,QString>("&amp;","&"), QPair<QString,QString>("&lt;","<"),
                    QPair<QString,QString>("&gt;",">"), QPair<QString,QString>("&quot;","\""),
                    QPair<QString,QString>("&apos;","'")})
        s.replace(p.first, p.second);
    s.remove('\r'); s.remove('\n'); s.remove('\t'); s.remove(' ');
    QByteArray out;
    int pos = 0;
    int n = 0;
    while (pos < s.size()) {
        n = ((s.at(pos).toLatin1() - 32) & 0x3F);
        pos++;
        if (n == 0) break;
        int nchars = ((n + 2) / 3) * 4;
        for (int i = 0; i < nchars; i += 4) {
            quint8 v[4];
            for (int j = 0; j < 4; ++j)
                v[j] = (pos + i + j < s.size())
                       ? ((s.at(pos + i + j).toLatin1() - 32) & 0x3F) : 0;
            quint32 b = (v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3];
            out += char((b >> 16) & 0xFF);
            out += char((b >> 8) & 0xFF);
            out += char(b & 0xFF);
        }
        pos += nchars;
    }
    return out.left(n);  // 只返回有效字节
}

bool MainWindow::parseDzxProcesses(const QString &dzxPath, QList<DzxProcess> &processes)
{
    QFile f(dzxPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QXmlStreamReader xml(&f);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("BinaryData")) {
            DzxProcess proc;
            QString rawText = xml.readElementText();
            proc.rawData = uuDecode(rawText);
            if (proc.rawData.size() >= 9)
                proc.typeId = static_cast<quint8>(proc.rawData.at(8));
            processes.append(proc);
        }
    }
    return !processes.isEmpty();
}

void MainWindow::applyDzxProcessing(const QString &dzxPath)
{
    QList<DzxProcess> processes;
    if (!parseDzxProcesses(dzxPath, processes)) return;

    int samplesPerTrace = m_pixelsPerRow;  // 512
    int totalSamples = m_rawData.size() / 4;
    int traceCount = totalSamples / samplesPerTrace;
    char *data = m_rawData.data();

    // ---- 诊断输出:DZX 文件名 + 所有处理步骤及其参数(只显示在诊断终端,用于与 RADAN 对比) ----
    auto typeName = [](int id) -> const char* {
        switch (id) {
        case 99: return "DC去除(振幅偏移去除)";
        case 77: return "时间零点";
        case 59: return "增益";
        case 4:  return "IIR垂直";
        case 63: return "FIR垂直低通";
        case 64: return "FIR垂直高通";
        case 65: return "FIR垂直低通(三角)";
        case 66: return "FIR垂直高通(三角)";
        case 13: return "IIR水平(叠加)";
        case 14: return "IIR水平(背景去除)";
        case 67: return "FIR水平平滑(叠加)";
        case 68: return "FIR水平背景去除";
        case 69: return "FIR水平平滑(三角)";
        case 70: return "FIR水平背景去除(三角)";
        case 95: return "专用背景去除";
        case 39: return "信号底跟踪";
        default: return "未知类型";
        }
    };
    auto isHandled = [](int id) -> const char* {
        // 所有 DZX typeId 暂不执行;时间零点处理用 rhf_position(offset 22)
        return "[暂不执行-跳过]";
    };
    auto hexDump = [](const QByteArray &b) {
        QString s;
        for (int i = 0; i < b.size(); ++i) {
            s += QString::number(static_cast<quint8>(b.at(i)), 16).rightJustified(2, '0') + " ";
            if ((i + 1) % 16 == 0) s += "\n      ";
        }
        return s.trimmed();
    };
    diagPrint("========== DZX 处理诊断 ==========");
    diagPrint(QString("DZX 文件: %1").arg(dzxPath));
    diagPrint(QString("DZT 道数: %1   每道采样: %2   记录长度: %3 ns   signalPos: %4")
              .arg(traceCount).arg(samplesPerTrace)
              .arg(QString::number(m_headerRange, 'f', 3))
              .arg(QString::number(m_signalPos, 'f', 3)));
    diagPrint(QString("找到 %1 个处理步骤:").arg(processes.size()));
    for (int i = 0; i < processes.size(); ++i) {
        const auto &p = processes[i];
        const QByteArray &blob = p.rawData;
        diagPrint(QString("[%1] typeId=%2 (%3)   blob=%4 字节   %5")
            .arg(i + 1).arg(p.typeId).arg(typeName(p.typeId)).arg(blob.size()).arg(isHandled(p.typeId)));
        if (p.typeId == 99) {
            diagPrint(QString::fromUtf8("     -> DC去除(振幅偏移去除) 无参数"));
        } else if (p.typeId == 77 && blob.size() >= 0x0E) {
            float v = 0.0f; memcpy(&v, blob.constData() + 0x0A, 4);
            diagPrint(QString::fromUtf8("     -> 振幅偏移去除 修整量 = %1 ns").arg(QString::number(v, 'f', 3)));
        } else if (p.typeId == 59 && blob.size() >= 0x0F) {
            int npts = static_cast<quint8>(blob.at(9));
            QString gains;
            for (int k = 0; k < npts && 0x0B + k * 4 + 4 <= blob.size(); ++k) {
                float g = 0.0f; memcpy(&g, blob.constData() + 0x0B + k * 4, 4);
                gains += QString::number(g, 'f', 1) + " ";
            }
            diagPrint(QString::fromUtf8("     -> 增益 npts=%1  gainDb=[ %2 ]").arg(npts).arg(gains.trimmed()));
        } else if (p.typeId == 4 && blob.size() >= 0x24) {
            int lp = static_cast<quint8>(blob.at(0x20)) | (static_cast<quint8>(blob.at(0x21)) << 8);
            int hp = static_cast<quint8>(blob.at(0x22)) | (static_cast<quint8>(blob.at(0x23)) << 8);
            diagPrint(QString::fromUtf8("     -> IIR垂直 LP=%1  HP=%2 MHz (u16)").arg(lp).arg(hp));
        } else if (p.typeId == 64 && blob.size() >= 0x22) {
            int hp = static_cast<quint8>(blob.at(0x20)) | (static_cast<quint8>(blob.at(0x21)) << 8);
            diagPrint(QString::fromUtf8("     -> FIR垂直HP = %1 MHz").arg(hp));
        } else if (p.typeId == 63 && blob.size() >= 0x20) {
            int lp = static_cast<quint8>(blob.at(0x1E)) | (static_cast<quint8>(blob.at(0x1F)) << 8);
            diagPrint(QString::fromUtf8("     -> FIR垂直LP = %1 MHz").arg(lp));
        } else if ((p.typeId == 14 || p.typeId == 13) && blob.size() >= 0x0E) {
            float v = 0.0f; memcpy(&v, blob.constData() + 0x0A, 4);
            diagPrint(QString::fromUtf8("     -> IIR水平 = %1 扫描数").arg(QString::number(v, 'f', 1)));
        } else if (p.typeId == 67 && blob.size() >= 0x0D) {
            float v = 0.0f; memcpy(&v, blob.constData() + 0x09, 4);
            diagPrint(QString::fromUtf8("     -> FIR水平平滑 长度 = %1 扫描数").arg(QString::number(v, 'f', 1)));
        }
        diagPrint(QString("     hex: %1").arg(hexDump(blob)));
    }
    diagPrint("==================================");

    // (其他处理暂不做 — 只做时间零点)
    for (const auto &proc : processes) {
        (void)proc;  // 跳过所有 DZX typeId 处理
    }

    // === 只做时间零点处理(其他操作暂不做) ===
    // 时间零点偏移 = rhf_position(offset 22),不是 DZX 的 typeId 99/77
    m_dzxTimeZeroSkip = 0;
    m_dzxOriginalSignalPos = m_signalPos;
    diagPrint(QString::fromUtf8("--- 执行时间零点处理 ---"));
    diagPrint(QString::fromUtf8("  信号位置 rhf_position = %1 ns").arg(QString::number(m_signalPos, 'f', 3)));
    if (m_signalPos < 0.0f && m_headerRange > 0.0f) {
        int skip = qRound(samplesPerTrace * (-m_signalPos) / m_headerRange);
        diagPrint(QString::fromUtf8("  跳过采样点 = %1 × %2 / %3 = %4")
                  .arg(samplesPerTrace).arg(QString::number(-m_signalPos,'f',3))
                  .arg(QString::number(m_headerRange,'f',3)).arg(skip));
        if (skip > 0 && skip < samplesPerTrace) {
            for (int t = 0; t < traceCount; ++t) {
                int base = t * samplesPerTrace * 4;
                memmove(data + base, data + base + skip * 4,
                        (samplesPerTrace - skip) * 4);
                memset(data + base + (samplesPerTrace - skip) * 4, 0, skip * 4);
            }
            m_dzxTimeZeroSkip = skip;
            diagPrint(QString::fromUtf8("  完成:每道上移 %1 采样,底部补零,显示 %2 行")
                      .arg(skip).arg(samplesPerTrace - skip));
        }
    } else {
        diagPrint(QString::fromUtf8("  信号位置 >= 0,无需处理"));
    }
    diagPrint(QString::fromUtf8("--- 其他处理(DC去除/增益/滤波等)暂不执行 ---"));
}

void MainWindow::saveProcessedWithDzx(const QString &origDztPath, const QList<DzxProcess> &processes)
{
    QFileInfo fi(origDztPath);
    // 如果已在 Proc 目录,直接用当前目录;否则在上级建 Proc
    QString parentDir = fi.absolutePath();
    QString procDir = (QDir(parentDir).dirName().compare("Proc", Qt::CaseInsensitive) == 0)
                      ? parentDir : parentDir + "/Proc";
    QDir().mkpath(procDir);

    // 找下一个可用编号 _P_##.DZT
    // 如果原文件已有 _P_## 或  P_##(RADAN格式),编号递增;否则从 01 开始
    QString baseName = fi.completeBaseName();
    int startN = 1;
    QRegularExpression reOur("_P_(\\d+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reRadan(" P_(\\d+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch mOur = reOur.match(baseName);
    QRegularExpressionMatch mRadan = reRadan.match(baseName);
    if (mOur.hasMatch()) {
        startN = mOur.captured(1).toInt() + 1;
        baseName = baseName.left(mOur.capturedStart());
    } else if (mRadan.hasMatch()) {
        startN = mRadan.captured(1).toInt() + 1;
        baseName = baseName.left(mRadan.capturedStart());
    }
    QString outDzt, outDzx;
    int N = startN;
    do {
        outDzt = procDir + QString("/%1_P_%2.DZT").arg(baseName).arg(N, 2, 10, QChar('0'));
        outDzx = procDir + QString("/%1_P_%2.DZX").arg(baseName).arg(N, 2, 10, QChar('0'));
        N++;
    } while (QFile::exists(outDzt));

    // 写 DZT: 原始头(0x20000) + 处理后 m_rawData
    QFile srcFile(origDztPath);
    QFile outFile(outDzt);
    if (!srcFile.open(QIODevice::ReadOnly) || !outFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to save processed file.");
        return;
    }
    QByteArray header = srcFile.read(m_dataOffset);
    patchDztHeaderForTab(header);   // v1.0.98: 裁剪后头一致性补丁
    srcFile.close();

    // 修改头:信号位置归零(时间零点已处理)
    if (header.size() >= 26) {
        float zero = 0.0f;
        memcpy(header.data() + 22, &zero, 4);  // rhf_position = 0
    }
    // 在处理记录区(procOff=128 处)增加一条:时间零点偏移
    {
        qint16 procOff = (header.size() >= 50) ?
            static_cast<qint16>(static_cast<quint8>(header[48]) | (static_cast<quint8>(header[49]) << 8)) : 128;
        qint16 procSize = (header.size() >= 52) ?
            static_cast<qint16>(static_cast<quint8>(header[50]) | (static_cast<quint8>(header[51]) << 8)) : 0;
        if (procOff <= 0) procOff = 128;
        int writeOff = procOff + procSize;
        // 记录格式: typeId(0x4d=77) + sub(0x00=时间零点) + float(偏移量ns) = 6字节
        QByteArray rec;
        rec.append(static_cast<char>(0x4d));  // typeId
        rec.append(static_cast<char>(0x00));  // sub = 时间零点
        float val = m_dzxOriginalSignalPos;
        rec.append(reinterpret_cast<const char*>(&val), 4);
        if (writeOff + rec.size() <= header.size()) {
            memcpy(header.data() + writeOff, rec.constData(), rec.size());
            qint16 newSize = static_cast<qint16>(procSize + rec.size());
            header[50] = static_cast<char>(newSize & 0xFF);
            header[51] = static_cast<char>((newSize >> 8) & 0xFF);
        }
    }

    // 写入编辑时间(rhb_mdt, offset 36)
    {
        QDateTime now = QDateTime::currentDateTime();
        QDate d = now.date();
        QTime t = now.time();
        quint32 mdt = 0;
        mdt |= (t.second() / 2) & 0x1F;
        mdt |= (t.minute() & 0x3F) << 5;
        mdt |= (t.hour() & 0x1F) << 11;
        mdt |= (d.day() & 0x1F) << 16;
        mdt |= (d.month() & 0xF) << 21;
        mdt |= ((d.year() - 1980) & 0x7F) << 25;
        if (header.size() >= 40) {
            header[36] = static_cast<char>(mdt & 0xFF);
            header[37] = static_cast<char>((mdt >> 8) & 0xFF);
            header[38] = static_cast<char>((mdt >> 16) & 0xFF);
            header[39] = static_cast<char>((mdt >> 24) & 0xFF);
        }
    }

    outFile.write(header);
    outFile.write(m_rawData);
    outFile.close();

    // 写 DZX: Proc 格式(无 Macro,简化版)
    int traceCount = m_rawData.size() / 4 / m_pixelsPerRow;
    double dielectric = m_epsr;
    double unitsPerScan = (m_headerRange > 0 && dielectric > 0)
        ? 0.299792458 * m_headerRange / (2.0 * std::sqrt(dielectric) * traceCount) : 0.01;

    QString dzxXml = QString::fromUtf8(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<DZX xmlns=\"www.geophysical.com/DZX/1.02\">\n"
        "  <GlobalProperties>\n"
        "    <verticalUnit>m</verticalUnit>\n"
        "    <horizontalUnit>m</horizontalUnit>\n"
        "    <dielectric>%1</dielectric>\n"
        "    <readOnly>0</readOnly>\n"
        "    <unitsPerMark>0.0000000</unitsPerMark>\n"
        "    <unitsPerScan>%2</unitsPerScan>\n"
        "  </GlobalProperties>\n"
        "  <File>\n"
        "    <scanRange>0,%3</scanRange>\n"
        "    <name>%4</name>\n"
        "    <Profile>\n"
        "      <scanRange>0,%3</scanRange>\n"
        "      <WayPt>\n"
        "        <scan>0</scan>\n"
        "        <distance>0.0000000</distance>\n"
        "        <localCoords>0.0000000,0.0000000,0.0000000</localCoords>\n"
        "      </WayPt>\n"
        "      <WayPt>\n"
        "        <scan>%3</scan>\n"
        "        <localCoords>%5,0.0000000,0.0000000</localCoords>\n"
        "      </WayPt>\n"
        "    </Profile>\n"
        "  </File>\n"
        "  <DataCollection>\n"
        "    <system>SIR4K</system>\n"
        "    <softwareVersion>1.4.21</softwareVersion>\n"
        "  </DataCollection>\n"
        "</DZX>\n")
        .arg(dielectric, 0, 'f', 7)
        .arg(unitsPerScan, 0, 'f', 7)
        .arg(traceCount)
        .arg(QFileInfo(outDzt).fileName())
        .arg(unitsPerScan * traceCount, 0, 'f', 7);

    QFile dzxFile(outDzx);
    if (dzxFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        dzxFile.write(dzxXml.toUtf8());
        dzxFile.close();
    }

    // 在新 tab 打开处理后的 DZT
    QImage image = loadDZTFile(outDzt);
    if (!image.isNull()) {
        createTab(outDzt, image);
        // 设置处理后 tab 的时间零点跳过信息(影响显示范围)
        if (m_currentTab && m_dzxTimeZeroSkip > 0) {
            m_currentTab->zeroApplied = true;
            m_currentTab->zeroSkipRows = m_dzxTimeZeroSkip;
        }
    }
}

// 重置增益面板到默认值:整体/分段增益归 0 dB(线性模式归 1.0),
// chart 手柄与 m_gain 同步复位。每次打开面板时调用,避免再次打开仍显示上次调节的数值。
void MainWindow::resetGainPanel()
{
    bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
    double defVal = isLinear ? 1.0 : 0.0;
    for (int i = 0; i < m_gainSpinBoxes.size(); ++i) {
        if (m_gainSpinBoxes[i]) {
            m_gainSpinBoxes[i]->blockSignals(true);
            m_gainSpinBoxes[i]->setValue(defVal);
            m_gainSpinBoxes[i]->blockSignals(false);
        }
    }
    m_gain = 1.0f;
    if (m_currentTab) m_currentTab->gain = 1.0f;
    if (chartView) {
        int cnt = chartView->lineCount();
        int actual = (cnt <= 1) ? 2 : cnt;
        float hDef = isLinear ? 1.0f : 0.0f;
        for (int j = 0; j < actual; ++j)
            chartView->setHandleX(j, hDef);
        float range = isLinear ? 10.0f : 6.0f;
        chartView->setGainRange(isLinear ? 0.0f : -range, range);
    }
}

void MainWindow::applyGain()
{
    if (!requireOpenFile()) return;

    if (m_currentTab->gainApplied) {
        // 撤销：恢复原始数据，重置增益手柄到0 dB
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_btnApply->setText(QString::fromUtf8("应用"));
    } else {
        // 应用：备份原始数据，将增益乘入rawData
        m_currentTab->originalRawData = m_rawData;

        bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
        const int gN = m_pixelsPerRow;
        QVector<float> gainTable(gN);
        for (int x = 0; x < gN; ++x) {
            float rawGain = (chartView) ? chartView->interpolatedGain(x) : m_gain;
            gainTable[x] = isLinear ? rawGain : std::pow(10.0f, rawGain / 20.0f);
        }

        int totalPixels = m_rawData.size() / 4;
        char *data = m_rawData.data();
        for (int i = 0; i < totalPixels; ++i) {
            int idx = i * 4;
            qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                         (static_cast<quint8>(data[idx+2]) << 16) |
                         (static_cast<quint8>(data[idx+1]) << 8) |
                         (static_cast<quint8>(data[idx]));
            float result = gainTable[i % gN] * static_cast<float>(val);
            if (result > 8388607.0f) result = 8388607.0f;
            if (result < -8388608.0f) result = -8388608.0f;
            val = static_cast<qint32>(result);
            data[idx]   = val & 0xFF;
            data[idx+1] = (val >> 8) & 0xFF;
            data[idx+2] = (val >> 16) & 0xFF;
            data[idx+3] = (val >> 24) & 0xFF;
        }
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = true;
        m_btnApply->setText("撤销");
    }

    refreshImage();
}

void MainWindow::saveProcessedFile()
{
    if (!requireOpenFile()) return;

    // 确保增益已应用（仅旧的增益系统；一键处理已自己处理数据）
    if (!m_currentTab->gainApplied && !m_oneClickApplied && !m_movingAvgApplied && !m_traceEqualApplied && !m_mathApplied && !m_deconvApplied && !m_hilbertApplied && !m_kirchhoffApplied && !m_filterApplied) {
        // 先应用增益
        m_currentTab->originalRawData = m_rawData;
        bool isLinear2 = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
        const int gN = m_pixelsPerRow;
        QVector<float> gainTable(gN);
        for (int x = 0; x < gN; ++x) {
            float rawGain = (chartView) ? chartView->interpolatedGain(x) : m_gain;
            gainTable[x] = isLinear2 ? rawGain : std::pow(10.0f, rawGain / 20.0f);
        }
        int totalPixels = m_rawData.size() / 4;
        char *data = m_rawData.data();
        for (int i = 0; i < totalPixels; ++i) {
            int idx = i * 4;
            qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                         (static_cast<quint8>(data[idx+2]) << 16) |
                         (static_cast<quint8>(data[idx+1]) << 8) |
                         (static_cast<quint8>(data[idx]));
            float result = gainTable[i % gN] * static_cast<float>(val);
            if (result > 8388607.0f) result = 8388607.0f;
            if (result < -8388608.0f) result = -8388608.0f;
            val = static_cast<qint32>(result);
            data[idx]   = val & 0xFF;
            data[idx+1] = (val >> 8) & 0xFF;
            data[idx+2] = (val >> 16) & 0xFF;
            data[idx+3] = (val >> 24) & 0xFF;
        }
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = true;
        m_btnApply->setText("撤销");
    }

    // 创建 Proc 目录(如果已在 Proc 目录则直接用当前目录)
    QFileInfo fi(m_currentTab->filePath);
    QString parentDir = fi.absolutePath();
    QString procDir = (QDir(parentDir).dirName().compare("Proc", Qt::CaseInsensitive) == 0)
                      ? parentDir : parentDir + "/Proc";
    QDir().mkpath(procDir);

    // 找到可用的文件名:如果原文件已有 _P_## 或  P_##(RADAN),编号递增;否则从 01
    // 重名时继续递增
    QString baseName = fi.completeBaseName();
    int startN = 1;
    QRegularExpression reOur("_P_(\\d+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reRadan(" P_(\\d+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch mOur = reOur.match(baseName);
    QRegularExpressionMatch mRadan = reRadan.match(baseName);
    if (mOur.hasMatch()) {
        startN = mOur.captured(1).toInt() + 1;
        baseName = baseName.left(mOur.capturedStart());
    } else if (mRadan.hasMatch()) {
        startN = mRadan.captured(1).toInt() + 1;
        baseName = baseName.left(mRadan.capturedStart());
    }
    int N = startN;
    QString outPath;
    do {
        outPath = procDir + QString("/%1_P_%2.DZT").arg(baseName).arg(N++, 2, 10, QChar('0'));
    } while (QFile::exists(outPath));

    // 写文件：0x20000 头部 + 处理后的数据
    QFile srcFile(m_currentTab->filePath);
    QFile outFile(outPath);
    if (!srcFile.open(QIODevice::ReadOnly) || !outFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to save processed file.");
        return;
    }
    QByteArray header = srcFile.read(m_currentTab->dataOffset);
    patchDztHeaderForTab(header);   // v1.0.98: 裁剪后头一致性补丁
    outFile.write(header);
    srcFile.close();
    outFile.write(m_rawData);
    outFile.close();

    // 恢复原文件 tab 到初始状态并在打开新 tab 之前刷新显示，
    // 否则 createTab 切走 m_currentTab 后 origTab 的图像仍停留在处理后的状态。
    TabData *origTab = m_currentTab;
    QByteArray origData = origTab->originalRawData;
    origTab->rawData = origData;
    origTab->gainApplied = false;
    origTab->zeroApplied = false;
    m_rawData = origData;
    m_btnApply->setText(QString::fromUtf8("应用"));
    refreshImage();
    updateChart(m_lastChartX);

    // 打开新文件作为新 tab（成功后 m_currentTab 切换到新 tab）
    QImage image = loadDZTFile(outPath);
    if (!image.isNull()) {
        createTab(outPath, image);
    }

    m_oneClickApplied = false;
    if (m_oneClickBtnApply)
        m_oneClickBtnApply->setText("应用");
}

// 颜色变换表名称
QString MainWindow::m_colorTransformName(int idx)
{
    return QString::fromUtf8("变换 %1").arg(idx);
}

// 颜色变换 LUT:20种灰度映射,从 specs/颜色变换表.png 精确提取(全灰度,1字节/级)
// ��ɫ�任 LUT:20�ֻҶ�ӳ��,�� specs/��ɫ�任��.png ��ȷ��ȡ(ȫ�Ҷ�,1�ֽ�/��)
static const unsigned char s_cxLUTData[20][256] = {
    { // 1
        0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  4,
        4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,  7,  7,  8,  8,
        8,  8,  9,  9,  9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
       13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18,
       18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26,
       26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 32, 32, 33, 34, 35, 36,
       37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 51, 53,
       55, 56, 58, 60, 62, 64, 67, 70, 73, 76, 80, 85, 90, 96,104,112,
      128,144,152,160,165,170,176,179,182,185,188,192,193,195,197,198,
      200,202,203,205,207,207,208,209,210,211,212,213,214,215,215,216,
      217,218,219,220,221,222,223,223,224,224,225,225,226,226,227,227,
      228,228,229,229,230,230,231,231,232,232,233,233,234,234,235,235,
      236,236,237,237,238,239,239,239,239,240,240,240,240,241,241,241,
      241,242,242,242,243,243,243,243,244,244,244,244,245,245,245,246,
      246,246,246,247,247,247,247,248,248,248,249,249,249,249,250,250,
      250,250,251,251,251,252,252,252,252,253,253,253,253,254,254,254,
    },
    { // 2
        0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  8,
        8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16,
       17, 17, 18, 19, 19, 20, 20, 21, 22, 22, 23, 24, 24, 25, 25, 26,
       27, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 34, 35, 36, 37,
       37, 38, 39, 40, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 48,
       49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63,
       64, 65, 66, 67, 68, 69, 70, 72, 73, 74, 75, 76, 77, 78, 80, 81,
       83, 84, 86, 88, 89, 91, 92, 94, 96, 98,101,104,106,109,112,120,
      128,136,144,146,149,152,154,157,160,161,163,164,166,168,169,171,
      172,174,176,177,178,179,180,181,182,184,185,186,187,188,189,190,
      192,192,193,194,195,196,197,197,198,199,200,201,202,202,203,204,
      205,206,207,207,208,209,209,210,211,212,212,213,214,215,215,216,
      217,217,218,219,220,220,221,222,223,223,224,224,225,226,226,227,
      227,228,229,229,230,231,231,232,232,233,234,234,235,235,236,237,
      237,238,239,239,240,240,241,241,242,242,243,243,244,244,245,245,
      246,247,247,248,248,249,249,250,250,251,251,252,252,253,253,254,
    },
    { // 3
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
       16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
       32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
       48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
       64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
       80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
       96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
      112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
      128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
      144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
      160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
      176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
      192,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,
      207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,
      223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,
      239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,
    },
    { // 4
        0,  1,  3,  5,  7,  8, 10, 12, 14, 16, 17, 19, 21, 23, 24, 26,
       28, 30, 32, 33, 35, 37, 39, 40, 42, 44, 46, 48, 49, 50, 52, 53,
       55, 56, 58, 59, 61, 62, 64, 65, 66, 68, 69, 70, 72, 73, 74, 76,
       77, 78, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
       94, 96, 96, 97, 98, 99,100,101,102,103,104,104,105,106,107,108,
      109,110,111,112,112,112,113,113,113,114,114,114,115,115,115,116,
      116,116,117,117,118,118,118,119,119,119,120,120,120,121,121,121,
      122,122,123,123,123,124,124,124,125,125,125,126,126,126,127,127,
      128,128,128,129,129,129,130,130,130,131,131,131,132,132,132,133,
      133,134,134,134,135,135,135,136,136,136,137,137,137,138,138,139,
      139,139,140,140,140,141,141,141,142,142,142,143,143,144,144,145,
      146,147,148,149,150,151,152,152,153,154,155,156,157,158,159,160,
      161,162,163,164,165,166,167,168,169,170,171,172,173,174,176,177,
      178,180,181,182,184,185,186,188,189,190,192,193,194,196,197,198,
      200,201,202,204,205,207,208,210,212,214,215,217,219,221,223,224,
      226,228,230,231,233,235,237,239,240,242,244,246,247,249,251,253,
    },
    { // 5
        0,  5, 10, 16, 19, 22, 25, 28, 32, 35, 38, 41, 44, 48, 50, 53,
       56, 58, 61, 64, 66, 68, 70, 73, 75, 77, 80, 81, 82, 84, 85, 87,
       88, 90, 91, 93, 94, 96, 96, 97, 98, 99,100,101,102,103,104,104,
      105,106,107,108,109,110,111,112,112,112,112,112,113,113,113,113,
      113,114,114,114,114,115,115,115,115,115,116,116,116,116,117,117,
      117,117,117,118,118,118,118,119,119,119,119,119,120,120,120,120,
      120,121,121,121,121,122,122,122,122,122,123,123,123,123,124,124,
      124,124,124,125,125,125,125,126,126,126,126,126,127,127,127,127,
      128,128,128,128,128,129,129,129,129,129,130,130,130,130,131,131,
      131,131,131,132,132,132,132,133,133,133,133,133,134,134,134,134,
      135,135,135,135,135,136,136,136,136,136,137,137,137,137,138,138,
      138,138,138,139,139,139,139,140,140,140,140,140,141,141,141,141,
      142,142,142,142,142,143,143,143,143,144,144,145,146,147,148,149,
      150,151,152,152,153,154,155,156,157,158,159,160,161,162,164,165,
      167,168,170,171,173,174,176,178,180,182,185,187,189,192,194,197,
      199,202,204,207,210,213,216,219,223,227,231,235,239,243,247,251,
    },
    { // 6
        0,  8, 16, 16, 17, 18, 19, 20, 21, 22, 23, 24, 24, 25, 26, 27,
       28, 29, 30, 31, 32, 32, 33, 34, 35, 36, 37, 38, 39, 40, 40, 41,
       42, 43, 44, 45, 46, 47, 48, 48, 49, 50, 51, 52, 53, 54, 55, 56,
       56, 57, 58, 59, 60, 61, 62, 63, 64, 64, 65, 66, 67, 68, 69, 70,
       71, 72, 72, 73, 74, 75, 76, 77, 78, 79, 80, 80, 81, 82, 83, 84,
       85, 86, 87, 88, 88, 89, 90, 91, 92, 93, 94, 95, 96, 96, 97, 98,
       99,100,101,102,103,104,104,105,106,107,108,109,110,111,112,112,
      113,114,115,116,117,118,119,120,120,121,122,123,124,125,126,127,
      128,128,129,130,131,132,133,134,135,136,136,137,138,139,140,141,
      142,143,144,144,145,146,147,148,149,150,151,152,152,153,154,155,
      156,157,158,159,160,160,161,162,163,164,165,166,167,168,168,169,
      170,171,172,173,174,175,176,176,177,178,179,180,181,182,183,184,
      184,185,186,187,188,189,190,191,192,192,193,194,195,196,197,197,
      198,199,200,201,202,202,203,204,205,206,207,207,208,209,210,211,
      212,213,214,215,215,216,217,218,219,220,221,222,223,223,224,225,
      226,227,228,229,230,231,232,233,234,235,236,237,238,239,244,249,
    },
    { // 7
        0,  0,  0,  1,  1,  1,  2,  2,  2,  3,  3,  4,  4,  4,  5,  5,
        5,  6,  6,  6,  7,  7,  8,  8,  8,  9,  9,  9, 10, 10, 10, 11,
       11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 17, 18, 20,
       21, 22, 24, 25, 26, 28, 29, 30, 32, 33, 34, 36, 37, 38, 40, 41,
       42, 44, 45, 46, 48, 49, 50, 52, 53, 54, 56, 57, 58, 60, 61, 62,
       64, 65, 66, 68, 69, 70, 72, 73, 74, 76, 77, 78, 80, 81, 82, 84,
       85, 86, 88, 89, 90, 92, 93, 94, 96, 97, 98,100,101,102,104,105,
      106,108,109,110,112,113,114,116,117,118,120,121,122,124,125,126,
      128,129,130,132,133,134,136,137,138,140,141,142,144,145,146,148,
      149,150,152,153,154,156,157,158,160,161,162,164,165,166,168,169,
      170,172,173,174,176,177,178,180,181,182,184,185,186,188,189,190,
      192,193,194,195,197,198,199,200,202,203,204,205,207,208,209,211,
      212,213,215,216,217,219,220,221,223,224,225,227,228,229,231,232,
      233,235,236,237,239,239,239,240,240,240,241,241,241,242,242,243,
      243,243,244,244,244,245,245,245,246,246,247,247,247,248,248,248,
      249,249,249,250,250,251,251,251,252,252,252,253,253,253,254,254,
    },
    { // 8
        0,  0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  2,  3,  3,
        3,  3,  4,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,  6,
        7,  7,  7,  7,  8,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10,
       10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14,
       14, 14, 14, 14, 15, 15, 15, 15, 16, 18, 20, 22, 24, 26, 28, 30,
       32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
       64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94,
       96, 98,100,102,104,106,108,110,112,114,116,118,120,122,124,126,
      128,130,132,134,136,138,140,142,144,146,148,150,152,154,156,158,
      160,162,164,166,168,170,172,174,176,178,180,182,184,186,188,190,
      192,193,195,197,199,201,203,205,207,209,211,213,215,217,219,221,
      223,225,227,229,231,233,235,237,239,239,239,239,239,240,240,240,
      240,241,241,241,241,241,242,242,242,242,243,243,243,243,243,244,
      244,244,244,245,245,245,245,245,246,246,246,246,247,247,247,247,
      247,248,248,248,248,249,249,249,249,249,250,250,250,250,251,251,
      251,251,251,252,252,252,252,253,253,253,253,253,254,254,254,254,
    },
    { // 9
        0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  2,  2,  2,
        2,  2,  2,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,
        5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,
        7,  7,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9, 10,
       10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12,
       12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15,
       15, 15, 15, 15, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60,
       64, 68, 72, 76, 80, 84, 88, 92, 96,100,104,108,112,116,120,124,
      128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,
      192,195,199,203,207,211,215,219,223,227,231,235,239,239,239,239,
      239,239,239,240,240,240,240,240,240,241,241,241,241,241,241,242,
      242,242,242,242,242,243,243,243,243,243,243,243,244,244,244,244,
      244,244,245,245,245,245,245,245,246,246,246,246,246,246,247,247,
      247,247,247,247,247,248,248,248,248,248,248,249,249,249,249,249,
      249,250,250,250,250,250,250,251,251,251,251,251,251,251,252,252,
      252,252,252,252,253,253,253,253,253,253,254,254,254,254,254,254,
    },
    { // 10
        0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,
        2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,
        4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,
        6,  6,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,
        8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,
       10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11,
       12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13,
       14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15,
       16, 18, 20, 22, 24, 26, 28, 30, 32, 33, 35, 37, 39, 40, 42, 44,
       46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 65, 67, 69, 71, 72, 74,
       76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 97, 99,101,103,104,
      106,108,110,112,114,116,118,120,122,124,126,128,129,131,133,135,
      136,138,140,142,144,146,148,150,152,154,156,158,160,161,163,165,
      167,168,170,172,174,176,178,180,182,184,186,188,190,192,193,195,
      197,198,200,202,203,205,207,209,211,213,215,217,219,221,223,224,
      226,228,230,231,233,235,237,239,240,242,244,246,247,249,251,253,
    },
    { // 11
        0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,
        4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,
        8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 10, 11, 11, 11, 11,
       12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15,
       16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
       32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
       64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94,
       96, 98,100,102,104,106,108,110,112,114,116,118,120,122,124,126,
      128,130,132,134,136,138,140,142,144,146,148,150,152,154,156,158,
      160,162,164,166,168,170,172,174,176,178,180,182,184,186,188,190,
      192,193,195,197,199,201,203,205,207,209,211,213,215,217,219,221,
      223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,
      239,239,239,239,240,240,240,240,241,241,241,241,242,242,242,242,
      243,243,243,243,244,244,244,244,245,245,245,245,246,246,246,246,
      247,247,247,247,248,248,248,248,249,249,249,249,250,250,250,250,
      251,251,251,251,252,252,252,252,253,253,253,253,254,254,254,254,
    },
    { // 12
        0,  1,  3,  5,  7,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28,
       30, 32, 33, 35, 37, 39, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58,
       60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 78, 80, 82, 84, 86, 88,
       90, 92, 94, 96, 97, 99,101,103,104,106,108,110,112,114,116,118,
      120,122,124,126,128,129,131,133,135,136,138,140,142,144,146,148,
      150,152,154,156,158,160,161,163,165,167,168,170,172,174,176,178,
      180,182,184,186,188,190,192,193,195,197,198,200,202,203,205,207,
      209,211,213,215,217,219,221,223,224,226,228,230,231,233,235,237,
      239,239,239,239,239,239,239,239,240,240,240,240,240,240,240,240,
      241,241,241,241,241,241,241,241,242,242,242,242,242,242,242,242,
      243,243,243,243,243,243,243,243,244,244,244,244,244,244,244,244,
      245,245,245,245,245,245,245,245,246,246,246,246,246,246,246,246,
      247,247,247,247,247,247,247,247,248,248,248,248,248,248,248,248,
      249,249,249,249,249,249,249,249,250,250,250,250,250,250,250,250,
      251,251,251,251,251,251,251,251,252,252,252,252,252,252,252,252,
      253,253,253,253,253,253,253,253,254,254,254,254,254,254,254,254,
    },
    { // 13
        0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  8,
        8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16,
       17, 17, 18, 19, 19, 20, 20, 21, 22, 22, 23, 24, 24, 25, 25, 26,
       27, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 34, 35, 36, 37,
       37, 38, 39, 40, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 48,
       49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63,
       64, 65, 66, 67, 68, 69, 70, 72, 73, 74, 75, 76, 77, 78, 80, 81,
       83, 84, 86, 88, 89, 91, 92, 94, 96, 98,101,104,106,109,112,120,
      128,136,144,146,149,152,154,157,160,161,163,164,166,168,169,171,
      172,174,176,177,178,179,180,181,182,184,185,186,187,188,189,190,
      192,192,193,194,195,196,197,197,198,199,200,201,202,202,203,204,
      205,206,207,207,208,209,209,210,211,212,212,213,214,215,215,216,
      217,217,218,219,220,220,221,222,223,223,224,224,225,226,226,227,
      227,228,229,229,230,231,231,232,232,233,234,234,235,235,236,237,
      237,238,239,239,240,240,241,241,242,242,243,243,244,244,245,245,
      246,247,247,248,248,249,249,250,250,251,251,252,252,253,253,254,
    },
    { // 14
        0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  4,
        4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,  7,  7,  8,  8,
        8,  8,  9,  9,  9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
       13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18,
       18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26,
       26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 32, 32, 33, 34, 35, 36,
       37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 51, 53,
       55, 56, 58, 60, 62, 64, 67, 70, 73, 76, 80, 85, 90, 96,104,112,
      128,144,152,160,165,170,176,179,182,185,188,192,193,195,197,198,
      200,202,203,205,207,207,208,209,210,211,212,213,214,215,215,216,
      217,218,219,220,221,222,223,223,224,224,225,225,226,226,227,227,
      228,228,229,229,230,230,231,231,232,232,233,233,234,234,235,235,
      236,236,237,237,238,239,239,239,239,240,240,240,240,241,241,241,
      241,242,242,242,243,243,243,243,244,244,244,244,245,245,245,246,
      246,246,246,247,247,247,247,248,248,248,249,249,249,249,250,250,
      250,250,251,251,251,252,252,252,252,253,253,253,253,254,254,254,
    },
    { // 15
        0,  1,  3,  5,  7,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28,
       30, 32, 33, 35, 37, 39, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58,
       60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 78, 80, 82, 84, 86, 88,
       90, 92, 94, 96, 97, 99,101,103,104,106,108,110,112,112,112,112,
      112,113,113,113,113,114,114,114,114,115,115,115,115,116,116,116,
      116,116,117,117,117,117,118,118,118,118,119,119,119,119,120,120,
      120,120,120,121,121,121,121,122,122,122,122,123,123,123,123,124,
      124,124,124,124,125,125,125,125,126,126,126,126,127,127,127,127,
      128,128,128,128,128,129,129,129,129,130,130,130,130,131,131,131,
      131,132,132,132,132,132,133,133,133,133,134,134,134,134,135,135,
      135,135,136,136,136,136,136,137,137,137,137,138,138,138,138,139,
      139,139,139,140,140,140,140,140,141,141,141,141,142,142,142,142,
      143,143,143,143,144,145,147,149,151,152,154,156,158,160,162,164,
      166,168,170,172,174,176,177,179,181,183,184,186,188,190,192,193,
      195,197,199,201,203,205,207,208,210,212,214,215,217,219,221,223,
      225,227,229,231,233,235,237,239,240,242,244,246,247,249,251,253,
    },
    { // 16
        0, 16, 21, 26, 32, 33, 35, 37, 39, 40, 42, 44, 46, 48, 48, 49,
       49, 50, 50, 51, 52, 52, 53, 53, 54, 55, 55, 56, 56, 57, 58, 58,
       59, 59, 60, 61, 61, 62, 62, 63, 64, 64, 64, 64, 65, 65, 65, 66,
       66, 66, 67, 67, 67, 68, 68, 68, 69, 69, 69, 69, 70, 70, 70, 71,
       71, 71, 72, 72, 72, 73, 73, 73, 74, 74, 74, 74, 75, 75, 75, 76,
       76, 76, 77, 77, 77, 78, 78, 78, 79, 79, 79, 80, 80, 81, 81, 82,
       82, 83, 84, 84, 85, 85, 86, 87, 87, 88, 88, 89, 90, 90, 91, 91,
       92, 93, 93, 94, 94, 95, 96, 97, 99,101,103,104,106,108,110,112,
      128,144,145,147,149,151,152,154,156,158,160,160,161,161,162,162,
      163,164,164,165,165,166,167,167,168,168,169,170,170,171,171,172,
      173,173,174,174,175,176,176,176,176,177,177,177,178,178,178,179,
      179,179,180,180,180,181,181,181,182,182,182,183,183,183,184,184,
      184,184,185,185,185,186,186,186,187,187,187,188,188,188,189,189,
      189,190,190,190,191,191,191,192,192,193,193,194,194,195,195,196,
      197,197,198,198,199,199,200,200,201,202,202,203,203,204,204,205,
      205,206,207,208,210,212,214,215,217,219,221,223,228,233,239,247,
    },
    { // 17
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
       16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
       32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
       48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
       64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
       80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
       96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
      112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
      128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
      144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
      160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
      176,176,177,177,178,178,179,179,180,180,181,182,182,183,183,184,
      184,185,185,186,187,187,188,188,189,189,190,190,191,192,195,198,
      201,204,207,215,223,225,227,229,231,233,235,237,239,239,239,240,
      240,241,241,242,242,243,243,243,244,244,245,245,246,246,247,247,
      247,248,248,249,249,250,250,251,251,251,252,252,253,253,254,254,
    },
    { // 18
        0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,
        8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16,
       32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39,
       40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47, 48,
       64, 64, 65, 65, 66, 66, 67, 67, 68, 68, 69, 69, 70, 70, 71, 71,
       72, 72, 73, 73, 74, 74, 75, 75, 76, 76, 77, 77, 78, 78, 79, 80,
       96, 96, 97, 97, 98, 98, 99, 99,100,100,101,101,102,102,103,103,
      104,104,105,105,106,106,107,107,108,108,109,109,110,110,111,112,
      128,128,129,129,130,130,131,131,132,132,133,133,134,134,135,135,
      136,136,137,137,138,138,139,139,140,140,141,141,142,142,143,144,
      160,160,161,161,162,162,163,163,164,164,165,165,166,166,167,167,
      168,168,169,169,170,170,171,171,172,172,173,173,174,174,175,176,
      192,192,192,193,193,194,194,195,195,196,196,197,197,198,198,199,
      199,200,200,201,201,202,202,203,203,204,204,205,205,206,206,207,
      223,223,224,224,225,225,226,226,227,227,228,228,229,229,230,231,
      231,232,232,233,233,234,234,235,235,236,236,237,237,238,239,247,
    },
    { // 19
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
       16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23,
       24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 32,
       48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55,
       56, 56, 57, 57, 58, 58, 59, 59, 60, 60, 61, 61, 62, 62, 63, 64,
       80, 80, 81, 81, 82, 82, 83, 83, 84, 84, 85, 85, 86, 86, 87, 87,
       88, 88, 89, 89, 90, 90, 91, 91, 92, 92, 93, 93, 94, 94, 95, 96,
      112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
      128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,144,
      160,160,161,161,162,162,163,163,164,164,165,165,166,166,167,167,
      168,168,169,169,170,170,171,171,172,172,173,173,174,174,175,176,
      192,192,192,193,193,194,194,195,195,196,196,197,197,198,198,199,
      199,200,200,201,201,202,202,203,203,204,204,205,205,206,206,207,
      223,223,224,224,225,225,226,226,227,227,228,228,229,229,230,230,
      231,231,232,232,233,233,234,234,235,235,236,236,237,237,238,239,
      239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,
    },
    { // 20
        0, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23,
       24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 32,
       48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55,
       56, 56, 57, 57, 58, 58, 59, 59, 60, 60, 61, 61, 62, 62, 63, 64,
       80, 80, 81, 81, 82, 82, 83, 83, 84, 84, 85, 85, 86, 86, 87, 87,
       88, 88, 89, 89, 90, 90, 91, 91, 92, 92, 93, 93, 94, 94, 95, 96,
      112,112,113,113,114,114,115,115,116,116,117,117,118,118,119,119,
      120,120,121,121,122,122,123,123,124,124,125,125,126,126,127,128,
      144,144,145,145,146,146,147,147,148,148,149,149,150,150,151,151,
      152,152,153,153,154,154,155,155,156,156,157,157,158,158,159,160,
      176,176,177,177,178,178,179,179,180,180,181,181,182,182,183,183,
      184,184,185,185,186,186,187,187,188,188,189,189,190,190,191,192,
      207,207,208,208,209,209,210,210,211,211,212,212,213,213,214,214,
      215,215,216,216,217,217,218,218,219,219,220,220,221,221,222,223,
      239,239,240,240,241,241,242,242,243,243,244,244,245,245,246,246,
      247,247,248,248,249,249,250,250,251,251,252,252,253,253,254,254,
    },
};
// 从 m_rawData 抽样过调色板(与主图同色系, 含变换表叠加) — 置于 s_cxLUTData 定义之后
QImage MainWindow::buildBscanThumbnail(int w, int h)
{
    if (!m_currentTab || m_rawData.isEmpty() || m_pixelsPerRow <= 0 || m_traceCount <= 0)
        return QImage();
    const unsigned char *cxMap = (m_colorTransformIndex >= 1 && m_colorTransformIndex <= 20)
                                     ? s_cxLUTData[m_colorTransformIndex - 1] : nullptr;
    const int dataSize = m_rawData.size();
    QImage img(w, h, QImage::Format_RGB32);
    for (int x = 0; x < w; ++x) {
        const int trace = qMin(m_traceCount - 1, x * m_traceCount / qMax(1, w - 1));
        for (int y = 0; y < h; ++y) {
            const int samp = qMin(m_pixelsPerRow - 1, y * m_pixelsPerRow / qMax(1, h - 1));
            const int dataIdx = (trace * m_pixelsPerRow + samp) * 4;
            if (dataIdx + 4 > dataSize) { img.setPixel(x, y, qRgb(0, 0, 0)); continue; }
            const qint32 pv = static_cast<qint32>(
                (static_cast<quint8>(m_rawData[dataIdx + 3]) << 24) |
                (static_cast<quint8>(m_rawData[dataIdx + 2]) << 16) |
                (static_cast<quint8>(m_rawData[dataIdx + 1]) << 8) |
                static_cast<quint8>(m_rawData[dataIdx]));
            int lutIdx = pv / (256 * 256) + 128;
            lutIdx = qBound(0, lutIdx, 255);
            img.setPixel(x, y, m_lut[cxMap ? cxMap[lutIdx] : lutIdx]);
        }
    }
    return img;
}

// 线性变换表下拉缩略图重绘: 颜色 = 当前调色板 ∘ 变换表 (RADAN 叠加规律, 随调色板联动)
void MainWindow::refreshCxBarThumbnails()
{
    for (int i = 0; i < m_cxBarLabels.size(); ++i) {
        if (!m_cxBarLabels[i]) continue;
        QPixmap pm(128, 14);
        QPainter pt(&pm);
        const unsigned char *cxMap = s_cxLUTData[i];
        for (int x = 0; x < 128; ++x) {
            const int gray = qRound(x * 255.0 / 127);
            pt.setPen(QPen(QColor(m_lut[cxMap[gray]]), 1));
            pt.drawLine(x, 0, x, 13);
        }
        pt.end();
        m_cxBarLabels[i]->setPixmap(pm);
    }
}

void MainWindow::updateTraceRange()
{
    int maxTrace = qMax(0, m_traceCount - 1);
    if (m_startTraceSpin) {
        m_startTraceSpin->setRange(0, maxTrace);
        m_startTraceSpin->setValue(0);
    }
    if (m_endTraceSpin) {
        m_endTraceSpin->setRange(0, maxTrace);
        m_endTraceSpin->setValue(maxTrace);
    }
}

void MainWindow::refreshImage()
{    if (!m_currentTab || m_rawData.isEmpty()) return;

    const int pixelsPerRow = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int rows = totalPixels / pixelsPerRow;
    int sigPad = 0;
    // zeroApplied(时间零点处理后):数据已上移,只减少 drawRows,不偏移 srcX
    int drawRows = pixelsPerRow;
    int srcOffset = sigPad;  // 数据读取偏移(仅 sigPad 跳预触发;zeroApplied 不偏移)
    if (m_currentTab->zeroApplied) {
        drawRows -= m_currentTab->zeroSkipRows;  // 只减少显示行数,去掉底部零区
    } else {
        srcOffset += 0;
    }
    int skipRows = srcOffset;  // 兼容旧代码变量名

    QImage image(rows, drawRows, QImage::Format_RGB32);

    // 堆积图(wiggle)模式:单独渲染,不走普通热图分支
    if (m_wiggleMode) {
        image = renderWiggleImage(rows, drawRows, skipRows);
        imageLabel->setImage(image);
        return;
    }

    if (m_transformMode == 3) {
        for (int col = 0; col < rows; ++col) {
            std::vector<std::complex<double>> data(512);
            for (int y = 0; y < 512; ++y)
                data[y] = std::complex<double>(getPixelValue(col, y), 0.0);

            fft(data);

            double maxMag = 0;
            double mags[256];
            for (int bin = 0; bin < 256; ++bin) {
                mags[bin] = std::abs(data[bin]) / 512.0;
                if (mags[bin] > maxMag) maxMag = mags[bin];
            }

            for (int bin = 0; bin < 256; ++bin) {
                int normalized = (maxMag > 0) ? static_cast<int>(mags[bin] / maxMag * 128.0) : 0;
                normalized = qBound(0, normalized, 128);

                int gray = 127 + normalized;
                int row = bin * 2;
                image.setPixel(col, row, qRgb(gray, gray, gray));
                if (row - 1 >= 0)
                    image.setPixel(col, row - 1, qRgb(gray, gray, gray));
            }
        }
    } else {
        int dataSize = m_rawData.size();
        int bytesPerPixel = 4;

        // RADAN 叠加规律(1_1方案叠加规律.png 数值闭环验证): 最终颜色 = 调色板[变换表(灰度)]
        const unsigned char *cxMap = (m_colorTransformIndex >= 1 && m_colorTransformIndex <= 20)
                                         ? s_cxLUTData[m_colorTransformIndex - 1] : nullptr;

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < drawRows; ++x) {
                int srcX = x + srcOffset;
                int dataIdx = (y * pixelsPerRow + srcX) * bytesPerPixel;
                if (dataIdx + 4 > dataSize) continue;
                qint32 pixelValue = static_cast<qint32>(
                    (static_cast<quint8>(m_rawData[dataIdx + 3]) << 24) |
                    (static_cast<quint8>(m_rawData[dataIdx + 2]) << 16) |
                    (static_cast<quint8>(m_rawData[dataIdx + 1]) << 8) |
                    (static_cast<quint8>(m_rawData[dataIdx]))
                );

                int pixelValue_display = pixelValue;

                if (m_transformMode == 1)
                    pixelValue_display = qAbs(pixelValue_display);
                else if (m_transformMode == 2)
                    pixelValue_display = -pixelValue_display;

                // Apply display gain (right-click menu, visual only)
                float displayGain = (m_currentTab && m_currentTab->gainApplied) ? 1.0f : m_gain;
                float dv = displayGain * static_cast<float>(pixelValue_display);
                if (dv > 8388607.0f) dv = 8388607.0f;
                if (dv < -8388608.0f) dv = -8388608.0f;
                pixelValue_display = static_cast<int>(dv);

                int lutIdx = pixelValue_display / (256 * 256) + 128;
                if (lutIdx < 0) lutIdx = 0;
                if (lutIdx > 255) lutIdx = 255;
                image.setPixel(y, x, m_lut[cxMap ? cxMap[lutIdx] : lutIdx]);
            }
        }
    }

    image = image.convertToFormat(QImage::Format_RGB32);
    imageLabel->setImage(image);
}

void MainWindow::updateRulers()
{
    if (!m_currentTab || m_rawData.isEmpty()) return;

    int totalPixels = m_rawData.size() / 4;
    m_traceCount = totalPixels / m_pixelsPerRow;
    m_currentTab->traceCount = m_traceCount;
    updateTraceRange();

    // Time range from file header record length (rhf_range)
    float sigPos = m_currentTab->signalPosition;
    double range = m_currentTab->headerRange;   // 记录长度 ns (offset 26)
    double epsr  = m_currentTab->epsr;          // 介电常数 (offset 54)
    (void)sigPos;
    // 时间零点处理后(zeroApplied),时间范围按实际显示行数缩短
    int skipR = (m_currentTab->zeroApplied) ? m_currentTab->zeroSkipRows : 0;
    int drawR = m_pixelsPerRow - skipR;
    m_timeRange = range * drawR / m_pixelsPerRow;  // 时间标尺 RANGE(处理后的有效范围)
    if (epsr > 0.0)
        m_depthRange = 0.299792458 * m_timeRange / (2.0 * std::sqrt(epsr));  // c·t/(2√εr), c=0.2998 m/ns
    else
        m_depthRange = m_timeRange * 0.05;      // 介电缺失兜底
    m_currentTab->timeRange = m_timeRange;
    m_currentTab->depthRange = m_depthRange;

    int sigPad = 0;   // 不再用 sigPos 裁剪行(始终显示全部 512 采样点)
    int skipRows = sigPad + (m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0);
    int drawRows = m_pixelsPerRow - skipRows;
    m_currentTab->topRuler->setDataRange(m_traceCount);
    m_currentTab->leftRuler->setRange(0, m_timeRange);
    m_currentTab->rightRuler->setRange(0, m_depthRange);
    m_currentTab->leftRuler->setImageHeight(drawRows);
    m_currentTab->rightRuler->setImageHeight(drawRows);
}

void MainWindow::resizeImageLabel()
{
    if (!m_currentTab || m_rawData.isEmpty()) return;

    int sigPad = 0;   // 始终显示全部 512 采样点
    int skipRows = sigPad + (m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0);
    int drawRows = m_pixelsPerRow - skipRows;
    int viewH = m_currentTab->scrollArea->viewport()->height();
    if (viewH <= 0) viewH = drawRows;

    // 逻辑图像宽度:普通模式=道数,堆积图模式=(道数/2)*32(每堆积32列,步长2)
    int logicalW = m_wiggleMode ? (((m_traceCount + 1) / 2) * 32) : m_traceCount;

    m_currentTab->imageLabel->setFixedSize(qMax(1, qRound(logicalW * m_hZoom)), viewH);

    int imgW = qMax(1, qRound(logicalW * m_hZoom));
    int maxVal = qMax(0, imgW - m_currentTab->scrollArea->viewport()->width());
    m_currentTab->extHScrollBar->setRange(0, maxVal);
    m_currentTab->extHScrollBar->setPageStep(m_currentTab->scrollArea->viewport()->width());
    m_currentTab->extHScrollBar->setVisible(maxVal > 0);

    // 道号标尺:同步当前水平缩放,使显示的道号 RANGE 随放大/缩小而变化
    // 堆积图模式下每道占 16 个显示像素(32列槽/步长2),故等效 zoom = 16*m_hZoom
    float rulerZoom = m_wiggleMode ? (16.0f * m_hZoom) : m_hZoom;
    m_currentTab->topRuler->setZoom(rulerZoom);
    // v1.0.98: 注入编辑覆盖层映射参数(trace/sample域 ↔ widget像素)
    m_currentTab->imageLabel->setGeometryForMapping(m_traceCount, drawRows, m_hZoom, m_wiggleMode ? 2 : 0);
    m_currentTab->leftRuler->update();
    m_currentTab->rightRuler->update();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 文档区尺寸/显隐变化:重定位文件切换三角按钮(延迟到事件处理完毕,几何/可见性才准确)
    if (watched == m_docSplitter &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        QTimer::singleShot(0, this, [this]() { repositionSwitchButton(); });
    }
    // welcome 底部图标悬停:放大该图标(Enter 显示,Leave 隐藏)
    int whi = m_welcomeHotspots.indexOf(qobject_cast<QWidget*>(watched));
    if (whi >= 0 && m_welcomeZoom) {
        if (event->type() == QEvent::Enter && whi < m_welcomeIconPix.size()) {
            int W = welcomeLabel->width(), H = welcomeLabel->height();
            int iw = m_welcomePix.width(), ih = m_welcomePix.height();
            double s = qMin(double(W) / iw, double(H) / ih);
            int dw = int(iw * s), dh = int(ih * s), ox = (W - dw) / 2, oy = (H - dh) / 2;
            static const double ICON_CX[4] = {0.304, 0.444, 0.580, 0.714};  // 图标中心(亮度检测实测)
            int gh = int(0.105 * dh);
            int side = gh * 2;                              // 圆形直径 ≈ 图标高度 2 倍
            int zx = ox + int(ICON_CX[whi] * dw) - side / 2;
            int zy = oy + int(0.912 * dh) - side / 2;
            m_welcomeZoom->setPixmap(m_welcomeIconPix[whi].scaled(side, side, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            m_welcomeZoom->setGeometry(zx, zy, side, side);
            m_welcomeZoom->setMask(QRegion(m_welcomeZoom->rect(), QRegion::Ellipse));  // 圆形遮罩
            m_welcomeZoom->raise();
            m_welcomeZoom->show();
            // 功能说明显示在该图标的右上角(图标上方偏右,不超出图片范围)
            int iconCx = ox + int(ICON_CX[whi] * dw);
            int iconTopY = oy + int(0.86 * dh);
            int tipW = int(0.32 * dw), tipH = int(0.22 * dh);
            int tipX = iconCx + int(0.02 * dw);
            int tipY = iconTopY - tipH - 8;
            if (tipX + tipW > ox + dw - 4) tipX = ox + dw - tipW - 4;   // 不超出右边
            if (tipY < oy + 4) tipY = oy + 4;                           // 不超出顶部
            m_welcomeTip->setText(m_welcomeTips.value(whi));
            m_welcomeTip->setGeometry(tipX, tipY, tipW, tipH);
            m_welcomeTip->raise();
            m_welcomeTip->show();
        } else if (event->type() == QEvent::Leave) {
            m_welcomeZoom->hide();
            m_welcomeTip->hide();
        }
        return QMainWindow::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Resize) {
        // Handle viewport resize for ALL tabs (not just current)
        for (auto *tab : m_tabs) {
            if (watched == tab->scrollArea->viewport()) {
                if (tab == m_currentTab) {
                    resizeImageLabel();
                } else {
                    // Resize non-current tab's image independently
                    int sigPad = 0;   // 始终显示全部 512 采样点
                    int skipRows = sigPad + (tab->zeroApplied ? tab->zeroSkipRows : 0);
                    int drawRows = tab->pixelsPerRow - skipRows;
                    int viewH = tab->scrollArea->viewport()->height();
                    if (viewH <= 0) viewH = drawRows;

                    int traceCount = tab->rawData.size() / 4 / tab->pixelsPerRow;
                    tab->imageLabel->setFixedSize(traceCount, viewH);

                    int maxVal = qMax(0, traceCount - tab->scrollArea->viewport()->width());
                    tab->extHScrollBar->setRange(0, maxVal);
                    tab->extHScrollBar->setPageStep(tab->scrollArea->viewport()->width());
                    tab->extHScrollBar->setVisible(maxVal > 0);

                    tab->leftRuler->update();
                    tab->rightRuler->update();
                }
                return QMainWindow::eventFilter(watched, event);
            }
        }
    }
    if (event->type() == QEvent::ContextMenu) {
        auto *tabBar = qobject_cast<QTabBar*>(watched);
        if (tabBar) {
            // Find which tab group owns this tab bar
            QTabWidget *srcGroup = nullptr;
            for (auto *grp : m_tabGroups) {
                if (grp->tabBar() == tabBar) { srcGroup = grp; break; }
            }
            if (srcGroup) {
                auto *ctx = static_cast<QContextMenuEvent*>(event);
                int idx = tabBar->tabAt(ctx->pos());
                if (idx >= 0) {
                    QMenu menu;
                    QAction *actPrev = nullptr, *actNext = nullptr;
                    QAction *actNewH = nullptr, *actNewV = nullptr;
                    QTabWidget *prevGrp = nullptr, *nextGrp = nullptr;

                    // 前提:源组有 2 个及以上文件才允许分割/移动
                    if (srcGroup->count() >= 2) {
                        // 定位源组在父 splitter 中的相邻"组"及排列方向
                        QSplitter *parent = qobject_cast<QSplitter*>(srcGroup->parentWidget());
                        if (!parent) parent = m_docSplitter;
                        int sidx = parent->indexOf(srcGroup);
                        for (int i = sidx - 1; i >= 0 && !prevGrp; --i)
                            prevGrp = qobject_cast<QTabWidget*>(parent->widget(i));
                        for (int i = sidx + 1; i < parent->count() && !nextGrp; ++i)
                            nextGrp = qobject_cast<QTabWidget*>(parent->widget(i));
                        // 父 splitter 垂直堆叠 → 水平选项卡组(上/下);水平并排 → 垂直选项卡组(左/右)
                        bool horizLayout = (parent->orientation() == Qt::Vertical);

                        // 仅当存在 2+ 组且有相邻组时提供"移动"
                        if (m_tabGroups.size() >= 2 && (prevGrp || nextGrp)) {
                            if (horizLayout) {
                                if (prevGrp) actPrev = menu.addAction(QString::fromUtf8("移到上一个选项卡组"));
                                if (nextGrp) actNext = menu.addAction(QString::fromUtf8("移到下一个选项卡组"));
                            } else {
                                if (prevGrp) actPrev = menu.addAction(QString::fromUtf8("移到左一个选项卡组"));
                                if (nextGrp) actNext = menu.addAction(QString::fromUtf8("移到右一个选项卡组"));
                            }
                            menu.addSeparator();
                        }

                        // 已有分组:只能新建同向;仅 1 组:水平/垂直可选
                        if (m_tabGroups.size() >= 2) {
                            if (horizLayout) actNewH = menu.addAction(QString::fromUtf8("新建水平选项卡组"));
                            else             actNewV = menu.addAction(QString::fromUtf8("新建垂直选项卡组"));
                        } else {
                            actNewH = menu.addAction(QString::fromUtf8("新建水平选项卡组"));
                            actNewV = menu.addAction(QString::fromUtf8("新建垂直选项卡组"));
                        }
                        menu.addSeparator();
                    }

                    menu.addAction(QString::fromUtf8("取消(&C)"));
                    QAction *chosen = menu.exec(ctx->globalPos());
                    if (chosen) {
                        if (chosen == actPrev && prevGrp) moveTabToGroup(srcGroup, idx, prevGrp);
                        else if (chosen == actNext && nextGrp) moveTabToGroup(srcGroup, idx, nextGrp);
                        else if (chosen == actNewH) splitHorizontal(srcGroup, idx);
                        else if (chosen == actNewV) splitVertical(srcGroup, idx);
                        // 取消:仅关闭菜单,不做任何操作
                    }
                }
                return true;
            }
        }
    }
    // Handle left-click on tab bar: activate group & start drag tracking
    if (event->type() == QEvent::MouseButtonPress) {
        auto *tabBar = qobject_cast<QTabBar*>(watched);
        if (tabBar) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                int idx = tabBar->tabAt(me->pos());
                if (idx >= 0) {
                    // Reset drag state
                    m_dragging = false;
                    m_dragSrcIdx = -1;
                    m_dragSrcGroup = nullptr;

                    for (auto *grp : m_tabGroups) {
                        if (grp->tabBar() == tabBar) {
                            m_activeTabGroup = grp;
                            updateGroupStyles(grp, m_tabGroups);
                            // Record potential drag start
                            m_dragSrcGroup = grp;
                            m_dragSrcIdx = idx;
                            m_dragStartPos = me->globalPosition().toPoint();
                            // Find TabData for the clicked tab
                            QWidget *page = grp->widget(idx);
                            for (auto *t : m_tabs) {
                                if (t->page == page) {
                                    if (m_currentTab != t) {
                                        m_currentTab = t;
                                        m_rawData = t->rawData;
                                        m_dataOffset = t->dataOffset;
                                        m_pixelsPerRow = t->pixelsPerRow;
                                        m_gain = t->gain;
                                        m_transformMode = t->transformMode;
                                        m_traceCount = t->traceCount;
                                        m_timeRange = t->timeRange;
                                        m_depthRange = t->depthRange;
                                        scrollArea = t->scrollArea;
                                        imageLabel = t->imageLabel;
                                        chartView = t->chartView;
                                        chartSeries = t->chartSeries;
                                        m_btnApply->setText(t->gainApplied ? "撤销" : "应用");
                                        updateWindowTitle();   // 切换活动标签页时同步窗口标题(覆盖 currentChanged 不触发的情形)
                                    }
                                    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
                                        QString fname = QFileInfo(t->filePath).fileName();
                                        m_oneClickDlg->setWindowTitle(QString("一键处理 - %1").arg(fname));
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    // Handle mouse move: enter drag mode
    if (event->type() == QEvent::MouseMove && m_dragSrcGroup && m_dragSrcIdx >= 0) {
        auto *tabBar = qobject_cast<QTabBar*>(watched);
        if (tabBar) {
            auto *me = static_cast<QMouseEvent*>(event);
            QPoint delta = me->globalPosition().toPoint() - m_dragStartPos;
            if (!m_dragging && (delta.manhattanLength() > 5)) {
                m_dragging = true;
                tabBar->setCursor(Qt::DragMoveCursor);
            }
        }
    }
    // Handle mouse release: execute tab drop
    if (event->type() == QEvent::MouseButtonRelease && m_dragSrcGroup) {
        auto *tabBar = qobject_cast<QTabBar*>(watched);
        if (tabBar) {
            tabBar->setCursor(Qt::ArrowCursor);
            // 先捕获拖拽状态并立即复位,避免后续事件干扰
            QTabWidget *srcGroup = m_dragSrcGroup;
            int srcIdx = m_dragSrcIdx;
            bool dragging = m_dragging;
            m_dragging = false;
            m_dragSrcGroup = nullptr;
            m_dragSrcIdx = -1;

            if (dragging && srcGroup && srcIdx >= 0 && srcIdx < srcGroup->count()) {
                auto *me = static_cast<QMouseEvent*>(event);
                QPoint globalPos = me->globalPosition().toPoint();

                // 找鼠标释放位置所在的目标组
                QTabWidget *dstGroup = nullptr;
                for (auto *grp : m_tabGroups) {
                    if (grp == srcGroup) continue;
                    if (grp->rect().contains(grp->mapFromGlobal(globalPos))) {
                        dstGroup = grp;
                        break;
                    }
                }

                if (dstGroup) {
                    // 关键:延迟到当前鼠标释放事件处理完毕后再移动/删除。
                    // 若在此事件中直接 delete 源组(其 tabBar 即 watched),
                    // 会造成 use-after-free,程序稳定闪退。
                    QTabWidget *src = srcGroup, *dst = dstGroup;
                    int idx = srcIdx;
                    QTimer::singleShot(0, this, [this, src, dst, idx]() {
                        if (!m_tabGroups.contains(src) || !m_tabGroups.contains(dst)) return;
                        moveTabToGroup(src, idx, dst);   // 复用右键移动逻辑(已验证稳定)
                    });
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateWelcomePixmap()
{
    // welcome 图片保持比例缩放铺满(KeepAspectRatio),居中显示
    if (!welcomeLabel || !welcomeLabel->isVisible() || m_welcomePix.isNull()) return;
    int W = welcomeLabel->width(), H = welcomeLabel->height();
    if (W < 2 || H < 2) return;
    welcomeLabel->setPixmap(m_welcomePix.scaled(W, H, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 计算 KeepAspectRatio 缩放后图片在标签里的实际显示矩形(居中)
    int iw = m_welcomePix.width(), ih = m_welcomePix.height();
    double s = qMin(double(W) / iw, double(H) / ih);
    int dw = int(iw * s), dh = int(ih * s);
    int ox = (W - dw) / 2, oy = (H - dh) / 2;

    // 4 个功能图标实测中心(挤在图中下部),热区覆盖图标+下方文字
    static const double ICON_CX[4] = {0.304, 0.444, 0.580, 0.714};
    const double rw = 0.09, rh = 0.16, ry = 0.84;
    for (int i = 0; i < m_welcomeHotspots.size() && i < 4; ++i) {
        m_welcomeHotspots[i]->setGeometry(
            ox + int((ICON_CX[i] - rw / 2.0) * dw), oy + int(ry * dh),
            int(rw * dw), int(rh * dh));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    resizeImageLabel();
    repositionSwitchButton();   // 窗体尺寸变化:三角按钮跟随文档区右上角
    // v1.0.98: 底部标记面板高度 = 窗体 35%(min 200), 变化触发视口 resize → resizeImageLabel
    if (m_markerPanel && m_markerPanel->isVisible())
        m_markerPanel->setFixedHeight(qMax(200, height() * 35 / 100));
    // 延后到布局重算完成后再缩放(此时 welcomeLabel 尺寸才正确),否则启动时用旧尺寸、要拖动才生效
    QTimer::singleShot(0, this, [this]() { updateWelcomePixmap(); });
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" && message) {
        MSG *msg = reinterpret_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            const long borderWidth = 5;
            long x = GET_X_LPARAM(msg->lParam);
            long y = GET_Y_LPARAM(msg->lParam);
            RECT winrect;
            GetWindowRect(reinterpret_cast<HWND>(winId()), &winrect);
            bool onLeft   = x <  winrect.left   + borderWidth;
            bool onRight  = x >= winrect.right  - borderWidth;
            bool onTop    = y <  winrect.top    + borderWidth;
            bool onBottom = y >= winrect.bottom - borderWidth;
            if (onTop    && onLeft)  { *result = HTTOPLEFT;     return true; }
            if (onTop    && onRight) { *result = HTTOPRIGHT;    return true; }
            if (onBottom && onLeft)  { *result = HTBOTTOMLEFT;  return true; }
            if (onBottom && onRight) { *result = HTBOTTOMRIGHT; return true; }
            if (onLeft)              { *result = HTLEFT;        return true; }
            if (onRight)             { *result = HTRIGHT;       return true; }
            if (onTop)               { *result = HTTOP;         return true; }
            if (onBottom)            { *result = HTBOTTOM;      return true; }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::updateWindowTitle()
{
    QString text = QString::fromUtf8("劳雷");
    if (m_currentTab && !m_currentTab->filePath.isEmpty()) {
        QString fname = QFileInfo(m_currentTab->filePath).completeBaseName();
        text = QString::fromUtf8("劳雷AI数据处理-%1").arg(fname);
    }
    setWindowTitle(text);  // 同步 OS 任务栏标题(顶栏品牌固定为"劳雷",不随文件变)
}

bool MainWindow::requireOpenFile()
{
    if (!m_currentTab) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                QString::fromUtf8("请先打开 DZT 文件"));
        return false;
    }
    return true;
}

void MainWindow::loadLUT(int index)
{
    m_paletteIndex = index;

    // Default: grayscale fallback
    for (int i = 0; i < 256; ++i)
        m_lut[i] = qRgb(i, i, i);

    QString path = QString(":/icons/resources/lut%1.txt").arg(index);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "LUT failed to open:" << path;
        return;
    }
    qDebug() << "LUT loaded:" << path;

    int idx = 0;
    while (!f.atEnd() && idx < 256) {
        QByteArray line = f.readLine().trimmed();
        if (line.length() < 6) continue;
        bool ok = false;
        uint val = line.toUInt(&ok, 16);
        if (!ok) continue;
        int r = (val >> 16) & 0xFF;
        int g = (val >> 8) & 0xFF;
        int b = val & 0xFF;
        m_lut[idx++] = qRgb(r, g, b);
    }
}

void MainWindow::createMenuBar()
{
    ribbonTab = new QTabWidget(this);
    ribbonTab->setTabPosition(QTabWidget::North);
    ribbonTab->setDocumentMode(true);
    ribbonTab->setFixedHeight(120);          // 设计稿 ribbon-height
    ribbonTab->tabBar()->hide();             // 5 个模块标签上移至 TopBar
    ribbonTab->setStyleSheet(
        "QTabWidget::pane { border: none; background: #ffffff; border-bottom: 1px solid #c3c6d6; }"
    );

    // --- Tab: 主页 (v1.0.87 严格按 主页-文件头.png: 4组,组名在底部,组间竖分隔线,Material Symbols 图标) ---
    QWidget *startPage = new QWidget();
    QHBoxLayout *startLayout = new QHBoxLayout(startPage);
    startLayout->setContentsMargins(8, 8, 8, 4);
    startLayout->setSpacing(0);

    const QColor cOutline(0x73, 0x77, 0x85);   // 灰图标 outline
    const QColor cPrimary(0x00, 0x48, 0xaf);   // 主色 primary
    const QColor cDark(0x12, 0x1c, 0x2a);      // hover 前景 on-surface

    // 组容器(数据处理标签沿用): 框式组
    auto addGroup = [](QHBoxLayout *parentLayout, const QString &groupName) -> QVBoxLayout* {
        QFrame *frame = new QFrame();
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Plain);
        frame->setStyleSheet("QFrame { border: 1px solid #d0d0d0; border-radius: 3px; background: #fafafa; }");
        QVBoxLayout *groupLayout = new QVBoxLayout(frame);
        groupLayout->setContentsMargins(4, 2, 4, 2);
        groupLayout->setSpacing(4);
        QLabel *groupLabel = new QLabel(groupName);
        groupLabel->setAlignment(Qt::AlignCenter);
        groupLabel->setStyleSheet("color: #666; font-size: 10px; border: none;");
        groupLayout->addWidget(groupLabel);
        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->setSpacing(2);
        groupLayout->insertLayout(0, btnRow);
        parentLayout->addWidget(frame);
        return groupLayout;
    };

    // 主页组容器: 按钮行在上 + 组名(11px粗体)在底部; 非末组右侧 1px 竖分隔线
    auto addRibbonGroup = [](QHBoxLayout *parentLayout, const QString &groupName, bool last = false) -> QHBoxLayout* {
        QWidget *group = new QWidget();
        group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);   // 防拉伸: 组容器不可增长
        QVBoxLayout *groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(2);
        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->setContentsMargins(12, 10, 12, 0);
        btnRow->setSpacing(2);
        groupLayout->addLayout(btnRow, 1);
        QLabel *groupLabel = new QLabel(groupName);
        groupLabel->setAlignment(Qt::AlignCenter);
        groupLabel->setStyleSheet(
            "color: #424654; font-size: 11px; font-weight: bold; letter-spacing: 1px;"
            " border: none; background: transparent;");
        groupLayout->addWidget(groupLabel);
        parentLayout->addWidget(group);
        if (!last) {
            QFrame *sep = new QFrame();
            sep->setFrameShape(QFrame::NoFrame);
            sep->setFixedWidth(1);
            sep->setStyleSheet("background: #c3c6d6; border: none;");
            parentLayout->addWidget(sep);
        }
        return btnRow;
    };

    // 图标在上文字在下的 ribbon 按钮(24px Material Symbols 字形)
    auto ribbonBtn = [&cOutline, &cPrimary, &cDark](const QString &glyph, const QString &text,
                                                    bool primaryIcon, int minW = 50) -> QToolButton* {
        QToolButton *btn = new QToolButton();
        if (MatIcon::ready())
            btn->setIcon(MatIcon::icon(glyph, primaryIcon ? cPrimary : cOutline, QColor(), cDark, 24));
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIconSize(QSize(24, 24));
        btn->setMinimumSize(minW, 52);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);   // 防拉伸: 钉死自然宽度
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QToolButton { border: none; border-radius: 2px; background: transparent;"
            " font-size: 12px; color: #121c2a; padding: 2px; }"
            "QToolButton:hover { background: #dee9fc; }"
            "QToolButton:pressed { background: #c9d8f0; }");
        return btn;
    };

    // 显示模式按钮(可选中): active 态 bg#1e60d5 前景#dee5ff 底部2px#0048af (按设计稿)
    auto displayBtn = [&cOutline, &cDark](const QString &glyph, const QString &text, int minW = 60) -> QToolButton* {
        QToolButton *btn = new QToolButton();
        if (MatIcon::ready())
            btn->setIcon(MatIcon::icon(glyph, cOutline, QColor(0xde, 0xe5, 0xff), cDark, 24));
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIconSize(QSize(24, 24));
        btn->setMinimumSize(minW, 52);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);   // 防拉伸: 钉死自然宽度
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QToolButton { border: none; border-bottom: 2px solid transparent; border-radius: 2px;"
            " background: transparent; font-size: 12px; color: #121c2a; padding: 2px; }"
            "QToolButton:hover { background: #dee9fc; }"
            "QToolButton:pressed { background: #c9d8f0; }"
            "QToolButton:checked { background: #1e60d5; color: #dee5ff; border-bottom: 2px solid #0048af; }"
            "QToolButton:checked:hover { background: #1e60d5; }"
            "QToolButton:checked:pressed { background: #1e60d5; }");
        return btn;
    };

    // ===== 组1: 文件操作 =====
    QHBoxLayout *fileRow = addRibbonGroup(startLayout, QString::fromUtf8("文件操作"));
    QToolButton *btnOpen = ribbonBtn(QStringLiteral("folder_open"), QString::fromUtf8("打开"), true);
    QToolButton *btnClose = ribbonBtn(QStringLiteral("close"), QString::fromUtf8("关闭"), false);
    QToolButton *btnSave = ribbonBtn(QStringLiteral("save"), QString::fromUtf8("保存"), false);
    fileRow->addWidget(btnOpen);
    fileRow->addWidget(btnClose);
    fileRow->addWidget(btnSave);

    connect(btnOpen, &QToolButton::clicked, this, &MainWindow::onOpenFile);
    connect(btnClose, &QToolButton::clicked, this, [this]() {
        if (!m_tabs.isEmpty()) {
            int idx = m_docTabWidget->currentIndex();
            if (idx >= 0) closeTab(idx);
        }
    });
    connect(btnSave, &QToolButton::clicked, this, [this]() {
        if (!m_currentTab) return;
        saveProcessedFile();
    });

    // ===== 组2: 图像显示 (三种视图模式, 三选一互斥) =====
    QHBoxLayout *dispRow = addRibbonGroup(startLayout, QString::fromUtf8("图像显示"));
    QToolButton *btnLineScan = displayBtn(QStringLiteral("view_agenda"), QString::fromUtf8("线扫描"));
    QToolButton *btnLineWave = displayBtn(QStringLiteral("ssid_chart"), QString::fromUtf8("线扫描+波形"), 84);
    QToolButton *btnWiggle = displayBtn(QStringLiteral("water"), QString::fromUtf8("波列图"));
    dispRow->addWidget(btnLineScan);
    dispRow->addWidget(btnLineWave);
    dispRow->addWidget(btnWiggle);
    m_displayGroup = new QButtonGroup(startPage);   // id: 0=线扫描 1=线扫描+波形 2=波列图 (三选一)
    m_displayGroup->setExclusive(true);
    m_displayGroup->addButton(btnLineScan, 0);
    m_displayGroup->addButton(btnLineWave, 1);
    m_displayGroup->addButton(btnWiggle, 2);
    btnLineScan->setChecked(true);

    // 波列图(wiggle)渲染开关
    auto setWiggle = [this](bool on) {
        m_wiggleMode = on;
        if (m_currentTab) {
            m_currentTab->wiggleMode = on;
            m_currentTab->imageLabel->setCrosshairDark(on);  // 堆积图白底→黑十字
        }
        refreshImage();
        resizeImageLabel();
    };

    // 线扫描: 只显示 B-SCAN — 隐藏 A-SCAN 波形列, 关闭 wiggle 渲染
    connect(btnLineScan, &QToolButton::clicked, this, [this, btnLineScan, setWiggle]() {
        btnLineScan->setChecked(true);
        m_showAscan = false;
        if (m_wiggleMode) setWiggle(false);
        syncAscanVisibility();
    });
    // 线扫描+波形: B-SCAN + A-SCAN(波形)并列显示 — 关闭 wiggle, 恢复纯波形标尺(无增益手柄)
    connect(btnLineWave, &QToolButton::clicked, this, [this, btnLineWave, setWiggle]() {
        btnLineWave->setChecked(true);
        m_showAscan = true;
        if (m_wiggleMode) setWiggle(false);
        syncAscanVisibility();
        const bool editing = m_leftPanel && m_leftPanel->isVisible();
        if (chartView && !m_rawData.isEmpty()) {
            if (!editing) {
                chartView->setGainVisible(false);
                chartView->setYScale(1.0f);
                QValueAxis *axisY = qobject_cast<QValueAxis*>(chartView->chart()->axisY(chartSeries));
                if (axisY) {
                    axisY->setRange(0, m_pixelsPerRow - 1);
                    axisY->setLabelFormat("%d");
                }
            }
            updateChart(m_lastChartX);
        }
    });
    // 波列图: 纯 wiggle 视图 — 隐藏 A-SCAN 波形列 (退出波列图请点线扫描/线扫描+波形)
    m_btnStack = btnWiggle;
    connect(btnWiggle, &QToolButton::clicked, this, [this, btnWiggle, btnLineScan, setWiggle]() {
        if (!requireOpenFile()) {
            btnLineScan->setChecked(true);   // 无文件回退线扫描(互斥组内, 程序化setChecked不发clicked)
            return;
        }
        btnWiggle->setChecked(true);   // 点击已选中的波列图保持选中
        m_showAscan = false;
        setWiggle(true);
        syncAscanVisibility();
    });

    // ===== 组3: 色彩渲染 (两行下拉框样式, 按设计稿) =====
    QHBoxLayout *colorRow = addRibbonGroup(startLayout, QString::fromUtf8("色彩渲染"));
    QVBoxLayout *colorRows = new QVBoxLayout();
    colorRows->setSpacing(6);
    colorRow->addLayout(colorRows);

    // 下拉框样式的行按钮: [16px图标] 文字 ▾
    auto comboRowBtn = [&cDark](const QString &glyph, const QString &text) -> QToolButton* {
        QToolButton *btn = new QToolButton();
        if (MatIcon::ready())
            btn->setIcon(MatIcon::icon(glyph, QColor(0x42, 0x46, 0x54), QColor(), cDark, 16));
        btn->setText(text + QString(QChar(0x25BE)));   // ▾
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setIconSize(QSize(16, 16));
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QToolButton { border: 1px solid #c3c6d6; border-radius: 2px; background: #ffffff;"
            " font-size: 12px; color: #121c2a; padding: 3px 8px; }"
            "QToolButton:hover { background: #dee9fc; border: 1px solid #737785; }"
            "QToolButton::menu-indicator { image: none; }"
            "QToolButton::down-arrow { image: none; }");
        return btn;
    };

    // 彩虹色(调色板): 现有 30 调色板悬停预览菜单逻辑原样保留
    {
        QToolButton *paletteBtn = comboRowBtn(QStringLiteral("palette"), QString::fromUtf8("彩虹色"));
        QMenu *paletteMenu = new QMenu(paletteBtn);

        // 事件过滤器：悬停预览 + 点击确认
        class PaletteItemFilter : public QObject {
            int &m_paletteIndex;
            int &m_hoverIndex;
            QVector<QLabel*> &m_numLabels;
            std::function<void(int)> m_onPreview;
        public:
            PaletteItemFilter(int &palIdx, int &hoverIdx, QVector<QLabel*> &labels, std::function<void(int)> onPreview, QObject *parent = nullptr)
                : QObject(parent), m_paletteIndex(palIdx), m_hoverIndex(hoverIdx), m_numLabels(labels), m_onPreview(onPreview) {}
            bool eventFilter(QObject *watched, QEvent *event) override {
                QWidget *w = qobject_cast<QWidget*>(watched);
                if (!w) return QObject::eventFilter(watched, event);
                int idx = w->property("paletteIndex").toInt();
                if (event->type() == QEvent::Enter) {
                    w->setStyleSheet("QWidget { background: #E8E8E8; border: 1px solid #888888; border-radius: 2px; }");
                    if (idx >= 1 && idx <= 30) {
                        m_hoverIndex = idx;
                        for (int j = 1; j <= 30; ++j) {
                            if (m_numLabels[j]) {
                                m_numLabels[j]->setStyleSheet(
                                    j == idx
                                        ? "background: #4A90D9; color: white; border: 1px solid #3A7BD5; border-radius: 3px; padding: 2px;"
                                        : "border: 1px solid #AAAAAA; border-radius: 3px; padding: 2px;");
                            }
                        }
                        if (m_onPreview) m_onPreview(idx);
                    }
                } else if (event->type() == QEvent::Leave) {
                    w->setStyleSheet("QWidget { background: transparent; border: none; }");
                } else if (event->type() == QEvent::MouseButtonPress) {
                    if (idx >= 1 && idx <= 30) {
                        m_paletteIndex = idx;
                    }
                }
                return QObject::eventFilter(watched, event);
            }
        };

        int *hoverIndex = new int(m_paletteIndex);

        QVector<QLabel*> *numLabels = new QVector<QLabel*>(31, nullptr);
        PaletteItemFilter *hoverFilter = new PaletteItemFilter(m_paletteIndex, *hoverIndex, *numLabels,
            [this](int idx) {
                loadLUT(idx);
                refreshCxBarThumbnails();   // 变换表缩略图随调色板联动
                if (m_currentTab) refreshImage();
            }, paletteMenu);

        for (int i = 1; i <= 30; ++i) {
            QString iconPath = QString(":/icons/resources/palette_bar%1.png").arg(i);
            QPixmap pix(iconPath);

            QWidgetAction *wa = new QWidgetAction(paletteMenu);
            QWidget *itemWidget = new QWidget;
            itemWidget->setProperty("paletteIndex", i);
            itemWidget->setMouseTracking(true);
            itemWidget->installEventFilter(hoverFilter);
            QHBoxLayout *itemLayout = new QHBoxLayout(itemWidget);
            itemLayout->setContentsMargins(4, 2, 4, 2);
            itemLayout->setSpacing(6);

            QLabel *pixLabel = new QLabel;
            pixLabel->setPixmap(pix.scaled(128, 10, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            pixLabel->setFixedSize(128, 10);
            itemLayout->addWidget(pixLabel);

            QLabel *numLabel = new QLabel(QString::number(i));
            numLabel->setFixedWidth(28);
            numLabel->setAlignment(Qt::AlignCenter);
            numLabel->setStyleSheet("border: 1px solid #AAAAAA; border-radius: 3px; padding: 2px;");
            itemLayout->addWidget(numLabel);
            (*numLabels)[i] = numLabel;

            wa->setDefaultWidget(itemWidget);
            paletteMenu->addAction(wa);
        }

        // 菜单打开时刷新高亮
        connect(paletteMenu, &QMenu::aboutToShow, this, [numLabels, this]() {
            for (int j = 1; j <= 30; ++j) {
                if ((*numLabels)[j]) {
                    (*numLabels)[j]->setStyleSheet(
                        j == m_paletteIndex
                            ? "background: #4A90D9; color: white; border: 1px solid #3A7BD5; border-radius: 3px; padding: 2px;"
                            : "border: 1px solid #AAAAAA; border-radius: 3px; padding: 2px;");
                }
            }
        });

        // 菜单关闭时恢复为当前已确认的palette
        connect(paletteMenu, &QMenu::aboutToHide, this, [this]() {
            loadLUT(m_paletteIndex);
            refreshCxBarThumbnails();
            if (m_currentTab) refreshImage();
        });

        paletteBtn->setMenu(paletteMenu);
        colorRows->addWidget(paletteBtn);
    }

    // 线性变换表(颜色变换): 现有 20 变换悬停预览菜单逻辑原样保留
    {
        QToolButton *colorXformBtn = comboRowBtn(QStringLiteral("tune"), QString::fromUtf8("线性变换表"));
        QMenu *cxMenu = new QMenu(colorXformBtn);

        // 悬停预览事件过滤器(与调色板风格一致); press 同时提交 confirmed(修复关闭菜单回滚旧值的悬垂捕获bug)
        class CxHoverFilter : public QObject {
            int &m_cxIndex;
            int &m_confirmed;
            std::function<void(int)> m_onPreview;
        public:
            CxHoverFilter(int &cxIdx, int &confirmed, std::function<void(int)> onPreview, QObject *parent = nullptr)
                : QObject(parent), m_cxIndex(cxIdx), m_confirmed(confirmed), m_onPreview(std::move(onPreview)) {}
            bool eventFilter(QObject *watched, QEvent *event) override {
                QWidget *w = qobject_cast<QWidget*>(watched);
                if (!w) return QObject::eventFilter(watched, event);
                int idx = w->property("cxIndex").toInt();
                if (event->type() == QEvent::Enter) {
                    w->setStyleSheet("QWidget { background: #E8E8E8; border-radius: 2px; }");
                    if (m_onPreview) m_onPreview(idx);
                } else if (event->type() == QEvent::Leave) {
                    w->setStyleSheet("QWidget { background: transparent; border: none; }");
                } else if (event->type() == QEvent::MouseButtonPress) {
                    m_cxIndex = idx;
                    m_confirmed = idx;   // 点击即确认
                }
                return QObject::eventFilter(watched, event);
            }
        };

        int *confirmedCx = new int(m_colorTransformIndex);   // 堆上共享: 已确认索引(悬停预览不覆盖)
        CxHoverFilter *cxHover = new CxHoverFilter(m_colorTransformIndex, *confirmedCx,
            [this](int idx) {
                m_colorTransformIndex = idx;
                if (m_currentTab) refreshImage();
            }, cxMenu);

        // 20 种变换(渐变条预览 + 编号); 预览条颜色 = 当前调色板∘变换表, 由 refreshCxBarThumbnails 统一重绘
        for (int i = 1; i <= 20; ++i) {
            QWidgetAction *wa = new QWidgetAction(cxMenu);
            QWidget *itemWidget = new QWidget;
            itemWidget->setProperty("cxIndex", i);
            itemWidget->setMouseTracking(true);
            itemWidget->installEventFilter(cxHover);
            QHBoxLayout *il = new QHBoxLayout(itemWidget);
            il->setContentsMargins(4, 2, 4, 2);
            il->setSpacing(8);

            QLabel *bar = new QLabel;
            bar->setFixedSize(128, 14);
            m_cxBarLabels.append(bar);
            il->addWidget(bar);

            QLabel *name = new QLabel(QString("%1").arg(i, 2, 10, QChar(' ')));
            name->setFixedWidth(28);
            name->setAlignment(Qt::AlignCenter);
            il->addWidget(name);

            wa->setDefaultWidget(itemWidget);
            cxMenu->addAction(wa);
        }

        // 菜单关闭时恢复为已确认的索引(悬停预览不落地)
        connect(cxMenu, &QMenu::aboutToHide, this, [this, confirmedCx]() {
            m_colorTransformIndex = *confirmedCx;
            if (m_currentTab) refreshImage();
        });
        // 每次打开菜单前同步已确认索引
        connect(cxMenu, &QMenu::aboutToShow, this, [this, confirmedCx]() {
            *confirmedCx = m_colorTransformIndex;
        });

        colorXformBtn->setMenu(cxMenu);
        colorRows->addWidget(colorXformBtn);

        refreshCxBarThumbnails();   // 初始绘制(当前调色板合成)
    }

    // ===== 组4: 数据信息 (文件头, 末组无分隔线; active 态 bg#d9e3f6+边框#c3c6d6) =====
    QHBoxLayout *infoRow = addRibbonGroup(startLayout, QString::fromUtf8("数据信息"), true);
    QToolButton *btnHeader = new QToolButton();
    if (MatIcon::ready())
        btnHeader->setIcon(MatIcon::icon(QStringLiteral("info"), cPrimary, QColor(), cDark, 24, 1.0));  // FILL=1 实心
    btnHeader->setText(QString::fromUtf8("文件头"));
    btnHeader->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btnHeader->setIconSize(QSize(24, 24));
    btnHeader->setMinimumSize(80, 52);
    btnHeader->setCheckable(true);
    btnHeader->setCursor(Qt::PointingHandCursor);
    btnHeader->setStyleSheet(
        "QToolButton { border: 1px solid transparent; border-radius: 2px; background: transparent;"
        " font-size: 12px; font-weight: bold; color: #121c2a; padding: 2px; }"
        "QToolButton:hover { background: #dee9fc; }"
        "QToolButton:pressed { background: #c9d8f0; }"
        "QToolButton:checked { background: #d9e3f6; border: 1px solid #c3c6d6; }");
    infoRow->addWidget(btnHeader);
    connect(btnHeader, &QToolButton::clicked, this, &MainWindow::showFileHeader);
    m_btnHeaderToggle = btnHeader;   // 与右侧文件头栏开合联动(Step4)

    // (v1.0.87 删除"简易处理"与"其他"组: 处理功能全部在"数据处理"标签,
    //  关于/升级移至顶栏右上角齿轮菜单 — 严格按 主页-文件头.png 只保留 4 组)

    startLayout->addStretch();
    ribbonTab->addTab(startPage, QString::fromUtf8("主页"));

    // --- Tab: 编辑 (v1.0.98 标记与数据块 + 视口缩放, 按 编辑-编辑标记/编辑数据块.html) ---
    {
        QWidget *editPage = new QWidget();
        QHBoxLayout *editLayout = new QHBoxLayout(editPage);
        editLayout->setContentsMargins(8, 8, 8, 4);
        editLayout->setSpacing(0);

        // 组1: 标记与数据块
        QHBoxLayout *editRow = addRibbonGroup(editLayout, QString::fromUtf8("标记与数据块"));
        m_btnEditMarker = displayBtn(QStringLiteral("bookmark"), QString::fromUtf8("编辑标记"), 76);
        m_btnEditBlock = displayBtn(QStringLiteral("grid_view"), QString::fromUtf8("编辑数据块"), 88);
        editRow->addWidget(m_btnEditMarker);
        editRow->addWidget(m_btnEditBlock);

        // 组2: 视口缩放(末组无分隔线)
        QHBoxLayout *zoomRow = addRibbonGroup(editLayout, QString::fromUtf8("视口缩放"), true);
        m_btnHZoom = displayBtn(QStringLiteral("zoom_in_map"), QString::fromUtf8("横向缩放"), 88);
        zoomRow->addWidget(m_btnHZoom);

        editLayout->addStretch(1);   // 关键: 剩余宽度给尾部空白(此前两组被撑开,按钮间距过大)

        // 三按钮联动(v1.0.102 规则): 标记/数据块不可取消、必居其一(默认标记);
        // 缩放可自由勾选/取消; 数据块选中时右栏恒显属性面板(无视缩放勾选)
        connect(m_btnEditMarker, &QToolButton::clicked, this, [this](bool on) {
            if (!on) { m_btnEditMarker->setChecked(true); return; }   // 不可取消
            if (!requireOpenFile()) { m_btnEditMarker->setChecked(false); return; }
            if (m_btnEditBlock && m_btnEditBlock->isChecked())
                m_btnEditBlock->setChecked(false);                    // 与数据块互斥
            syncEditUiState();
        });
        connect(m_btnEditBlock, &QToolButton::clicked, this, [this](bool on) {
            if (!on) { m_btnEditBlock->setChecked(true); return; }    // 不可取消
            if (!requireOpenFile()) { m_btnEditBlock->setChecked(false); return; }
            if (m_btnEditMarker && m_btnEditMarker->isChecked())
                m_btnEditMarker->setChecked(false);                   // 与标记互斥
            syncEditUiState();
        });
        connect(m_btnHZoom, &QToolButton::clicked, this, [this](bool on) {
            Q_UNUSED(on);   // 自由勾选/取消(无文件时不允许选中)
            if (m_btnHZoom->isChecked() && !requireOpenFile())
                m_btnHZoom->setChecked(false);
            syncEditUiState();
        });

        ribbonTab->addTab(editPage, QString::fromUtf8("编辑"));
    }

    // --- Tab: 数据处理 ---
    QWidget *dataPage = new QWidget();
    QHBoxLayout *dataLayout = new QHBoxLayout(dataPage);
    dataLayout->setContentsMargins(4, 2, 4, 2);
    dataLayout->setSpacing(8);

    // Text-only button maker (no icons yet)
    auto makeTextBtn = [](const QString &text) -> QToolButton* {
        QToolButton *btn = new QToolButton();
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFixedSize(56, 50);
        btn->setStyleSheet(
            "QToolButton { border: none; border-radius: 3px; background: transparent; font-size: 11px; }"
            "QToolButton:hover { background: #dce7f5; }"
            "QToolButton:pressed { background: #b8d0ea; }"
        );
        return btn;
    };

    // Group 1: 零点调节
    QVBoxLayout *g1 = addGroup(dataLayout, "零点调节");
    QHBoxLayout *g1btns = qobject_cast<QHBoxLayout*>(g1->itemAt(0)->layout());
    QToolButton *btnAdjZero2 = makeTextBtn("调节零点");
    connect(btnAdjZero2, &QToolButton::clicked, this, [this]() {
        if (!requireOpenFile()) return;
        m_leftStack->setCurrentWidget(m_zeroPage);
        m_leftPanel->show();
        syncAscanVisibility();   // 零点编辑需要 A-SCAN 波形可见
        if (chartView) {
            chartView->setGainVisible(false);
            chartView->setYScale(20.0f / 511.0f);
            float zeroOff = m_zeroRangePctSpin ? -m_zeroRangePctSpin->value() * 0.2f : 0.0f;
            chartView->setZeroOffset(zeroOff);
            updateChart(m_lastChartX);
        }
    });
    g1btns->addWidget(btnAdjZero2);
    g1btns->addWidget(makeTextBtn("寻找地面"));
    g1btns->addWidget(makeTextBtn("校平地面"));

    // Group 2: 滤波
    QVBoxLayout *g2 = addGroup(dataLayout, "滤波");
    QHBoxLayout *g2row1 = qobject_cast<QHBoxLayout*>(g2->itemAt(0)->layout());
    QToolButton *btnAdjGain = makeTextBtn("调节增益");
    connect(btnAdjGain, &QToolButton::clicked, this, [this]() {
        if (!requireOpenFile()) return;
        m_leftStack->setCurrentWidget(m_gainPage);
        if (m_currentTab) {
            QFileInfo fi(m_currentTab->filePath);
            m_leftPanel->setWindowTitle(QString("增益-%1").arg(fi.completeBaseName()));
        }
        m_leftPanel->setWindowIcon(QIcon(":/icons/resources/adjustgain.png"));
        m_leftPanel->setVisible(!m_leftPanel->isVisible());
        syncAscanVisibility();   // 增益编辑需要 A-SCAN 波形可见
        if (m_leftPanel->isVisible() && chartView) {
            resetGainPanel();   // 打开时重置增益为默认,清除上次残留值
            chartView->setGainVisible(true);
            chartView->setYScale(1.0f);
            QValueAxis *axisY = qobject_cast<QValueAxis*>(chartView->chart()->axisY(chartSeries));
            if (axisY) {
                axisY->setRange(0, m_pixelsPerRow - 1);
                axisY->setLabelFormat("%d");
            }
            updateChart(m_lastChartX);
        }
    });
    g2row1->addWidget(btnAdjGain);
    QToolButton *btnCorrectOffset = makeTextBtn("校正零偏");
    connect(btnCorrectOffset, &QToolButton::clicked, this, &MainWindow::showCorrectOffset);
    g2row1->addWidget(btnCorrectOffset);
    QToolButton *btnBgRemove = makeTextBtn("背景消除");
    connect(btnBgRemove, &QToolButton::clicked, this, &MainWindow::showBackgroundRemoval);
    g2row1->addWidget(btnBgRemove);
    QHBoxLayout *g2row2 = new QHBoxLayout();
    g2row2->setSpacing(2);
    QToolButton *btnDigFilter = makeTextBtn("数字滤波");
    g2row2->addWidget(btnDigFilter);
    connect(btnDigFilter, &QToolButton::clicked, this, &MainWindow::showDigitalFilter);
    QToolButton *btnMovingAvg = makeTextBtn("滑动平均");
    connect(btnMovingAvg, &QToolButton::clicked, this, &MainWindow::showMovingAverage);
    g2row2->addWidget(btnMovingAvg);
    QToolButton *btnTraceEqual = makeTextBtn("道间均衡");
    connect(btnTraceEqual, &QToolButton::clicked, this, &MainWindow::showTraceEqualization);
    g2row2->addWidget(btnTraceEqual);
    g2->insertLayout(1, g2row2);

    // Group 3: 其他处理
    QVBoxLayout *g3 = addGroup(dataLayout, "其他处理");
    QHBoxLayout *g3row1 = qobject_cast<QHBoxLayout*>(g3->itemAt(0)->layout());
    QToolButton *btnMathOp = makeTextBtn("数学运算");
    connect(btnMathOp, &QToolButton::clicked, this, &MainWindow::showMathOperation);
    g3row1->addWidget(btnMathOp);
    QToolButton *btnDeconv = makeTextBtn("反褶积");
    connect(btnDeconv, &QToolButton::clicked, this, &MainWindow::showDeconvolution);
    g3row1->addWidget(btnDeconv);
    QToolButton *btnHilbert = makeTextBtn("希尔伯特");
    connect(btnHilbert, &QToolButton::clicked, this, &MainWindow::showHilbertTransform);
    g3row1->addWidget(btnHilbert);
    QHBoxLayout *g3row2 = new QHBoxLayout();
    g3row2->setSpacing(2);
    QToolButton *btnKirchhoff = makeTextBtn("克西霍夫");
    connect(btnKirchhoff, &QToolButton::clicked, this, &MainWindow::showKirchhoffMigration);
    g3row2->addWidget(btnKirchhoff);
    QToolButton *btnOneClickData = makeTextBtn("一键处理");
    connect(btnOneClickData, &QToolButton::clicked, this, &MainWindow::showOneClickProcess);
    g3row2->addWidget(btnOneClickData);
    g3row2->addWidget(makeTextBtn("批处理"));
    g3->insertLayout(1, g3row2);

    // Group 4: 处理范围 (labels + spinboxes)
    QFrame *rangeFrame = new QFrame();
    rangeFrame->setFrameShape(QFrame::StyledPanel);
    rangeFrame->setFrameShadow(QFrame::Plain);
    rangeFrame->setStyleSheet("QFrame { border: 1px solid #d0d0d0; border-radius: 3px; background: #fafafa; }");
    QVBoxLayout *rangeLayout = new QVBoxLayout(rangeFrame);
    rangeLayout->setContentsMargins(4, 2, 4, 2);
    rangeLayout->setSpacing(4);

    QLabel *rangeLabel = new QLabel("处理范围");
    rangeLabel->setAlignment(Qt::AlignCenter);
    rangeLabel->setStyleSheet("color: #666; font-size: 10px; border: none;");

    // Row 1: 起始道 / 终止道
    QHBoxLayout *rangeRow1 = new QHBoxLayout();
    rangeRow1->setSpacing(4);
    rangeRow1->addWidget(new QLabel("起始道"));
    m_startTraceSpin = new QSpinBox();
    m_startTraceSpin->setRange(0, 0);
    m_startTraceSpin->setValue(0);
    m_startTraceSpin->setFixedWidth(70);
    rangeRow1->addWidget(m_startTraceSpin);
    rangeRow1->addWidget(new QLabel("终止道"));
    m_endTraceSpin = new QSpinBox();
    m_endTraceSpin->setRange(0, 0);
    m_endTraceSpin->setValue(0);
    m_endTraceSpin->setFixedWidth(70);
    rangeRow1->addWidget(m_endTraceSpin);

    // Row 2: 起始点 / 终止点
    QHBoxLayout *rangeRow2 = new QHBoxLayout();
    rangeRow2->setSpacing(4);
    rangeRow2->addWidget(new QLabel("起始点"));
    QSpinBox *startPointSpin = new QSpinBox();
    startPointSpin->setRange(0, 511);
    startPointSpin->setValue(0);
    startPointSpin->setFixedWidth(70);
    rangeRow2->addWidget(startPointSpin);
    rangeRow2->addWidget(new QLabel("终止点"));
    QSpinBox *endPointSpin = new QSpinBox();
    endPointSpin->setRange(0, 511);
    endPointSpin->setValue(511);
    endPointSpin->setFixedWidth(70);
    rangeRow2->addWidget(endPointSpin);

    rangeLayout->addSpacing(15);
    rangeLayout->addLayout(rangeRow1);
    rangeLayout->addLayout(rangeRow2);
    rangeLayout->addWidget(rangeLabel);
    dataLayout->addWidget(rangeFrame);

    dataLayout->addStretch();
    ribbonTab->addTab(dataPage, QString::fromUtf8("数据处理"));

    // --- Tab: 数据解译 (v1.0.108 按 数据解译-追踪异常.html: 追踪/标注/导出 三组) ---
    {
        QWidget *interpPage = new QWidget();
        QHBoxLayout *interpLayout = new QHBoxLayout(interpPage);
        interpLayout->setContentsMargins(8, 8, 8, 4);
        interpLayout->setSpacing(0);

        // 组1: 层位/目标追踪 (自动/手动 互斥)
        QHBoxLayout *trackRow = addRibbonGroup(interpLayout, QString::fromUtf8("层位/目标追踪"));
        m_btnAutoTrack = displayBtn(QStringLiteral("magic_button"), QString::fromUtf8("自动追踪"), 88);
        m_btnManualTrack = displayBtn(QStringLiteral("edit"), QString::fromUtf8("手动追踪"), 88);
        trackRow->addWidget(m_btnAutoTrack);
        trackRow->addWidget(m_btnManualTrack);
        m_trackGroup = new QButtonGroup(interpPage);
        m_trackGroup->setExclusive(true);
        m_trackGroup->addButton(m_btnAutoTrack);
        m_trackGroup->addButton(m_btnManualTrack);

        // 组2: 异常标注工具 (圆/矩/多边形/文本 四选一互斥)
        QHBoxLayout *annoRow = addRibbonGroup(interpLayout, QString::fromUtf8("异常标注工具"));
        m_btnAnoCircle = displayBtn(QStringLiteral("radio_button_unchecked"), QString::fromUtf8("圆形"), 64);
        m_btnAnoRect = displayBtn(QStringLiteral("check_box_outline_blank"), QString::fromUtf8("矩形"), 64);
        m_btnAnoPoly = displayBtn(QStringLiteral("pentagon"), QString::fromUtf8("闭合多边形"), 100);
        m_btnAnoText = displayBtn(QStringLiteral("title"), QString::fromUtf8("文本批注"), 88);
        for (auto *b : { m_btnAnoCircle, m_btnAnoRect, m_btnAnoPoly, m_btnAnoText })
            annoRow->addWidget(b);
        m_annoGroup = new QButtonGroup(interpPage);
        m_annoGroup->setExclusive(true);
        for (auto *b : { m_btnAnoCircle, m_btnAnoRect, m_btnAnoPoly, m_btnAnoText })
            m_annoGroup->addButton(b);
        m_annoGroup->setId(m_btnAnoCircle, 0);
        m_annoGroup->setId(m_btnAnoRect, 1);
        m_annoGroup->setId(m_btnAnoPoly, 2);
        m_annoGroup->setId(m_btnAnoText, 3);
        // v1.0.129: 形状按钮 — 仅编辑状态或新项(shape=-1)时可用; 选中≠编辑
        auto shapeBtnHandler = [this](int shapeId) {
            if (!m_currentTab) return;
            if (m_selectedAnomaly >= 0
                && m_selectedAnomaly < m_currentTab->anomalies.size()) {
                auto &a = m_currentTab->anomalies[m_selectedAnomaly];
                if (a.editing || a.shape < 0) {
                    anomalySetShape(m_selectedAnomaly, shapeId);
                }
            }
        };
        connect(m_btnAnoCircle, &QToolButton::clicked, this,
                [shapeBtnHandler]() { shapeBtnHandler(0); });
        connect(m_btnAnoRect, &QToolButton::clicked, this,
                [shapeBtnHandler]() { shapeBtnHandler(1); });
        connect(m_btnAnoPoly, &QToolButton::clicked, this,
                [shapeBtnHandler]() { shapeBtnHandler(2); });
        connect(m_btnAnoText, &QToolButton::clicked, this,
                [shapeBtnHandler]() { shapeBtnHandler(3); });

        // 组3: 解译成果导出 (占位)
        QHBoxLayout *expRow = addRibbonGroup(interpLayout, QString::fromUtf8("解译成果导出"), true);
        QToolButton *btnExpData = displayBtn(QStringLiteral("download"), QString::fromUtf8("数据导出"), 88);
        QToolButton *btnExpImg = displayBtn(QStringLiteral("image"), QString::fromUtf8("图像导出"), 88);
        btnExpData->setCheckable(false);
        btnExpImg->setCheckable(false);
        expRow->addWidget(btnExpData);
        expRow->addWidget(btnExpImg);
        connect(btnExpData, &QToolButton::clicked, this, [this]() {
            QMessageBox::information(this, QString::fromUtf8("数据导出"),
                QString::fromUtf8("解译成果数据导出将在后续版本提供。"));
        });
        connect(btnExpImg, &QToolButton::clicked, this, [this]() {
            QMessageBox::information(this, QString::fromUtf8("图像导出"),
                QString::fromUtf8("解译成果图像导出将在后续版本提供。"));
        });

        interpLayout->addStretch(1);
        ribbonTab->addTab(interpPage, QString::fromUtf8("数据解译"));
    }

    // --- Tab: AI分析(占位) ---
    {
        QWidget *aiPage = new QWidget();
        ribbonTab->addTab(aiPage, QString::fromUtf8("AI分析"));
    }

    qobject_cast<QVBoxLayout*>(centralWidget()->layout())->insertWidget(0, ribbonTab);
}

// ==================== AI 识别 (YOLOv8 classification) ====================

void MainWindow::showAIRecognition()
{
    if (!m_currentTab) {
        QMessageBox::warning(this, QString::fromUtf8("AI识别"),
                             QString::fromUtf8("请先打开 DZT 文件"));
        return;
    }

    // 懒加载 ONNX
    if (!m_yoloNetLoaded) {
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList candidates = {
            appDir + "/AI/yolo_gpr_cls.onnx",                       // 部署后
            appDir + "/../AI/yolo_gpr_cls.onnx",                    // build 目录 dev
            QDir::currentPath() + "/AI/yolo_gpr_cls.onnx",
            "D:/gpr_software/AI/yolo_gpr_cls.onnx"                  // 兜底绝对路径
        };
        QString onnxPath;
        for (const QString &p : candidates) {
            if (QFile::exists(p)) { onnxPath = p; break; }
        }
        if (onnxPath.isEmpty()) {
            QMessageBox::critical(this, QString::fromUtf8("AI识别"),
                                  QString::fromUtf8("未找到 ONNX 模型文件 yolo_gpr_cls.onnx\n"
                                                    "请确认 AI/ 目录下存在该文件"));
            return;
        }
        try {
            // 用 QFile 读 ONNX 到内存再交给 OpenCV,避免中文安装路径导致 toStdString() 编码丢失
            QFile onnxFile(onnxPath);
            if (!onnxFile.open(QIODevice::ReadOnly)) {
                QMessageBox::critical(this, QString::fromUtf8("AI识别"),
                                      QString::fromUtf8("无法读取 ONNX 模型文件:\n%1").arg(onnxPath));
                return;
            }
            QByteArray onnxData = onnxFile.readAll();
            onnxFile.close();
            m_yoloNet = cv::dnn::readNetFromONNX(onnxData.constData(), onnxData.size());
            m_yoloNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            m_yoloNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            cv::setNumThreads(cv::getNumberOfCPUs());   // 确保用满所有 CPU 核
            m_yoloNetLoaded = true;
        } catch (const cv::Exception &e) {
            QMessageBox::critical(this, QString::fromUtf8("AI识别"),
                                  QString::fromUtf8("加载 ONNX 失败:\n%1\n\n路径: %2").arg(QString::fromStdString(e.what()), onnxPath));
            return;
        }
    }

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QString::fromUtf8("AI识别: 构建图像..."));
    m_progressBar->show();
    QCoreApplication::processEvents();

    cv::Mat full;
    buildRadarCVMat(full);
    m_progressBar->setValue(10);
    QCoreApplication::processEvents();

    QList<cv::Rect> rects;
    sliceAndSaveCrops(full, rects);
    m_progressBar->setValue(30);
    QCoreApplication::processEvents();

    QList<int> top1Ids;
    QList<float> confidences;
    runInference(full, rects, top1Ids, confidences);

    m_progressBar->setValue(95);
    m_progressBar->setFormat(QString::fromUtf8("AI识别: 后处理..."));
    QCoreApplication::processEvents();

    // === 跨类别 NMS + 每类前10: 过滤出最终结果 ===
    auto iou = [](const cv::Rect &a, const cv::Rect &b) -> double {
        int x1 = qMax(a.x, b.x), y1 = qMax(a.y, b.y);
        int x2 = qMin(a.x + a.width, b.x + b.width), y2 = qMin(a.y + a.height, b.y + b.height);
        if (x2 <= x1 || y2 <= y1) return 0.0;
        double inter = (x2 - x1) * (y2 - y1);
        double uni = a.area() + b.area() - inter;
        return uni > 0 ? inter / uni : 0.0;
    };
    // 所有有效框按置信度降序
    QList<int> allIdxs;
    for (int i = 0; i < rects.size(); ++i)
        if (top1Ids[i] >= 0 && top1Ids[i] < 3) allIdxs.append(i);
    std::sort(allIdxs.begin(), allIdxs.end(), [&](int a, int b) {
        return confidences[a] > confidences[b];
    });
    // 跨类别 NMS: 不分种类,重叠>30%的抑制低分者
    QSet<int> suppressed;
    QList<int> survived;
    for (int i = 0; i < allIdxs.size(); ++i) {
        if (suppressed.contains(allIdxs[i])) continue;
        survived.append(allIdxs[i]);
        for (int j = i + 1; j < allIdxs.size(); ++j) {
            if (!suppressed.contains(allIdxs[j]) && iou(rects[allIdxs[i]], rects[allIdxs[j]]) > 0.15)
                suppressed.insert(allIdxs[j]);
        }
    }
    // 每类从幸存者中取前10
    QHash<int, QList<int>> byClass;
    for (int idx : survived)
        byClass[top1Ids[idx]].append(idx);
    QList<int> finalIdxs;
    for (int cid = 0; cid < 3; ++cid) {
        if (byClass.contains(cid))
            finalIdxs.append(byClass[cid].mid(0, 20));
    }
    // 构建过滤后的列表
    QList<cv::Rect> fRects;
    QList<int> fIds;
    QList<float> fConfs;
    for (int idx : finalIdxs) {
        fRects.append(rects[idx]);
        fIds.append(top1Ids[idx]);
        fConfs.append(confidences[idx]);
    }

    cv::Mat annotated;
    drawResultOverlay(full, fRects, fIds, fConfs, annotated);

    m_progressBar->setValue(100);
    m_progressBar->setFormat(QString::fromUtf8("AI识别: 完成"));
    QCoreApplication::processEvents();

    showAIResultDialog(annotated, fRects, fIds, fConfs);

    // 1 秒后重置进度条
    QTimer::singleShot(1000, this, [this]() {
        m_progressBar->setValue(0);
        m_progressBar->setFormat("");
    });
}

void MainWindow::buildRadarCVMat(cv::Mat &out)
{
    int traces = m_currentTab->traceCount;          // 宽
    int samples = m_pixelsPerRow;                   // 512 高
    out = cv::Mat::zeros(samples, traces, CV_8UC3); // OpenCV: Mat(rows=高, cols=宽)
    const char *data = m_rawData.constData();
    int dataSize = m_rawData.size();
for (int x = 0; x < samples; ++x) {
        cv::Vec3b *rowPtr = out.ptr<cv::Vec3b>(x);
        for (int y = 0; y < traces; ++y) {
            int dataIdx = (y * m_pixelsPerRow + x) * 4;
            if (dataIdx + 4 > dataSize) break;
            qint32 val = (static_cast<quint8>(data[dataIdx+3]) << 24) |
                         (static_cast<quint8>(data[dataIdx+2]) << 16) |
                         (static_cast<quint8>(data[dataIdx+1]) << 8) |
                         static_cast<quint8>(data[dataIdx]);
            int lutIdx = val / 65536 + 128;
            if (lutIdx < 0) lutIdx = 0;
            else if (lutIdx > 255) lutIdx = 255;
            QRgb rgb = m_lut[lutIdx];
            rowPtr[y] = cv::Vec3b(static_cast<uchar>(qBlue(rgb)),
                                  static_cast<uchar>(qGreen(rgb)),
                                  static_cast<uchar>(qRed(rgb)));
        }
    }
}

void MainWindow::sliceAndSaveCrops(const cv::Mat &full, QList<cv::Rect> &rects)
{
    int W = full.cols;
    int H = full.rows;
    const int WIN = 64;
    const int STRIDE = 32;

    for (int y = 0; y <= H - WIN; y += STRIDE) {
        for (int x = 0; x <= W - WIN; x += STRIDE) {
            rects.append(cv::Rect(x, y, WIN, WIN));
        }
    }
}

void MainWindow::runInference(const cv::Mat &full, const QList<cv::Rect> &rects, QList<int> &top1Ids, QList<float> &confidences)
{
    int N = rects.size();
    const int BATCH = 64;

    // 预过滤:跳过低信息窗口(标准差 < 5 的均匀背景直接标 intact)
    QList<int> inferIdxs;  // 需要推理的窗口索引
    for (int i = 0; i < N; ++i) {
        cv::Mat mean, stdDev;
        cv::meanStdDev(full(rects[i]), mean, stdDev);
        if (stdDev.at<double>(0) < 3.0) {
            top1Ids.append(1);   // intact
            confidences.append(0.5f);
        } else {
            top1Ids.append(-1);  // 占位,待推理后回填
            confidences.append(0.0f);
            inferIdxs.append(i);
        }
    }
    int M = inferIdxs.size();

    // 批处理推理
    for (int bi = 0; bi < M; bi += BATCH) {
        int curBatch = qMin(BATCH, M - bi);
        std::vector<cv::Mat> batchCrops;
        batchCrops.reserve(curBatch);
        for (int j = 0; j < curBatch; ++j)
            batchCrops.push_back(full(rects[inferIdxs[bi + j]]));

        cv::Mat batchBlob = cv::dnn::blobFromImages(batchCrops, 1.0 / 255.0, cv::Size(224, 224),
                                                     cv::Scalar(0, 0, 0),
                                                     /*swapRB=*/true, /*crop=*/false, CV_32F);
        m_yoloNet.setInput(batchBlob);
        cv::Mat out = m_yoloNet.forward();

        cv::Mat probs2d = out.reshape(1, curBatch);
        for (int j = 0; j < curBatch; ++j) {
            int origIdx = inferIdxs[bi + j];
            cv::Mat row = probs2d.row(j);
            double maxVal;
            cv::Point maxLoc;
            cv::minMaxLoc(row, nullptr, &maxVal, nullptr, &maxLoc);
            top1Ids[origIdx] = maxLoc.x;
            confidences[origIdx] = static_cast<float>(maxVal);  // 模型已输出 softmax 概率,直接用最大值
        }

        int done = qMin(bi + curBatch, M);
        m_progressBar->setValue(30 + 60 * done / qMax(1, M));
        m_progressBar->setFormat(QString::fromUtf8("AI识别: 推理 %1/%2 (跳过%3背景)").arg(done).arg(M).arg(N - M));
        QCoreApplication::processEvents();
    }
}

void MainWindow::drawResultOverlay(const cv::Mat &full, const QList<cv::Rect> &rects,
                                   const QList<int> &top1Ids, const QList<float> &confidences, cv::Mat &out)
{
    full.copyTo(out);
    cv::Scalar colors[3] = {{0, 0, 255}, {255, 0, 0}, {0, 255, 255}};
    int n = qMin(qMin(rects.size(), top1Ids.size()), confidences.size());
    const int HALF = 12;  // 画 24×24 小框,精确定位单个目标

    for (int i = 0; i < n; ++i) {
        int cid = top1Ids[i];
        if (cid < 0 || cid >= 3) continue;
        const cv::Rect &r = rects[i];
        // 找双曲线顶点(最浅高能量点):窗口内从上往下扫,找第一个能量峰值行
        cv::Mat roi = full(r);
        cv::Mat gray, energy;
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        cv::Mat kernel = cv::Mat::ones(16, 16, CV_32F) / 256.0;
        cv::filter2D(gray, energy, CV_32F, kernel);
        // 计算全局能量阈值(高能量的 60% 分位)
        double emin, emax;
        cv::minMaxLoc(energy, &emin, &emax);
        double thresh = emin + (emax - emin) * 0.6;
        // 从上往下找第一个高能量行 → 顶点 Y
        int apexY = gray.rows / 2;  // 默认中心
        for (int yy = 8; yy < gray.rows - 8; ++yy) {
            cv::Mat row = energy.row(yy);
            double rowMax;
            cv::minMaxLoc(row, nullptr, &rowMax);
            if (rowMax > thresh) { apexY = yy; break; }
        }
        // 在顶点行附近找最强 X → 顶点 X
        cv::Mat apexBand = energy(cv::Rect(0, qMax(0, apexY - 4), energy.cols, qMin(8, energy.rows - qMax(0, apexY - 4))));
        cv::Mat bandMax;
        cv::reduce(apexBand, bandMax, 0, cv::REDUCE_MAX);
        cv::Point bx;
        cv::minMaxLoc(bandMax, nullptr, nullptr, nullptr, &bx);
        cv::Point ctr(r.x + bx.x, r.y + apexY);  // 精确定位到双曲线顶点
        cv::Rect small(ctr.x - HALF, ctr.y - HALF, HALF * 2, HALF * 2);
        cv::rectangle(out, small, colors[cid], 2);
        QString label = QString("%1 %2%").arg(m_yoloClasses[cid])
                        .arg(static_cast<int>(confidences[i] * 100));
        cv::putText(out, label.toStdString(), small.tl() + cv::Point(3, 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, colors[cid], 1);
    }
}

void MainWindow::showAIResultDialog(const cv::Mat &annotated, const QList<cv::Rect> &rects,
                                    const QList<int> &top1Ids, const QList<float> &confidences)
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(QString::fromUtf8("AI识别 - %1")
                        .arg(QFileInfo(m_currentTab->filePath).completeBaseName()));
    QVBoxLayout *lay = new QVBoxLayout(dlg);

    // 统计 + 颜色图例（三类全显示）
    QHash<int, int> counts;
    QHash<int, float> maxConf;
    for (int i = 0; i < top1Ids.size(); ++i) {
        int id = top1Ids[i];
        if (id < 0) continue;
        counts[id]++;
        if (confidences[i] > maxConf.value(id, 0))
            maxConf[id] = confidences[i];
    }
    // 颜色图例：cavities=红, intact=蓝, utilities=黄
    QString cssColor[3] = {"red", "blue", "#c8a000"};
    QString summary = QString::fromUtf8("识别统计 (共 %1 窗口):   ").arg(top1Ids.size());
    for (int i = 0; i < 3; ++i) {
        summary += QString::fromUtf8("<span style='color:%2'>■</span> %1=%3(最高%4%)   ")
                   .arg(m_yoloClasses[i]).arg(cssColor[i])
                   .arg(counts.value(i, 0))
                   .arg(static_cast<int>(maxConf.value(i, 0) * 100));
    }
    summary += QString::fromUtf8("<span style='color:#888'>（每类仅显示置信度最高的前10个框）</span>");
    QLabel *summaryLabel = new QLabel(summary);
    summaryLabel->setStyleSheet("font-size: 13px; padding: 6px;");
    summaryLabel->setTextFormat(Qt::RichText);
    lay->addWidget(summaryLabel);

    // 操作提示
    QLabel *hintLabel = new QLabel(QString::fromUtf8("Ctrl+滚轮缩放 | 普通滚轮上下滚动 | Shift+滚轮左右滚动"));
    hintLabel->setStyleSheet("color: #666; font-size: 11px; padding: 2px 6px;");
    lay->addWidget(hintLabel);

    // cv::Mat BGR → QImage RGB
    cv::Mat rgb;
    cv::cvtColor(annotated, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    QImage qimgCopy = qimg.copy();

    ZoomableImageView *imgView = new ZoomableImageView(QPixmap::fromImage(qimgCopy));

    // 右侧文本面板：每类 top-10 的位置(道/采样)和置信度
    QTextEdit *detailPanel = new QTextEdit;
    detailPanel->setReadOnly(true);
    detailPanel->setStyleSheet("font-family: Consolas, monospace; font-size: 12px;");
    detailPanel->setMaximumWidth(360);

    QString colorNameZh[3] = {QString::fromUtf8("红色"), QString::fromUtf8("蓝色"), QString::fromUtf8("黄色")};
    QString html;
    int n = qMin(qMin(rects.size(), top1Ids.size()), confidences.size());
    for (int cid = 0; cid < 3; ++cid) {
        // 收集该类索引并按置信度降序
        QList<int> idxs;
        for (int i = 0; i < n; ++i) {
            if (top1Ids[i] == cid) idxs.append(i);
        }
        std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
            return confidences[a] > confidences[b];
        });
        html += QString::fromUtf8("<b><span style='color:%1'>%2框 %3</span></b> (共%4个，显示前%5)<br>")
                .arg(cssColor[cid]).arg(colorNameZh[cid]).arg(m_yoloClasses[cid])
                .arg(counts.value(cid, 0)).arg(qMin(10, idxs.size()));
        html += QString::fromUtf8("<span style='color:#888'>序号  道    采样   置信度</span><br>");
        int showN = qMin(10, idxs.size());
        for (int k = 0; k < showN; ++k) {
            int i = idxs[k];
            const cv::Rect &r = rects[i];
            // 中心点 = trace, sample
            int trace = r.x + r.width / 2;
            int sample = r.y + r.height / 2;
            html += QString::fromUtf8("%1 &nbsp;%2 &nbsp;%3 &nbsp;%4%<br>")
                    .arg(k + 1, 3)
                    .arg(trace, 5)
                    .arg(sample, 5)
                    .arg(static_cast<int>(confidences[i] * 100), 3);
        }
        html += "<br>";
    }
    detailPanel->setHtml(html);

    // 图像 + 文本面板 横向布局
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(imgView);
    splitter->addWidget(detailPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({900, 360});
    lay->addWidget(splitter, 1);

    // 按钮
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    QPushButton *btnSave = new QPushButton(QString::fromUtf8("保存为 JPG"));
    QPushButton *btnReport = new QPushButton(QString::fromUtf8("生成报告"));
    QPushButton *btnClose = new QPushButton(QString::fromUtf8("关闭"));
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnReport);
    btnRow->addWidget(btnClose);
    lay->addLayout(btnRow);

    // 保存：源 DZT 同级 AI/<basename>_ai_result.jpg
    QObject::connect(btnSave, &QPushButton::clicked, [annotated, this]() {
        QFileInfo fi(m_currentTab->filePath);
        QString outDir = fi.absolutePath() + "/AI";
        QDir().mkpath(outDir);
        QString outPath = outDir + "/" + fi.completeBaseName() + "_ai_result.jpg";
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 95};
        if (cv::imwrite(outPath.toStdString(), annotated, params)) {
            QMessageBox::information(this, QString::fromUtf8("AI识别"),
                                     QString::fromUtf8("已保存:\n%1").arg(outPath));
        } else {
            QMessageBox::warning(this, QString::fromUtf8("AI识别"),
                                 QString::fromUtf8("保存失败: %1").arg(outPath));
        }
    });
    QObject::connect(btnReport, &QPushButton::clicked,
                     [annotated, rects, top1Ids, confidences, this]() {
                         generateReport(annotated, rects, top1Ids, confidences);
                     });
    QObject::connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(1280, 720);
    dlg->show();
}

void MainWindow::generateReport(const cv::Mat &annotated, const QList<cv::Rect> &rects,
                                const QList<int> &top1Ids, const QList<float> &confidences)
{
    if (!requireOpenFile()) return;

    QFileInfo fi(m_currentTab->filePath);
    QString reportDir = fi.absolutePath() + "/report";
    QDir().mkpath(reportDir);

    // 找下一个可用的 report_##.pdf 文件名（01-999）
    QString pdfPath;
    for (int i = 1; i <= 999; ++i) {
        QString candidate = QString("%1/report_%2.pdf")
                                .arg(reportDir)
                                .arg(i, 2, 10, QChar('0'));
        if (!QFile::exists(candidate)) {
            pdfPath = candidate;
            break;
        }
    }
    if (pdfPath.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("AI识别"),
                             QString::fromUtf8("无法创建报告文件：已存在 999 份报告"));
        return;
    }

    // 统计
    QHash<int, int> counts;
    QHash<int, float> maxConf;
    int n = qMin(qMin(rects.size(), top1Ids.size()), confidences.size());
    for (int i = 0; i < n; ++i) {
        int id = top1Ids[i];
        if (id < 0) continue;
        counts[id]++;
        if (confidences[i] > maxConf.value(id, 0))
            maxConf[id] = confidences[i];
    }

    QString colorNameZh[3] = {QString::fromUtf8("红色"),
                              QString::fromUtf8("蓝色"),
                              QString::fromUtf8("黄色")};
    QString cssColor[3] = {"red", "blue", "#c8a000"};

    // 构建 HTML
    QString html;
    html += "<html><head><meta charset='utf-8'></head><body>";
    html += QString::fromUtf8("<h1 style='text-align:center; font-size:22pt;'>地质雷达探测报告</h1>");
    html += "<hr>";
    html += QString::fromUtf8("<p style='font-size:11pt;'><b>数据文件：</b>%1</p>").arg(fi.fileName());
    html += QString::fromUtf8("<p style='font-size:11pt;'><b>生成时间：</b>%1</p>")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    html += "<hr>";

    // 分类统计表
    html += QString::fromUtf8("<h2 style='font-size:14pt;'>识别结果统计</h2>");
    html += QString::fromUtf8("<p style='font-size:11pt;'>共识别 %1 个窗口</p>").arg(top1Ids.size());
    html += "<table border='1' cellspacing='0' cellpadding='6' "
            "style='border-collapse:collapse; font-size:11pt;'>";
    html += QString::fromUtf8("<tr><th>类别</th><th>框颜色</th><th>数量</th><th>最高置信度</th></tr>");
    for (int i = 0; i < 3; ++i) {
        html += QString::fromUtf8(
                    "<tr><td>%1</td>"
                    "<td><span style='color:%2;'>■</span> %3</td>"
                    "<td>%4</td><td>%5%</td></tr>")
                    .arg(m_yoloClasses[i])
                    .arg(cssColor[i])
                    .arg(colorNameZh[i])
                    .arg(counts.value(i, 0))
                    .arg(static_cast<int>(maxConf.value(i, 0) * 100));
    }
    html += "</table><br>";

    // 每类 top-10 详细
    html += QString::fromUtf8("<h2 style='font-size:14pt;'>各类别置信度前10位置详情</h2>");
    for (int cid = 0; cid < 3; ++cid) {
        QList<int> idxs;
        for (int i = 0; i < n; ++i) {
            if (top1Ids[i] == cid) idxs.append(i);
        }
        std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
            return confidences[a] > confidences[b];
        });
        html += QString::fromUtf8("<h3 style='color:%1; font-size:12pt;'>%2框 %3 "
                                  "(共%4个，显示前%5)</h3>")
                    .arg(cssColor[cid]).arg(colorNameZh[cid]).arg(m_yoloClasses[cid])
                    .arg(counts.value(cid, 0)).arg(qMin(10, idxs.size()));
        html += "<table border='1' cellspacing='0' cellpadding='4' "
                "style='border-collapse:collapse; font-size:10pt;'>";
        html += QString::fromUtf8("<tr><th>序号</th><th>道(横向)</th><th>采样(纵向)</th><th>置信度</th></tr>");
        int showN = qMin(10, idxs.size());
        for (int k = 0; k < showN; ++k) {
            int i = idxs[k];
            const cv::Rect &r = rects[i];
            int trace = r.x + r.width / 2;
            int sample = r.y + r.height / 2;
            html += QString::fromUtf8("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4%</td></tr>")
                        .arg(k + 1).arg(trace).arg(sample)
                        .arg(static_cast<int>(confidences[i] * 100));
        }
        html += "</table><br>";
    }

    // 嵌入图片标题（图片标签在下面计算页面宽度后再插入）
    html += QString::fromUtf8("<h2 style='font-size:14pt;'>带识别框的雷达剖面图</h2>");

    // 准备图片资源 (cv::Mat BGR → QImage RGB 深拷贝)
    cv::Mat rgb;
    cv::cvtColor(annotated, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    QImage qimgCopy = qimg.copy();
    QPixmap pixmap = QPixmap::fromImage(qimgCopy);

    QPrinter printer(QPrinter::PrinterResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(pdfPath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    // 计算页面可用宽度（设备像素），据此缩放图片并设置 HTML 像素宽度
    // QTextDocument 不支持 width='100%'，必须用具体像素值
    QSizeF pageRectPx = printer.pageRect(QPrinter::DevicePixel).size();
    int availWidth = static_cast<int>(pageRectPx.width());

    // 高 DPI 下原始 annotated 像素宽度可能远大于 availWidth，预先缩到不超页面宽
    if (pixmap.width() > availWidth) {
        pixmap = pixmap.scaledToWidth(availWidth, Qt::SmoothTransformation);
    }

    // 重新拼接图片标签：用具体像素宽度（pixmap 当前宽度）
    html += QString("<img src='annotated.jpg' width='%1' />").arg(pixmap.width());
    html += "</body></html>";

    QTextDocument doc;
    doc.setPageSize(pageRectPx);
    doc.addResource(QTextDocument::ImageResource, QUrl("annotated.jpg"), pixmap.toImage());
    doc.setHtml(html);

    doc.print(&printer);

    QMessageBox::information(this, QString::fromUtf8("AI识别"),
                             QString::fromUtf8("报告已生成:\n%1").arg(pdfPath));
}

void MainWindow::showDigitalFilter()
{
    if (!requireOpenFile()) return;

    // If dialog already exists, bring it to front
    if (m_filterDlg) {
        m_filterDlg->raise();
        m_filterDlg->activateWindow();
        return;
    }

    m_filterDlg = new QDialog(this);
    m_filterDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_filterDlg->setWindowFlags(Qt::Tool | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    QFileInfo fi(m_currentTab->filePath);
    m_filterDlg->setWindowTitle(QString("数字滤波-%1").arg(fi.completeBaseName()));
    m_filterDlg->setMinimumSize(900, 700);

    // Clear member pointers on dialog close
    connect(m_filterDlg, &QDialog::finished, this, [this]() {
        m_filterDlg = nullptr;
        m_filterSeriesBefore = nullptr;
        m_filterSeriesAfter = nullptr;
        m_filterAxisXBefore = nullptr;
        m_filterAxisXAfter = nullptr;
        m_filterAxisYBefore = nullptr;
        m_filterAxisYAfter = nullptr;
        m_filterChartAfter = nullptr;
        m_filterChartBefore = nullptr;
        m_filterChartViewBefore = nullptr;
        m_filterLowMarker = nullptr;
        m_filterHighMarker = nullptr;
        m_filterSpinLow = nullptr;
        m_filterSpinHigh = nullptr;
        m_filterBandGroup = nullptr;
        m_filterTypeGroup = nullptr;
        m_filterBtnApply = nullptr;
        m_filterApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_filterDlg);

    // --- Top: Two charts stacked vertically ---
    QVBoxLayout *chartLayout = new QVBoxLayout();

    // Chart: 处理前 (Before)
    m_filterChartBefore = new QChart();
    m_filterChartBefore->setTitle("处理前");
    m_filterChartBefore->setTitleFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    m_filterSeriesBefore = new QLineSeries();
    m_filterSeriesBefore->setColor(Qt::black);
    m_filterAxisXBefore = new QValueAxis();
    m_filterAxisXBefore->setTitleText("频率(MHz)");
    m_filterAxisXBefore->setRange(0, 6000);
    m_filterAxisYBefore = new QValueAxis();
    m_filterAxisYBefore->setTitleText("幅度(dB)");
    m_filterAxisYBefore->setRange(-500, 0);
    m_filterChartBefore->addSeries(m_filterSeriesBefore);
    m_filterChartBefore->addAxis(m_filterAxisXBefore, Qt::AlignBottom);
    m_filterChartBefore->addAxis(m_filterAxisYBefore, Qt::AlignLeft);
    m_filterSeriesBefore->attachAxis(m_filterAxisXBefore);
    m_filterSeriesBefore->attachAxis(m_filterAxisYBefore);

    // Vertical marker line: 低频 (green)
    m_filterLowMarker = new QLineSeries();
    QPen lowPen(Qt::green);
    lowPen.setWidth(2);
    m_filterLowMarker->setPen(lowPen);
    m_filterLowMarker->append(200, -500);
    m_filterLowMarker->append(200, 0);
    m_filterChartBefore->addSeries(m_filterLowMarker);
    m_filterLowMarker->attachAxis(m_filterAxisXBefore);
    m_filterLowMarker->attachAxis(m_filterAxisYBefore);

    // Vertical marker line: 高频 (red)
    m_filterHighMarker = new QLineSeries();
    QPen highPen(Qt::red);
    highPen.setWidth(2);
    m_filterHighMarker->setPen(highPen);
    m_filterHighMarker->append(600, -500);
    m_filterHighMarker->append(600, 0);
    m_filterChartBefore->addSeries(m_filterHighMarker);
    m_filterHighMarker->attachAxis(m_filterAxisXBefore);
    m_filterHighMarker->attachAxis(m_filterAxisYBefore);

    m_filterChartBefore->legend()->hide();
    auto *filterChartView = new FilterChartView(m_filterChartBefore);
    m_filterChartViewBefore = filterChartView;

    // Chart: 处理后 (After)
    m_filterChartAfter = new QChart();
    m_filterChartAfter->setTitle("处理后");
    m_filterChartAfter->setTitleFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    m_filterSeriesAfter = new QLineSeries();
    m_filterSeriesAfter->setColor(Qt::black);
    m_filterAxisXAfter = new QValueAxis();
    m_filterAxisXAfter->setTitleText("频率(MHz)");
    m_filterAxisXAfter->setRange(0, 6000);
    m_filterAxisYAfter = new QValueAxis();
    m_filterAxisYAfter->setTitleText("幅度(dB)");
    m_filterAxisYAfter->setRange(-500, 0);
    m_filterChartAfter->addSeries(m_filterSeriesAfter);
    m_filterChartAfter->addAxis(m_filterAxisXAfter, Qt::AlignBottom);
    m_filterChartAfter->addAxis(m_filterAxisYAfter, Qt::AlignLeft);
    m_filterSeriesAfter->attachAxis(m_filterAxisXAfter);
    m_filterSeriesAfter->attachAxis(m_filterAxisYAfter);
    m_filterChartAfter->legend()->hide();
    QChartView *chartViewAfter = new QChartView(m_filterChartAfter);
    chartViewAfter->setRenderHint(QPainter::Antialiasing);

    chartLayout->addWidget(m_filterChartViewBefore);
    chartLayout->addWidget(chartViewAfter);
    mainLayout->addLayout(chartLayout, 3);

    // --- Bottom: Controls ---
    QHBoxLayout *ctrlLayout = new QHBoxLayout();

    // Left: Filter type radio buttons in a group box
    QGroupBox *typeGroupBox = new QGroupBox("滤波类型");
    QVBoxLayout *typeLayout = new QVBoxLayout(typeGroupBox);
    typeLayout->setContentsMargins(6, 12, 6, 6);
    typeLayout->setSpacing(4);
    m_filterTypeGroup = new QButtonGroup(m_filterDlg);
    QRadioButton *rbFIR = new QRadioButton("FIR滤波");
    QRadioButton *rbIIR = new QRadioButton("IIR滤波");
    rbFIR->setChecked(true);
    m_filterTypeGroup->addButton(rbFIR, 0);
    m_filterTypeGroup->addButton(rbIIR, 1);
    QHBoxLayout *typeRow = new QHBoxLayout();
    typeRow->addWidget(rbFIR);
    typeRow->addWidget(rbIIR);
    typeLayout->addLayout(typeRow);

    m_filterBandGroup = new QButtonGroup(m_filterDlg);
    QRadioButton *rbLowPass  = new QRadioButton("低通");
    QRadioButton *rbHighPass = new QRadioButton("高通");
    QRadioButton *rbBandPass = new QRadioButton("带通");
    QRadioButton *rbBandStop = new QRadioButton("带阻");
    rbBandPass->setChecked(true);
    m_filterBandGroup->addButton(rbLowPass, 0);
    m_filterBandGroup->addButton(rbHighPass, 1);
    m_filterBandGroup->addButton(rbBandPass, 2);
    m_filterBandGroup->addButton(rbBandStop, 3);
    QHBoxLayout *bandRow = new QHBoxLayout();
    bandRow->addWidget(rbLowPass);
    bandRow->addWidget(rbHighPass);
    bandRow->addWidget(rbBandPass);
    bandRow->addWidget(rbBandStop);
    typeLayout->addLayout(bandRow);
    ctrlLayout->addWidget(typeGroupBox);

    // Radio button → lock/unlock low freq spinbox + marker line visibility
    auto updateBandUI = [this]() {
        int bandType = m_filterBandGroup ? m_filterBandGroup->checkedId() : 2;
        bool needLow  = (bandType != 0);
        bool needHigh = (bandType != 1);
        if (m_filterSpinLow)  m_filterSpinLow->setEnabled(needLow);
        if (m_filterSpinHigh) m_filterSpinHigh->setEnabled(needHigh);
        if (m_filterLowMarker)  m_filterLowMarker->setVisible(needLow);
        if (m_filterHighMarker) m_filterHighMarker->setVisible(needHigh);
    };
    connect(m_filterBandGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, updateBandUI);
    // updateBandUI() called after spinboxes are created

    // Middle: Frequency parameters in a group box
    QGroupBox *freqGroupBox = new QGroupBox("频率参数");
    QVBoxLayout *freqLayout = new QVBoxLayout(freqGroupBox);
    freqLayout->setContentsMargins(6, 12, 6, 6);
    freqLayout->setSpacing(6);
    QHBoxLayout *lowFreqRow = new QHBoxLayout();
    lowFreqRow->setSpacing(2);
    lowFreqRow->setContentsMargins(0,0,0,0);
    QLabel *lblLow = new QLabel("低频:");
    lblLow->setFixedWidth(32);
    lowFreqRow->addWidget(lblLow);
    m_filterSpinLow = new QDoubleSpinBox();
    m_filterSpinLow->setRange(0, 12800);
    m_filterSpinLow->setValue(200);
    m_filterSpinLow->setDecimals(0);
    m_filterSpinLow->setFixedWidth(70);
    lowFreqRow->addWidget(m_filterSpinLow);
    freqLayout->addLayout(lowFreqRow);

    QHBoxLayout *highFreqRow = new QHBoxLayout();
    highFreqRow->setSpacing(2);
    highFreqRow->setContentsMargins(0,0,0,0);
    QLabel *lblHigh = new QLabel("高频:");
    lblHigh->setFixedWidth(32);
    highFreqRow->addWidget(lblHigh);
    m_filterSpinHigh = new QDoubleSpinBox();
    m_filterSpinHigh->setRange(0, 12800);
    m_filterSpinHigh->setValue(600);
    m_filterSpinHigh->setDecimals(0);
    m_filterSpinHigh->setFixedWidth(70);
    highFreqRow->addWidget(m_filterSpinHigh);
    freqLayout->addLayout(highFreqRow);
    ctrlLayout->addWidget(freqGroupBox);

    // Now spinboxes exist, apply initial band UI state
    updateBandUI();

    // Connect marker drag to spinboxes (must be after spinboxes created)
    filterChartView->setMarkers(m_filterLowMarker, m_filterHighMarker,
                                m_filterAxisXBefore, m_filterAxisYBefore,
                                m_filterSpinLow, m_filterSpinHigh);
    filterChartView->setFreqChangedCallback([this]() {
        computeFilteredSpectrumPreview();
    });

    // Re-compute filtered preview when filter type, band type or frequency changes
    connect(m_filterTypeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
            [this]() { computeFilteredSpectrumPreview(); });
    connect(m_filterBandGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
            [this]() { computeFilteredSpectrumPreview(); });
    connect(m_filterSpinLow, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this]() { computeFilteredSpectrumPreview(); });
    connect(m_filterSpinHigh, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this]() { computeFilteredSpectrumPreview(); });

    // Spinbox → marker line sync
    connect(m_filterSpinLow, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this]() { updateFilterMarkerLine(m_filterLowMarker, m_filterSpinLow->value()); });
    connect(m_filterSpinHigh, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this]() { updateFilterMarkerLine(m_filterHighMarker, m_filterSpinHigh->value()); });

    // Right: Action buttons
    QVBoxLayout *btnLayout = new QVBoxLayout();
    QHBoxLayout *zoomRow = new QHBoxLayout();
    QPushButton *btnZoomIn  = new QPushButton("放大");
    QPushButton *btnZoomOut = new QPushButton("缩小");
    QPushButton *btnReset   = new QPushButton("还原");
    zoomRow->addWidget(btnZoomIn);
    zoomRow->addWidget(btnZoomOut);
    zoomRow->addWidget(btnReset);
    btnLayout->addLayout(zoomRow);

    QHBoxLayout *actionRow = new QHBoxLayout();
    m_filterBtnApply = new QPushButton("应用");
    QPushButton *btnOK     = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    m_filterBtnApply->setStyleSheet("QPushButton { background-color: #0078d7; color: white; padding: 4px 12px; }");
    btnOK->setStyleSheet("QPushButton { background-color: #0078d7; color: white; padding: 4px 12px; }");
    actionRow->addWidget(m_filterBtnApply);
    actionRow->addWidget(btnOK);
    actionRow->addWidget(btnCancel);
    btnLayout->addLayout(actionRow);
    ctrlLayout->addLayout(btnLayout);

    mainLayout->addLayout(ctrlLayout, 1);

    // Show spectrum for the last clicked trace
    m_filterApplied = false;
    updateFilterSpectrum(m_lastChartX);

    // Connections
    connect(m_filterBtnApply, &QPushButton::clicked, this, [this]() {
        if (!requireOpenFile()) return;

        if (m_filterApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_filterApplied = false;
            m_filterBtnApply->setText("应用");
            refreshImage();
            updateChart(m_lastChartX);
            updateFilterSpectrum(m_lastChartX);
            computeFilteredSpectrumPreview();
            return;
        }

        int N = m_currentTab->pixelsPerRow;
        int traceCount = m_rawData.size() / (N * 4);
        double fsHz = N / (m_currentTab->timeRange * 1e-9);
        double lowHz = m_filterSpinLow->value() * 1e6;
        double highHz = m_filterSpinHigh->value() * 1e6;
        int bandType = m_filterBandGroup ? m_filterBandGroup->checkedId() : 2;
        bool isIIR = m_filterTypeGroup && m_filterTypeGroup->checkedId() == 1;

        // Backup
        m_currentTab->originalRawData = m_rawData;
        const char *src = m_currentTab->originalRawData.constData();
        char *dst = m_rawData.data();

        if (isIIR) {
            // --- IIR Butterworth 8-order per section (forward-backward → effective 16) ---
            // Band-pass = HP(lowHz) cascade LP(highHz), Band-stop = LP(lowHz) cascade HP(highHz)
            int order = 8;
            int nSos = order / 2;

            struct Biquad { double b0, b1, b2, a1, a2; };
            Biquad sos[8]; // max 8 sections for cascade
            int totalSos = nSos;

            double Wc1 = tan(M_PI * lowHz / fsHz);
            double Wc2 = tan(M_PI * highHz / fsHz);

            for (int k = 0; k < nSos; ++k) {
                double theta = M_PI * (2*k + 1) / (2*order);
                double d = sin(theta);

                switch (bandType) {
                case 0: { // Low-pass
                    double Wc = Wc2;
                    double a0 = 1 + 2*d*Wc + Wc*Wc;
                    sos[k] = {Wc*Wc/a0, 2*Wc*Wc/a0, Wc*Wc/a0,
                              2*(Wc*Wc-1)/a0, (1-2*d*Wc+Wc*Wc)/a0};
                    break;
                }
                case 1: { // High-pass
                    double Wc = Wc1;
                    double a0 = Wc*Wc + 2*d*Wc + 1;
                    sos[k] = {1.0/a0, -2.0/a0, 1.0/a0,
                              2*(Wc*Wc-1)/a0, (Wc*Wc-2*d*Wc+1)/a0};
                    break;
                }
                case 2: { // Band-pass = HP(lowHz) then LP(highHz)
                    totalSos = nSos * 2;
                    // HP section (cutoff = lowHz)
                    {
                        double a0 = Wc1*Wc1 + 2*d*Wc1 + 1;
                        sos[k] = {1.0/a0, -2.0/a0, 1.0/a0,
                                  2*(Wc1*Wc1-1)/a0, (Wc1*Wc1-2*d*Wc1+1)/a0};
                    }
                    // LP section (cutoff = highHz)
                    {
                        double a0 = 1 + 2*d*Wc2 + Wc2*Wc2;
                        sos[nSos + k] = {Wc2*Wc2/a0, 2*Wc2*Wc2/a0, Wc2*Wc2/a0,
                                         2*(Wc2*Wc2-1)/a0, (1-2*d*Wc2+Wc2*Wc2)/a0};
                    }
                    break;
                }
                case 3: { // Band-stop = LP(lowHz) then HP(highHz)
                    totalSos = nSos * 2;
                    // LP section (cutoff = lowHz)
                    {
                        double a0 = 1 + 2*d*Wc1 + Wc1*Wc1;
                        sos[k] = {Wc1*Wc1/a0, 2*Wc1*Wc1/a0, Wc1*Wc1/a0,
                                  2*(Wc1*Wc1-1)/a0, (1-2*d*Wc1+Wc1*Wc1)/a0};
                    }
                    // HP section (cutoff = highHz)
                    {
                        double a0 = Wc2*Wc2 + 2*d*Wc2 + 1;
                        sos[nSos + k] = {1.0/a0, -2.0/a0, 1.0/a0,
                                         2*(Wc2*Wc2-1)/a0, (Wc2*Wc2-2*d*Wc2+1)/a0};
                    }
                    break;
                }
                }
            }

            // Apply IIR: forward-backward for zero phase
            for (int t = 0; t < traceCount; ++t) {
                double buf[512];
                const qint32 *s32 = reinterpret_cast<const qint32*>(src + t * N * 4);
                for (int i = 0; i < N; ++i) buf[i] = s32[i];

                // Forward pass through all sections
                for (int s = 0; s < totalSos; ++s) {
                    double w1 = 0, w2 = 0;
                    double b0=sos[s].b0, b1=sos[s].b1, b2=sos[s].b2;
                    double a1=sos[s].a1, a2=sos[s].a2;
                    for (int i = 0; i < N; ++i) {
                        double w0 = buf[i] - a1*w1 - a2*w2;
                        buf[i] = b0*w0 + b1*w1 + b2*w2;
                        w2 = w1; w1 = w0;
                    }
                }

                // Backward pass (reverse iteration, no array flip needed)
                for (int s = 0; s < totalSos; ++s) {
                    double w1 = 0, w2 = 0;
                    double b0=sos[s].b0, b1=sos[s].b1, b2=sos[s].b2;
                    double a1=sos[s].a1, a2=sos[s].a2;
                    for (int i = N-1; i >= 0; --i) {
                        double w0 = buf[i] - a1*w1 - a2*w2;
                        buf[i] = b0*w0 + b1*w1 + b2*w2;
                        w2 = w1; w1 = w0;
                    }
                }

                qint32 *d32 = reinterpret_cast<qint32*>(dst + t * N * 4);
                for (int i = 0; i < N; ++i)
                    d32[i] = static_cast<qint32>(buf[i]);
            }
        } else {
            // --- FIR 32-order ---
            int order = 32;
            int M = order;
            int hLen = M + 1;
            double fc1 = lowHz / fsHz;
            double fc2 = highHz / fsHz;
            double h[33];
            for (int n = 0; n < hLen; ++n) {
                double nm = n - M / 2.0;
                double hd = 0.0;
                switch (bandType) {
                case 0:
                    if (nm == 0.0) hd = 2.0 * fc2;
                    else hd = 2.0 * fc2 * sin(2.0 * M_PI * fc2 * nm) / (2.0 * M_PI * fc2 * nm);
                    break;
                case 1:
                    if (nm == 0.0) hd = 1.0 - 2.0 * fc1;
                    else hd = -2.0 * fc1 * sin(2.0 * M_PI * fc1 * nm) / (2.0 * M_PI * fc1 * nm);
                    break;
                case 2:
                    if (nm == 0.0) hd = 2.0 * (fc2 - fc1);
                    else hd = 2.0 * fc2 * sin(2.0 * M_PI * fc2 * nm) / (2.0 * M_PI * fc2 * nm)
                             - 2.0 * fc1 * sin(2.0 * M_PI * fc1 * nm) / (2.0 * M_PI * fc1 * nm);
                    break;
                case 3:
                    if (nm == 0.0) hd = 1.0 - 2.0 * (fc2 - fc1);
                    else hd = -2.0 * fc2 * sin(2.0 * M_PI * fc2 * nm) / (2.0 * M_PI * fc2 * nm)
                             + 2.0 * fc1 * sin(2.0 * M_PI * fc1 * nm) / (2.0 * M_PI * fc1 * nm);
                    break;
                }
                double w = 0.54 - 0.46 * cos(2.0 * M_PI * n / M);
                h[n] = hd * w;
            }

            for (int t = 0; t < traceCount; ++t) {
                const qint32 *s32 = reinterpret_cast<const qint32*>(src + t * N * 4);
                qint32 *d32 = reinterpret_cast<qint32*>(dst + t * N * 4);
                for (int i = 0; i < N; ++i) {
                    double sum = 0.0;
                    for (int k = 0; k < hLen; ++k) {
                        int si = i + M / 2 - k;
                        if (si >= 0 && si < N) sum += s32[si] * h[k];
                    }
                    d32[i] = static_cast<qint32>(sum);
                }
            }
        }

        m_currentTab->rawData = m_rawData;
        m_filterApplied = true;
        m_filterBtnApply->setText("撤销");

        refreshImage();
        updateChart(m_lastChartX);
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        // 若未应用，先触发应用（与其它处理对话框一致）
        if (!m_filterApplied && m_filterBtnApply) {
            m_filterBtnApply->click();
        }
        if (m_filterApplied) {
            saveProcessedFile();
        }
        if (m_filterDlg) {
            m_filterDlg->close();
        }
    });

    connect(btnCancel, &QPushButton::clicked, m_filterDlg, &QDialog::reject);

    connect(btnZoomIn, &QPushButton::clicked, this, [this]() {
        double curMax = m_filterAxisXBefore->max();
        double newMax = qMax(1200.0, curMax - 600.0);
        m_filterAxisXBefore->setRange(0, newMax);
        m_filterAxisXAfter->setRange(0, newMax);
    });
    connect(btnZoomOut, &QPushButton::clicked, this, [this]() {
        double curMax = m_filterAxisXBefore->max();
        double newMax = qMin(6000.0, curMax + 600.0);
        m_filterAxisXBefore->setRange(0, newMax);
        m_filterAxisXAfter->setRange(0, newMax);
    });
    connect(btnReset, &QPushButton::clicked, this, [this]() {
        m_filterAxisXBefore->setRange(0, 6000);
        m_filterAxisXAfter->setRange(0, 6000);
    });

    m_filterDlg->show();
}

void MainWindow::updateFilterSpectrum(int traceIdx)
{
    if (!m_filterSeriesBefore || !m_currentTab) return;

    const QByteArray &rawData = m_currentTab->rawData;
    int N = m_currentTab->pixelsPerRow;
    int traceCount = rawData.size() / (N * 4);
    if (traceIdx < 0 || traceIdx >= traceCount) return;

    m_filterSeriesBefore->clear();

    // Next power of 2 for FFT
    int fftN = 1;
    while (fftN < N) fftN <<= 1;

    std::vector<std::complex<double>> x(fftN, 0.0);
    for (int i = 0; i < N; ++i) {
        int idx = (traceIdx * N + i) * 4;
        qint32 val = static_cast<quint8>(rawData[idx])
                   | (static_cast<quint8>(rawData[idx+1]) << 8)
                   | (static_cast<quint8>(rawData[idx+2]) << 16)
                   | (static_cast<quint8>(rawData[idx+3]) << 24);
        x[i] = std::complex<double>(val, 0.0);
    }
    fft(x);

    // 20ns, 512 samples: fs = 512/20e-9 = 25.6GHz = 25600MHz
    // freqStep = 25600/512 = 50MHz, Nyquist = 12800MHz
    double fs = N / (m_currentTab->timeRange * 1e-9);  // Hz
    double freqStep = fs / fftN / 1e6;                  // MHz

    double maxDb = -500.0, minDb = 500.0;
    for (int i = 0; i < fftN / 2; ++i) {
        double mag = std::abs(x[i]);
        double db = (mag > 0) ? 20.0 * log10(mag) : -500.0;
        if (db < -500.0) db = -500.0;
        if (db > maxDb) maxDb = db;
        if (db < minDb) minDb = db;
    }
    // Normalize: peak = 0 dB
    for (int i = 0; i < fftN / 2; ++i) {
        double mag = std::abs(x[i]);
        double db = (mag > 0) ? 20.0 * log10(mag) : -500.0;
        if (db < -500.0) db = -500.0;
        m_filterSeriesBefore->append(i * freqStep, db - maxDb);
    }

    // Auto-scale Y axis (normalized, range 0 to bottom)
    if (m_filterAxisYBefore) {
        double range = maxDb - minDb;
        m_filterAxisYBefore->setRange(qMax(-200.0, -range - 10), 5);
    }
    if (m_filterAxisYAfter) {
        double range = maxDb - minDb;
        m_filterAxisYAfter->setRange(qMax(-200.0, -range - 10), 5);
    }

    // Compute filtered spectrum preview
    if (!m_filterApplied) {
        computeFilteredSpectrumPreview();
    }

    // Update marker line Y range to match new axis
    updateFilterMarkerLine(m_filterLowMarker, m_filterSpinLow ? m_filterSpinLow->value() : 200);
    updateFilterMarkerLine(m_filterHighMarker, m_filterSpinHigh ? m_filterSpinHigh->value() : 600);
}

void MainWindow::computeFilteredSpectrumPreview()
{
    if (!m_filterSeriesAfter || !m_filterSeriesBefore || !m_currentTab) return;

    m_filterSeriesAfter->clear();
    if (m_filterSeriesBefore->points().isEmpty()) return;

    int bandType = m_filterBandGroup ? m_filterBandGroup->checkedId() : 2;
    double lowMHz = m_filterSpinLow ? m_filterSpinLow->value() : 200;
    double highMHz = m_filterSpinHigh ? m_filterSpinHigh->value() : 600;
    bool isIIR = m_filterTypeGroup && m_filterTypeGroup->checkedId() == 1;
    double fsHz = m_currentTab->pixelsPerRow / (m_currentTab->timeRange * 1e-9);
    double lowHz = lowMHz * 1e6;
    double highHz = highMHz * 1e6;

    int fftN = 512;

    // Compute filter frequency response H(w)
    std::vector<std::complex<double>> H(fftN, std::complex<double>(0.0, 0.0));

    if (isIIR) {
        // IIR Butterworth order 8, forward-backward → effective order 16
        // Forward-backward squares the magnitude: |H_fb| = |H|^2
        int N = 8;
        double Wc1 = tan(M_PI * lowHz / fsHz);
        double Wc2 = tan(M_PI * highHz / fsHz);

        for (int i = 0; i < fftN; ++i) {
            double fd = (double)i * fsHz / fftN;
            if (fd <= 0) { H[i] = std::complex<double>(1.0, 0.0); continue; }
            double Wa = tan(M_PI * fd / fsHz);
            double mag = 0.0;
            switch (bandType) {
            case 0: { // Low-pass
                double x = Wa / Wc2;
                double mag1 = 1.0 / sqrt(1.0 + pow(x, 2*N));
                mag = mag1 * mag1;  // forward-backward squares magnitude
                break;
            }
            case 1: { // High-pass
                double x = Wc1 / Wa;
                double mag1 = 1.0 / sqrt(1.0 + pow(x, 2*N));
                mag = mag1 * mag1;
                break;
            }
            case 2: { // Band-pass
                double BW = Wc2 - Wc1;
                double W0sq = Wc1 * Wc2;
                double x = (Wa*Wa - W0sq) / (BW * Wa);
                double mag1 = 1.0 / sqrt(1.0 + pow(x, 2*N));
                mag = mag1 * mag1;
                break;
            }
            case 3: { // Band-stop
                double BW = Wc2 - Wc1;
                double W0sq = Wc1 * Wc2;
                double x = BW * Wa / fabs(Wa*Wa - W0sq + 1e-30);
                double mag1 = 1.0 / sqrt(1.0 + pow(x, 2*N));
                mag = mag1 * mag1;
                break;
            }
            }
            H[i] = std::complex<double>(mag, 0.0);
        }
    } else {
        // FIR: design coefficients then FFT for frequency response
        int order = 32, M = order, hLen = M + 1;
        double fc1 = lowHz / fsHz, fc2 = highHz / fsHz;
        for (int n = 0; n < hLen; ++n) {
            double nm = n - M / 2.0;
            double hd = 0.0;
            switch (bandType) {
            case 0:
                if (nm == 0.0) hd = 2.0*fc2;
                else hd = 2.0*fc2*sin(2*M_PI*fc2*nm)/(2*M_PI*fc2*nm);
                break;
            case 1:
                if (nm == 0.0) hd = 1.0-2.0*fc1;
                else hd = -2.0*fc1*sin(2*M_PI*fc1*nm)/(2*M_PI*fc1*nm);
                break;
            case 2:
                if (nm == 0.0) hd = 2.0*(fc2-fc1);
                else hd = 2.0*fc2*sin(2*M_PI*fc2*nm)/(2*M_PI*fc2*nm)
                         -2.0*fc1*sin(2*M_PI*fc1*nm)/(2*M_PI*fc1*nm);
                break;
            case 3:
                if (nm == 0.0) hd = 1.0-2.0*(fc2-fc1);
                else hd = -2.0*fc2*sin(2*M_PI*fc2*nm)/(2*M_PI*fc2*nm)
                         +2.0*fc1*sin(2*M_PI*fc1*nm)/(2*M_PI*fc1*nm);
                break;
            }
            double w = 0.54 - 0.46 * cos(2*M_PI*n/M);
            H[n] = std::complex<double>(hd * w, 0.0);
        }
        fft(H);
    }

    double freqStep = fsHz / fftN / 1e6;

    const QByteArray &rawData = m_currentTab->originalRawData.isEmpty()
                                ? m_currentTab->rawData : m_currentTab->originalRawData;
    int N = m_currentTab->pixelsPerRow;
    int traceIdx = m_lastChartX;
    int traceCount = rawData.size() / (N * 4);
    if (traceIdx < 0 || traceIdx >= traceCount) return;

    std::vector<std::complex<double>> sig(fftN, 0.0);
    for (int i = 0; i < N; ++i) {
        int idx = (traceIdx * N + i) * 4;
        qint32 val = static_cast<quint8>(rawData[idx])
                   | (static_cast<quint8>(rawData[idx+1]) << 8)
                   | (static_cast<quint8>(rawData[idx+2]) << 16)
                   | (static_cast<quint8>(rawData[idx+3]) << 24);
        sig[i] = std::complex<double>(val, 0.0);
    }
    fft(sig);

    // Compute filtered values, find own peak for normalization
    double filtMaxDb = -500.0;
    double filtDb[256];
    for (int i = 0; i < fftN / 2; ++i) {
        auto filtered = sig[i] * H[i];
        double mag = std::abs(filtered);
        double db = (mag > 0) ? 20.0 * log10(mag) : -500.0;
        if (db < -500.0) db = -500.0;
        filtDb[i] = db;
        if (db > filtMaxDb) filtMaxDb = db;
    }

    for (int i = 0; i < fftN / 2; ++i) {
        m_filterSeriesAfter->append(i * freqStep, filtDb[i] - filtMaxDb);
    }
}

void MainWindow::updateFilterSpectrumFiltered(int traceIdx)
{
    if (!m_filterSeriesAfter || !m_currentTab) return;

    const QByteArray &rawData = m_currentTab->rawData;
    int N = m_currentTab->pixelsPerRow;
    int traceCount = rawData.size() / (N * 4);
    if (traceIdx < 0 || traceIdx >= traceCount) return;

    m_filterSeriesAfter->clear();

    int fftN = 1;
    while (fftN < N) fftN <<= 1;

    std::vector<std::complex<double>> x(fftN, 0.0);
    for (int i = 0; i < N; ++i) {
        int idx = (traceIdx * N + i) * 4;
        qint32 val = static_cast<quint8>(rawData[idx])
                   | (static_cast<quint8>(rawData[idx+1]) << 8)
                   | (static_cast<quint8>(rawData[idx+2]) << 16)
                   | (static_cast<quint8>(rawData[idx+3]) << 24);
        x[i] = std::complex<double>(val, 0.0);
    }
    fft(x);

    double fs = N / (m_currentTab->timeRange * 1e-9);
    double freqStep = fs / fftN / 1e6;

    // Normalize: peak = 0 dB
    double maxDb = -500.0;
    double dbArr[256];
    int halfN = fftN / 2;
    for (int i = 0; i < halfN; ++i) {
        double mag = std::abs(x[i]);
        double db = (mag > 0) ? 20.0 * log10(mag) : -500.0;
        if (db < -500.0) db = -500.0;
        dbArr[i] = db;
        if (db > maxDb) maxDb = db;
    }
    for (int i = 0; i < halfN; ++i) {
        m_filterSeriesAfter->append(i * freqStep, dbArr[i] - maxDb);
    }
}

void MainWindow::updateFilterMarkerLine(QLineSeries *marker, double freq)
{
    if (!marker || !m_filterAxisYBefore) return;
    double yMin = m_filterAxisYBefore->min();
    double yMax = m_filterAxisYBefore->max();
    marker->replace(0, QPointF(freq, yMin));
    marker->replace(1, QPointF(freq, yMax));
}

void MainWindow::showBackgroundRemoval()
{
    if (!requireOpenFile()) return;

    if (m_bgRemovalDlg) {
        m_bgRemovalDlg->raise();
        m_bgRemovalDlg->activateWindow();
        return;
    }

    m_bgRemovalDlg = new QDialog(this);
    m_bgRemovalDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_bgRemovalDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_bgRemovalDlg->setWindowTitle(QString("背景消除-%1").arg(fi.completeBaseName()));

    connect(m_bgRemovalDlg, &QDialog::finished, this, [this]() {
        m_bgRemovalDlg = nullptr;
        m_bgRemovalMethodCombo = nullptr;
        m_bgRemovalWindowSpin = nullptr;
        m_bgRemovalBtnApply = nullptr;
        m_bgRemovalApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_bgRemovalDlg);

    // Row 1: 处理方式
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->addWidget(new QLabel("处理方式："));
    m_bgRemovalMethodCombo = new QComboBox();
    m_bgRemovalMethodCombo->addItem("整体法");
    m_bgRemovalMethodCombo->addItem("扫描范围");
    m_bgRemovalMethodCombo->addItem("全部通过");
    m_bgRemovalMethodCombo->addItem("自适应");
    m_bgRemovalMethodCombo->addItem("无");
    row1->addWidget(m_bgRemovalMethodCombo);
    row1->addStretch();
    mainLayout->addLayout(row1);

    // Row 2: 滑动窗口
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->addWidget(new QLabel("滑动窗口："));
    m_bgRemovalWindowSpin = new QSpinBox();
    m_bgRemovalWindowSpin->setRange(1, 9999);
    m_bgRemovalWindowSpin->setValue(200);
    row2->addWidget(m_bgRemovalWindowSpin);
    row2->addStretch();
    mainLayout->addLayout(row2);

    // Enable/disable window spin based on method
    connect(m_bgRemovalMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        // Only enable window spin for "扫描范围" (index 1)
        m_bgRemovalWindowSpin->setEnabled(idx == 1);
    });
    m_bgRemovalWindowSpin->setEnabled(false); // default "整体法" disables it

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_bgRemovalBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_bgRemovalBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    // Apply button: toggle apply/undo
    connect(m_bgRemovalBtnApply, &QPushButton::clicked, this, [this]() {
        applyBackgroundRemoval();
    });

    // OK button: apply then save
    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_bgRemovalApplied)
            applyBackgroundRemoval();
        if (m_bgRemovalApplied)
            saveProcessedFile();
        if (m_bgRemovalDlg)
            m_bgRemovalDlg->close();
    });

    // Cancel button: undo if applied, then close
    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_bgRemovalApplied) {
            // Undo: restore original data
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_bgRemovalApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_bgRemovalDlg)
            m_bgRemovalDlg->close();
    });

    m_bgRemovalDlg->show();
}

void MainWindow::applyBackgroundRemoval()
{
    if (!requireOpenFile()) return;

    if (m_bgRemovalApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_bgRemovalApplied = false;
        m_bgRemovalBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    int methodIdx = m_bgRemovalMethodCombo ? m_bgRemovalMethodCombo->currentIndex() : 0;

    // "无" means no processing
    if (methodIdx == 4) return;

    // Backup original data
    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    // Show progress bar
    m_progressBar->setValue(0);
    m_progressBar->setFormat("背景消除: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    // Parse raw data into 2D array of qint32
    const char *srcData = m_rawData.constData();
    std::vector<qint32> samples(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        samples[i] = (static_cast<quint8>(srcData[idx+3]) << 24) |
                     (static_cast<quint8>(srcData[idx+2]) << 16) |
                     (static_cast<quint8>(srcData[idx+1]) << 8) |
                     (static_cast<quint8>(srcData[idx]));
    }

    m_progressBar->setValue(10);
    m_progressBar->setFormat("背景消除: 处理中... %p%");
    QCoreApplication::processEvents();

    if (methodIdx == 0 || methodIdx == 2) {
        // 整体法 / 全部通过: compute global average, subtract from each
        std::vector<double> avg(samplesPerTrace, 0.0);
        for (int t = 0; t < numTraces; ++t) {
            for (int s = 0; s < samplesPerTrace; ++s) {
                avg[s] += samples[t * samplesPerTrace + s];
            }
        }
        for (int s = 0; s < samplesPerTrace; ++s)
            avg[s] /= numTraces;
        for (int t = 0; t < numTraces; ++t) {
            for (int s = 0; s < samplesPerTrace; ++s) {
                samples[t * samplesPerTrace + s] -= static_cast<qint32>(avg[s]);
            }
            if (t % qMax(1, numTraces / 20) == 0) {
                m_progressBar->setValue(10 + 70 * t / numTraces);
                QCoreApplication::processEvents();
            }
        }
    } else if (methodIdx == 1) {
        // 扫描范围: sliding window of N traces
        int winSize = m_bgRemovalWindowSpin ? m_bgRemovalWindowSpin->value() : 200;
        if (winSize < 1) winSize = 1;
        int halfWin = winSize / 2;

        for (int t = 0; t < numTraces; ++t) {
            int start = qMax(0, t - halfWin);
            int end = qMin(numTraces - 1, t + halfWin);
            int count = end - start + 1;

            for (int s = 0; s < samplesPerTrace; ++s) {
                double sum = 0.0;
                for (int tt = start; tt <= end; ++tt) {
                    sum += samples[tt * samplesPerTrace + s];
                }
                samples[t * samplesPerTrace + s] -= static_cast<qint32>(sum / count);
            }
            if (t % qMax(1, numTraces / 20) == 0) {
                m_progressBar->setValue(10 + 70 * t / numTraces);
                QCoreApplication::processEvents();
            }
        }
    } else if (methodIdx == 3) {
        // 自适应: Gaussian weighted local average
        int winSize = qMax(10, numTraces / 10);
        int halfWin = winSize / 2;

        for (int t = 0; t < numTraces; ++t) {
            int start = qMax(0, t - halfWin);
            int end = qMin(numTraces - 1, t + halfWin);
            double sumWeight = 0.0;

            std::vector<double> avg(samplesPerTrace, 0.0);
            for (int tt = start; tt <= end; ++tt) {
                double dist = qAbs(tt - t);
                double sigma = halfWin / 3.0;
                double w = std::exp(-(dist * dist) / (2.0 * sigma * sigma));
                sumWeight += w;
                for (int s = 0; s < samplesPerTrace; ++s) {
                    avg[s] += w * samples[tt * samplesPerTrace + s];
                }
            }
            for (int s = 0; s < samplesPerTrace; ++s) {
                avg[s] /= sumWeight;
                samples[t * samplesPerTrace + s] -= static_cast<qint32>(avg[s]);
            }
            if (t % qMax(1, numTraces / 20) == 0) {
                m_progressBar->setValue(10 + 70 * t / numTraces);
                m_progressBar->setFormat(QString("背景消除: 自适应处理 %1/%2").arg(t + 1).arg(numTraces));
                QCoreApplication::processEvents();
            }
        }
    }

    // Write back to m_rawData
    m_progressBar->setValue(85);
    m_progressBar->setFormat("背景消除: 写回数据...");
    QCoreApplication::processEvents();

    char *dstData = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        qint32 val = samples[i];
        int idx = i * 4;
        dstData[idx]   = val & 0xFF;
        dstData[idx+1] = (val >> 8) & 0xFF;
        dstData[idx+2] = (val >> 16) & 0xFF;
        dstData[idx+3] = (val >> 24) & 0xFF;
    }

    m_progressBar->setValue(95);
    m_progressBar->setFormat("背景消除: 刷新图像...");
    QCoreApplication::processEvents();

    m_currentTab->rawData = m_rawData;
    m_bgRemovalApplied = true;
    if (m_bgRemovalBtnApply)
        m_bgRemovalBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    // Sync one-click dialog reference chart
    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("背景消除: 完成");
    QCoreApplication::processEvents();

    // Hide progress bar after a short delay
    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::showMovingAverage()
{
    if (!requireOpenFile()) return;

    if (m_movingAvgDlg) {
        m_movingAvgDlg->raise();
        m_movingAvgDlg->activateWindow();
        return;
    }

    m_movingAvgDlg = new QDialog(this);
    m_movingAvgDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_movingAvgDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_movingAvgDlg->setWindowTitle(QString("滑动平均-%1").arg(fi.completeBaseName()));

    connect(m_movingAvgDlg, &QDialog::finished, this, [this]() {
        m_movingAvgDlg = nullptr;
        m_movingAvgWindowSpin = nullptr;
        m_movingAvgBtnApply = nullptr;
        m_movingAvgApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_movingAvgDlg);

    // 参数设置 group
    QGroupBox *paramGroup = new QGroupBox("参数设置");
    QHBoxLayout *paramRow = new QHBoxLayout(paramGroup);
    paramRow->addWidget(new QLabel("滑动窗口："));
    m_movingAvgWindowSpin = new QSpinBox();
    m_movingAvgWindowSpin->setRange(1, 999);
    m_movingAvgWindowSpin->setValue(8);
    paramRow->addWidget(m_movingAvgWindowSpin);
    paramRow->addStretch();
    mainLayout->addWidget(paramGroup);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_movingAvgBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_movingAvgBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // Apply button: toggle apply/undo
    connect(m_movingAvgBtnApply, &QPushButton::clicked, this, [this]() {
        applyMovingAverage();
    });

    // OK button: apply then save processed file.
    // saveProcessedFile writes proc/<name>_p##.DZT, opens it as a new tab,
    // and makes that new tab active — the processed result shows in the image area.
    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_movingAvgApplied)
            applyMovingAverage();
        if (m_movingAvgApplied)
            saveProcessedFile();
        if (m_movingAvgDlg)
            m_movingAvgDlg->close();
    });

    // Cancel button: undo if applied, then close
    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_movingAvgApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_movingAvgApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_movingAvgDlg)
            m_movingAvgDlg->close();
    });

    m_movingAvgDlg->show();
}

void MainWindow::applyMovingAverage()
{
    if (!requireOpenFile()) return;

    if (m_movingAvgApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_movingAvgApplied = false;
        m_movingAvgBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    int winSize = m_movingAvgWindowSpin ? m_movingAvgWindowSpin->value() : 8;
    if (winSize < 1) winSize = 1;
    int halfWin = winSize / 2;

    // Backup original data
    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    // Show progress bar
    m_progressBar->setValue(0);
    m_progressBar->setFormat("滑动平均: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    // Parse raw data into 2D array of qint32 (little-endian 4-byte)
    const char *srcData = m_rawData.constData();
    std::vector<qint32> samples(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        samples[i] = (static_cast<quint8>(srcData[idx+3]) << 24) |
                     (static_cast<quint8>(srcData[idx+2]) << 16) |
                     (static_cast<quint8>(srcData[idx+1]) << 8) |
                     (static_cast<quint8>(srcData[idx]));
    }

    m_progressBar->setValue(10);
    m_progressBar->setFormat("滑动平均: 处理中... %p%");
    QCoreApplication::processEvents();

    // Moving average along depth (s-axis) for each trace, boundary-clamped
    for (int t = 0; t < numTraces; ++t) {
        for (int s = 0; s < samplesPerTrace; ++s) {
            int sStart = qMax(0, s - halfWin);
            int sEnd = qMin(samplesPerTrace - 1, s + halfWin);
            double sum = 0.0;
            int count = sEnd - sStart + 1;
            for (int ss = sStart; ss <= sEnd; ++ss) {
                sum += samples[t * samplesPerTrace + ss];
            }
            samples[t * samplesPerTrace + s] = static_cast<qint32>(sum / count);
        }
        if (t % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(10 + 70 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }

    // Write back to m_rawData
    char *data = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        qint32 val = samples[i];
        data[idx]   = val & 0xFF;
        data[idx+1] = (val >> 8) & 0xFF;
        data[idx+2] = (val >> 16) & 0xFF;
        data[idx+3] = (val >> 24) & 0xFF;
    }

    m_currentTab->rawData = m_rawData;
    m_movingAvgApplied = true;
    m_movingAvgBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    // Sync one-click dialog reference chart if visible
    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("滑动平均: 完成");
    QCoreApplication::processEvents();

    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::showTraceEqualization()
{
    if (!requireOpenFile()) return;

    if (m_traceEqualDlg) {
        m_traceEqualDlg->raise();
        m_traceEqualDlg->activateWindow();
        return;
    }

    m_traceEqualDlg = new QDialog(this);
    m_traceEqualDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_traceEqualDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_traceEqualDlg->setWindowTitle(QString("道间均衡-%1").arg(fi.completeBaseName()));

    connect(m_traceEqualDlg, &QDialog::finished, this, [this]() {
        m_traceEqualDlg = nullptr;
        m_traceEqualBtnApply = nullptr;
        m_traceEqualApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_traceEqualDlg);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_traceEqualBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_traceEqualBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_traceEqualBtnApply, &QPushButton::clicked, this, [this]() {
        applyTraceEqualization();
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_traceEqualApplied)
            applyTraceEqualization();
        if (m_traceEqualApplied)
            saveProcessedFile();
        if (m_traceEqualDlg)
            m_traceEqualDlg->close();
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_traceEqualApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_traceEqualApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_traceEqualDlg)
            m_traceEqualDlg->close();
    });

    m_traceEqualDlg->show();
}

void MainWindow::applyTraceEqualization()
{
    if (!requireOpenFile()) return;

    if (m_traceEqualApplied) {
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_traceEqualApplied = false;
        m_traceEqualBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    m_progressBar->setValue(0);
    m_progressBar->setFormat("道间均衡: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    const char *srcData = m_rawData.constData();
    std::vector<qint32> samples(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        samples[i] = (static_cast<quint8>(srcData[idx+3]) << 24) |
                     (static_cast<quint8>(srcData[idx+2]) << 16) |
                     (static_cast<quint8>(srcData[idx+1]) << 8) |
                     (static_cast<quint8>(srcData[idx]));
    }

    m_progressBar->setValue(10);
    m_progressBar->setFormat("道间均衡: 计算 RMS... %p%");
    QCoreApplication::processEvents();

    // 每条 trace 的 RMS 振幅；参考值为所有 trace RMS 的均值
    std::vector<double> rmsPerTrace(numTraces, 0.0);
    double rmsSum = 0.0;
    for (int t = 0; t < numTraces; ++t) {
        double sumSq = 0.0;
        const qint32 *tr = &samples[t * samplesPerTrace];
        for (int s = 0; s < samplesPerTrace; ++s) {
            double v = static_cast<double>(tr[s]);
            sumSq += v * v;
        }
        double rms = std::sqrt(sumSq / samplesPerTrace);
        rmsPerTrace[t] = rms;
        rmsSum += rms;
        if (t % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(10 + 30 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }
    double refRms = rmsSum / numTraces;
    if (refRms <= 0.0) refRms = 1.0;

    m_progressBar->setValue(40);
    m_progressBar->setFormat("道间均衡: 缩放 trace... %p%");
    QCoreApplication::processEvents();

    // 按 refRms/rms_t 缩放每条 trace；RMS 极小的 trace 保持原值
    for (int t = 0; t < numTraces; ++t) {
        double scale = (rmsPerTrace[t] > 1e-9) ? (refRms / rmsPerTrace[t]) : 1.0;
        qint32 *tr = &samples[t * samplesPerTrace];
        for (int s = 0; s < samplesPerTrace; ++s) {
            double scaled = static_cast<double>(tr[s]) * scale;
            tr[s] = static_cast<qint32>(std::round(scaled));
        }
        if (t % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(40 + 50 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }

    char *data = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        qint32 val = samples[i];
        data[idx]   = val & 0xFF;
        data[idx+1] = (val >> 8) & 0xFF;
        data[idx+2] = (val >> 16) & 0xFF;
        data[idx+3] = (val >> 24) & 0xFF;
    }

    m_currentTab->rawData = m_rawData;
    m_traceEqualApplied = true;
    m_traceEqualBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("道间均衡: 完成");
    QCoreApplication::processEvents();

    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::showMathOperation()
{
    if (!requireOpenFile()) return;

    if (m_mathDlg) {
        m_mathDlg->raise();
        m_mathDlg->activateWindow();
        return;
    }

    m_mathDlg = new QDialog(this);
    m_mathDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_mathDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_mathDlg->setWindowTitle(QString("数学运算-%1").arg(fi.completeBaseName()));

    connect(m_mathDlg, &QDialog::finished, this, [this]() {
        m_mathDlg = nullptr;
        m_mathOpTypeCombo = nullptr;
        m_mathNormalizeCombo = nullptr;
        m_mathBtnApply = nullptr;
        m_mathApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_mathDlg);

    // 参数设置 group
    QGroupBox *paramGroup = new QGroupBox("参数设置");
    QFormLayout *paramForm = new QFormLayout(paramGroup);
    m_mathOpTypeCombo = new QComboBox();
    m_mathOpTypeCombo->addItem("差分");
    m_mathOpTypeCombo->addItem("积分");
    m_mathOpTypeCombo->addItem("平方");
    m_mathOpTypeCombo->addItem("开方");
    m_mathOpTypeCombo->addItem("对数");
    m_mathOpTypeCombo->addItem("指数");
    m_mathOpTypeCombo->setCurrentIndex(2);  // 默认 平方
    paramForm->addRow("运算类型：", m_mathOpTypeCombo);

    m_mathNormalizeCombo = new QComboBox();
    m_mathNormalizeCombo->addItem("是");
    m_mathNormalizeCombo->addItem("否");
    m_mathNormalizeCombo->setCurrentIndex(0);  // 默认 是
    paramForm->addRow("是否归一化：", m_mathNormalizeCombo);
    mainLayout->addWidget(paramGroup);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_mathBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_mathBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_mathBtnApply, &QPushButton::clicked, this, [this]() {
        applyMathOperation();
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_mathApplied)
            applyMathOperation();
        if (m_mathApplied)
            saveProcessedFile();
        if (m_mathDlg)
            m_mathDlg->close();
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_mathApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_mathApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_mathDlg)
            m_mathDlg->close();
    });

    m_mathDlg->show();
}

void MainWindow::applyMathOperation()
{
    if (!requireOpenFile()) return;

    if (m_mathApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_mathApplied = false;
        m_mathBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    int opType = m_mathOpTypeCombo ? m_mathOpTypeCombo->currentIndex() : 2;
    bool normalize = (m_mathNormalizeCombo && m_mathNormalizeCombo->currentIndex() == 0);

    m_progressBar->setValue(0);
    m_progressBar->setFormat("数学运算: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    const char *srcData = m_rawData.constData();
    std::vector<qint32> samples(totalPixels);
    double srcMin = std::numeric_limits<double>::max();
    double srcMax = std::numeric_limits<double>::lowest();
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        qint32 v = (static_cast<quint8>(srcData[idx+3]) << 24) |
                   (static_cast<quint8>(srcData[idx+2]) << 16) |
                   (static_cast<quint8>(srcData[idx+1]) << 8) |
                   (static_cast<quint8>(srcData[idx]));
        samples[i] = v;
        double dv = static_cast<double>(v);
        if (dv < srcMin) srcMin = dv;
        if (dv > srcMax) srcMax = dv;
    }

    m_progressBar->setValue(10);
    m_progressBar->setFormat("数学运算: 计算中... %p%");
    QCoreApplication::processEvents();

    std::vector<double> out(totalPixels, 0.0);
    double outMin = std::numeric_limits<double>::max();
    double outMax = std::numeric_limits<double>::lowest();

    auto updateOutRange = [&](double v) {
        if (v < outMin) outMin = v;
        if (v > outMax) outMax = v;
    };

    for (int t = 0; t < numTraces; ++t) {
        double acc = 0.0;  // running sum for 积分
        double prev = static_cast<double>(samples[t * samplesPerTrace]);
        for (int s = 0; s < samplesPerTrace; ++s) {
            int i = t * samplesPerTrace + s;
            double v = static_cast<double>(samples[i]);
            double r = 0.0;
            switch (opType) {
                case 0: {  // 差分（中心差分；边界用单侧差分）
                    if (s == 0) {
                        double next = (samplesPerTrace > 1)
                            ? static_cast<double>(samples[i + 1]) : v;
                        r = next - v;
                    } else if (s == samplesPerTrace - 1) {
                        r = v - prev;
                    } else {
                        double next = static_cast<double>(samples[i + 1]);
                        r = (next - prev) * 0.5;
                    }
                    break;
                }
                case 1: {  // 积分（沿 s 方向累加）
                    acc += v;
                    r = acc;
                    break;
                }
                case 2: {  // 平方
                    r = v * v;
                    break;
                }
                case 3: {  // 开方（保留符号）
                    r = (v >= 0.0) ? std::sqrt(v) : -std::sqrt(-v);
                    break;
                }
                case 4: {  // 对数（自然对数；保留符号，幅值取绝对值）
                    double a = std::abs(v);
                    r = (a > 1e-9) ? ((v >= 0.0) ? std::log(a) : -std::log(a)) : 0.0;
                    break;
                }
                case 5: {  // 指数（保留符号；为避免数值爆炸用 exp(v/scale)）
                    // 用 srcMax 把指数范围限制在合理量级
                    double base = (srcMax > 1.0) ? srcMax : 1.0;
                    double ex = v / base;
                    if (ex > 50.0) ex = 50.0;
                    if (ex < -50.0) ex = -50.0;
                    r = (v >= 0.0) ? std::exp(ex) : -std::exp(ex);
                    break;
                }
                default:
                    r = v;
                    break;
            }
            out[i] = r;
            updateOutRange(r);
            prev = v;
        }
        if (t % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(10 + 70 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }

    // 归一化到原始 [srcMin, srcMax] 范围；若选"否"，则按 qint32 范围裁剪
    double outRange = outMax - outMin;
    double srcRange = srcMax - srcMin;
    if (outRange < 1e-9) outRange = 1.0;
    if (srcRange < 1e-9) srcRange = 1.0;

    char *data = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        double v = out[i];
        if (normalize) {
            // 线性映射到 [srcMin, srcMax]
            double norm = (v - outMin) / outRange;  // 0..1
            v = srcMin + norm * srcRange;
        }
        // qint32 范围裁剪
        if (v > 2147483647.0) v = 2147483647.0;
        if (v < -2147483648.0) v = -2147483648.0;
        qint32 iv = static_cast<qint32>(std::round(v));
        int idx = i * 4;
        data[idx]   = iv & 0xFF;
        data[idx+1] = (iv >> 8) & 0xFF;
        data[idx+2] = (iv >> 16) & 0xFF;
        data[idx+3] = (iv >> 24) & 0xFF;
    }

    m_currentTab->rawData = m_rawData;
    m_mathApplied = true;
    m_mathBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("数学运算: 完成");
    QCoreApplication::processEvents();

    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::pushKirchhoffParamsToImage()
{
    if (!imageLabel || !m_kirchhoffFirstWaveSpin) return;
    double firstWave = m_kirchhoffFirstWaveSpin->value();
    double vel = m_kirchhoffVelocitySpin->value();
    int width = m_kirchhoffWidthSpin->value();
    double spacingCm = m_kirchhoffSpacingSpin->value();
    double tps = (m_pixelsPerRow > 0 && m_timeRange > 0) ? (m_timeRange / m_pixelsPerRow) : 0.039;
    imageLabel->setHyperbolaParams(firstWave, vel, width, spacingCm * 0.01, tps);
}

void MainWindow::showKirchhoffMigration()
{
    if (!requireOpenFile()) return;

    if (m_kirchhoffDlg) {
        m_kirchhoffDlg->raise();
        m_kirchhoffDlg->activateWindow();
        return;
    }

    m_kirchhoffDlg = new QDialog(this);
    m_kirchhoffDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_kirchhoffDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_kirchhoffDlg->setWindowTitle(QString("克西霍夫-%1").arg(fi.completeBaseName()));

    connect(m_kirchhoffDlg, &QDialog::finished, this, [this]() {
        m_kirchhoffDlg = nullptr;
        m_kirchhoffFirstWaveSpin = nullptr;
        m_kirchhoffVelocitySpin = nullptr;
        m_kirchhoffWidthSpin = nullptr;
        m_kirchhoffSpacingSpin = nullptr;
        m_kirchhoffBtnApply = nullptr;
        m_kirchhoffApplied = false;
        if (imageLabel) imageLabel->setHyperbolaTracking(false);
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_kirchhoffDlg);

    // 参数设置 group
    QGroupBox *paramGroup = new QGroupBox("参数设置");
    QFormLayout *paramForm = new QFormLayout(paramGroup);

    m_kirchhoffFirstWaveSpin = new QDoubleSpinBox();
    m_kirchhoffFirstWaveSpin->setRange(0, 511);
    m_kirchhoffFirstWaveSpin->setDecimals(0);
    m_kirchhoffFirstWaveSpin->setValue(27);
    paramForm->addRow("首波位置：", m_kirchhoffFirstWaveSpin);

    m_kirchhoffVelocitySpin = new QDoubleSpinBox();
    m_kirchhoffVelocitySpin->setRange(0.001, 1.0);
    m_kirchhoffVelocitySpin->setDecimals(3);
    m_kirchhoffVelocitySpin->setSuffix(" m/ns");
    m_kirchhoffVelocitySpin->setValue(0.106);
    paramForm->addRow("波 速：", m_kirchhoffVelocitySpin);

    m_kirchhoffWidthSpin = new QSpinBox();
    m_kirchhoffWidthSpin->setRange(2, 9999);
    m_kirchhoffWidthSpin->setValue(60);
    paramForm->addRow("双曲线宽：", m_kirchhoffWidthSpin);

    m_kirchhoffSpacingSpin = new QDoubleSpinBox();
    m_kirchhoffSpacingSpin->setRange(0.001, 1000.0);
    m_kirchhoffSpacingSpin->setDecimals(3);
    m_kirchhoffSpacingSpin->setSuffix(" cm");
    m_kirchhoffSpacingSpin->setValue(1.0);
    paramForm->addRow("道间距离：", m_kirchhoffSpacingSpin);
    mainLayout->addWidget(paramGroup);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_kirchhoffBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_kirchhoffBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // 参数变化时即时更新图片上的绿色双曲线
    auto onParamChanged = [this]() {
        pushKirchhoffParamsToImage();
    };
    connect(m_kirchhoffFirstWaveSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onParamChanged);
    connect(m_kirchhoffVelocitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onParamChanged);
    connect(m_kirchhoffWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onParamChanged);
    connect(m_kirchhoffSpacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onParamChanged);

    connect(m_kirchhoffBtnApply, &QPushButton::clicked, this, [this]() {
        applyKirchhoffMigration();
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_kirchhoffApplied)
            applyKirchhoffMigration();
        if (m_kirchhoffApplied)
            saveProcessedFile();
        if (m_kirchhoffDlg)
            m_kirchhoffDlg->close();
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_kirchhoffApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_kirchhoffApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_kirchhoffDlg)
            m_kirchhoffDlg->close();
    });

    // 启用图片上的绿色双曲线交互
    pushKirchhoffParamsToImage();
    if (imageLabel) imageLabel->setHyperbolaTracking(true);

    m_kirchhoffDlg->show();
}

void MainWindow::applyKirchhoffMigration()
{
    if (!requireOpenFile()) return;

    if (m_kirchhoffApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_kirchhoffApplied = false;
        m_kirchhoffBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    double firstWave = m_kirchhoffFirstWaveSpin ? m_kirchhoffFirstWaveSpin->value() : 27.0;
    double vel = m_kirchhoffVelocitySpin ? m_kirchhoffVelocitySpin->value() : 0.106;       // m/ns
    int aperture = m_kirchhoffWidthSpin ? m_kirchhoffWidthSpin->value() : 60;              // traces
    double spacingCm = m_kirchhoffSpacingSpin ? m_kirchhoffSpacingSpin->value() : 1.0;
    double dxM = spacingCm * 0.01;                                                          // m
    double tps = (m_timeRange > 0) ? (m_timeRange / samplesPerTrace) : 0.039;               // ns/sample
    if (vel < 1e-6 || dxM < 1e-9 || tps < 1e-12) return;

    m_progressBar->setValue(0);
    m_progressBar->setFormat("克西霍夫: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    const char *srcData = m_rawData.constData();
    std::vector<double> samples(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        qint32 v = (static_cast<quint8>(srcData[idx+3]) << 24) |
                   (static_cast<quint8>(srcData[idx+2]) << 16) |
                   (static_cast<quint8>(srcData[idx+1]) << 8) |
                   (static_cast<quint8>(srcData[idx]));
        samples[i] = static_cast<double>(v);
    }

    // DC 去除：减去输入中位数（仪器偏移会使深部数据整体偏置 > 0，
    // 算术平均保留该偏置导致深部灰偏白）。中位数对绕射峰鲁棒。
    {
        std::vector<double> tmp(samples);
        std::nth_element(tmp.begin(), tmp.begin() + totalPixels / 2, tmp.end());
        double inMedian = tmp[totalPixels / 2];
        for (int i = 0; i < totalPixels; ++i) samples[i] -= inMedian;
    }

    m_progressBar->setValue(10);
    m_progressBar->setFormat("克西霍夫: 偏移求和... %p%");
    QCoreApplication::processEvents();

    int halfW = aperture / 2;
    if (halfW < 1) halfW = 1;
    std::vector<double> out(totalPixels, 0.0);

    // Kirchhoff 时间偏移：动态孔径 + Yilmaz 标准权重 (t0/t)^(3/2)
    //   τ(x_in) = sqrt(τ0^2 + (dx/v_mig)^2)
    //   动态孔径: dx ≤ z·tan(θ_max), θ_max=45°
    //     浅层 z 小 → 孔径自动收缩到 1 道 → 浅层不受远道反向极性抵消
    //     深部 z 大 → 孔径自动放大(上限 halfW) → 多道参与聚焦绕射
    //   权重 w = (t0/t)^(3/2) = cos(θ)·sqrt(t0/t) (倾斜因子×反混叠因子)
    //     远道 t 大 → w 小 → 软截断远道"拖尾"
    //   归一化 sum/wsum 保留相干绕射峰振幅，输入已 ±2^23 满量程不后处理缩放
    const double thetaMax = 45.0 * M_PI / 180.0;
    const double tanThetaMax = std::tan(thetaMax);  // ≈ 1.0
    for (int xOut = 0; xOut < numTraces; ++xOut) {
        for (int sOut = 0; sOut < samplesPerTrace; ++sOut) {
            int outIdx = xOut * samplesPerTrace + sOut;
            double tau0 = (sOut - firstWave) * tps;
            if (tau0 <= 0.0) {
                out[outIdx] = samples[outIdx];
                continue;
            }
            double zM = vel * tau0 / 2.0;
            double dxMaxM = zM * tanThetaMax;
            int halfWDyn = (dxM > 1e-9) ? (static_cast<int>(dxMaxM / dxM) + 1) : halfW;
            if (halfWDyn < 1) halfWDyn = 1;
            if (halfWDyn > halfW) halfWDyn = halfW;
            int xMin = std::max(0, xOut - halfWDyn);
            int xMax = std::min(numTraces - 1, xOut + halfWDyn);

            double sum = 0.0;
            double wsum = 0.0;
            for (int xIn = xMin; xIn <= xMax; ++xIn) {
                double dx = (xIn - xOut) * dxM;
                double arg = dx / vel;
                double tauIn = std::sqrt(tau0 * tau0 + arg * arg);  // ns
                double sInF = firstWave + tauIn / tps;
                if (sInF < 0.0 || sInF > samplesPerTrace - 1.001) continue;
                int sI0 = static_cast<int>(sInF);
                double frac = sInF - sI0;
                double v0 = samples[xIn * samplesPerTrace + sI0];
                double v1 = samples[xIn * samplesPerTrace + sI0 + 1];
                double val = v0 + (v1 - v0) * frac;
                double w = std::pow(tau0 / tauIn, 1.5);  // (t0/t)^(3/2)
                sum += val * w;
                wsum += w;
            }
            out[outIdx] = (wsum > 1e-12) ? (sum / wsum) : 0.0;
        }
        if (xOut % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(10 + 80 * xOut / numTraces);
            QCoreApplication::processEvents();
        }
    }

    // 无后处理缩放：输出已在 ±2^23 范围内，直接写入（仅做 qint32 截断保护）
    char *data = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        double v = out[i];
        if (v > 8388607.0) v = 8388607.0;
        if (v < -8388608.0) v = -8388608.0;
        qint32 iv = static_cast<qint32>(std::round(v));
        int idx = i * 4;
        data[idx]   = iv & 0xFF;
        data[idx+1] = (iv >> 8) & 0xFF;
        data[idx+2] = (iv >> 16) & 0xFF;
        data[idx+3] = (iv >> 24) & 0xFF;
    }

    m_currentTab->rawData = m_rawData;
    m_kirchhoffApplied = true;
    m_kirchhoffBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("克西霍夫: 完成");
    QCoreApplication::processEvents();

    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::showHilbertTransform()
{
    if (!requireOpenFile()) return;

    if (m_hilbertDlg) {
        m_hilbertDlg->raise();
        m_hilbertDlg->activateWindow();
        return;
    }

    m_hilbertDlg = new QDialog(this);
    m_hilbertDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_hilbertDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_hilbertDlg->setWindowTitle(QString("希尔伯特-%1").arg(fi.completeBaseName()));

    connect(m_hilbertDlg, &QDialog::finished, this, [this]() {
        m_hilbertDlg = nullptr;
        m_hilbertTypeCombo = nullptr;
        m_hilbertBtnApply = nullptr;
        m_hilbertApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_hilbertDlg);

    // 参数设置 group
    QGroupBox *paramGroup = new QGroupBox("参数设置");
    QFormLayout *paramForm = new QFormLayout(paramGroup);
    m_hilbertTypeCombo = new QComboBox();
    m_hilbertTypeCombo->addItem("瞬时振幅");
    m_hilbertTypeCombo->addItem("瞬时频率");
    m_hilbertTypeCombo->addItem("瞬时相位");
    m_hilbertTypeCombo->setCurrentIndex(0);  // 默认 瞬时振幅
    paramForm->addRow("变换类型：", m_hilbertTypeCombo);
    mainLayout->addWidget(paramGroup);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_hilbertBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_hilbertBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_hilbertBtnApply, &QPushButton::clicked, this, [this]() {
        applyHilbertTransform();
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_hilbertApplied)
            applyHilbertTransform();
        if (m_hilbertApplied)
            saveProcessedFile();
        if (m_hilbertDlg)
            m_hilbertDlg->close();
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_hilbertApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_hilbertApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_hilbertDlg)
            m_hilbertDlg->close();
    });

    m_hilbertDlg->show();
}

void MainWindow::applyHilbertTransform()
{
    if (!requireOpenFile()) return;

    if (m_hilbertApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_hilbertApplied = false;
        m_hilbertBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    int opType = m_hilbertTypeCombo ? m_hilbertTypeCombo->currentIndex() : 0;

    m_progressBar->setValue(0);
    m_progressBar->setFormat("希尔伯特: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    const char *srcData = m_rawData.constData();
    std::vector<double> samples(totalPixels);
    double srcMin = std::numeric_limits<double>::max();
    double srcMax = std::numeric_limits<double>::lowest();
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        qint32 v = (static_cast<quint8>(srcData[idx+3]) << 24) |
                   (static_cast<quint8>(srcData[idx+2]) << 16) |
                   (static_cast<quint8>(srcData[idx+1]) << 8) |
                   (static_cast<quint8>(srcData[idx]));
        samples[i] = static_cast<double>(v);
        if (samples[i] < srcMin) srcMin = samples[i];
        if (samples[i] > srcMax) srcMax = samples[i];
    }

    m_progressBar->setValue(10);
    m_progressBar->setFormat("希尔伯特: 计算解析信号... %p%");
    QCoreApplication::processEvents();

    // FFT 长度：取 ≥ samplesPerTrace 的最小 2 的幂
    int fftN = 1;
    while (fftN < samplesPerTrace) fftN <<= 1;

    std::vector<double> out(totalPixels, 0.0);
    std::vector<std::complex<double>> X(fftN);

    for (int t = 0; t < numTraces; ++t) {
        const double *tr = &samples[t * samplesPerTrace];

        // 装载到复数缓冲（补零到 fftN）
        for (int n = 0; n < fftN; ++n) {
            X[n] = (n < samplesPerTrace) ? std::complex<double>(tr[n], 0.0)
                                         : std::complex<double>(0.0, 0.0);
        }
        fft(X);

        // 构造解析信号谱：
        //   Z[0] = X[0], Z[fftN/2] = X[fftN/2]
        //   Z[k] = 2*X[k]            for k = 1..fftN/2-1  (正频)
        //   Z[k] = 0                 for k = fftN/2+1..fftN-1  (负频置零)
        X[0] *= 1.0;
        if (fftN > 1) {
            for (int k = 1; k < fftN / 2; ++k) X[k] *= 2.0;
            for (int k = fftN / 2 + 1; k < fftN; ++k) X[k] = std::complex<double>(0.0, 0.0);
        }

        ifft(X);  // 解析信号 z[n]

        if (opType == 0) {
            // 瞬时振幅: |z[n]|
            for (int n = 0; n < samplesPerTrace; ++n) {
                out[t * samplesPerTrace + n] = std::abs(X[n]);
            }
        } else if (opType == 1) {
            // 瞬时频率: 解卷绕相位后中心差分，单位归一化到 [0, 0.5] cycle/sample
            std::vector<double> phase(samplesPerTrace);
            for (int n = 0; n < samplesPerTrace; ++n) phase[n] = std::arg(X[n]);
            // 解卷绕
            for (int n = 1; n < samplesPerTrace; ++n) {
                double d = phase[n] - phase[n - 1];
                while (d > M_PI) d -= 2.0 * M_PI;
                while (d < -M_PI) d += 2.0 * M_PI;
                phase[n] = phase[n - 1] + d;
            }
            // 中心差分 -> 瞬时频率 (rad/sample) / (2π) -> cycle/sample
            for (int n = 0; n < samplesPerTrace; ++n) {
                double pm = (n > 0) ? phase[n - 1] : phase[n];
                double pp = (n < samplesPerTrace - 1) ? phase[n + 1] : phase[n];
                double freq = (pp - pm) / (4.0 * M_PI);  // (1/2)*(dφ/2)/(2π) = (dφ/2π)/2
                if (freq < 0.0) freq = 0.0;
                if (freq > 0.5) freq = 0.5;
                out[t * samplesPerTrace + n] = freq * 1000.0;  // 放大便于显示
            }
        } else {
            // 瞬时相位: arg(z[n])，弧度，缩放到 qint32 友好范围
            for (int n = 0; n < samplesPerTrace; ++n) {
                out[t * samplesPerTrace + n] = std::arg(X[n]) * 1000.0;  // -π..π -> -3141..3141
            }
        }

        if (t % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(10 + 80 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }

    // 归一化回原始 [srcMin, srcMax] 范围
    double outMin = std::numeric_limits<double>::max();
    double outMax = std::numeric_limits<double>::lowest();
    for (int i = 0; i < totalPixels; ++i) {
        if (out[i] < outMin) outMin = out[i];
        if (out[i] > outMax) outMax = out[i];
    }
    double outRange = outMax - outMin;
    double srcRange = srcMax - srcMin;
    if (outRange < 1e-9) outRange = 1.0;
    if (srcRange < 1e-9) srcRange = 1.0;

    char *data = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        double norm = (out[i] - outMin) / outRange;
        double v = srcMin + norm * srcRange;
        if (v > 2147483647.0) v = 2147483647.0;
        if (v < -2147483648.0) v = -2147483648.0;
        qint32 iv = static_cast<qint32>(std::round(v));
        int idx = i * 4;
        data[idx]   = iv & 0xFF;
        data[idx+1] = (iv >> 8) & 0xFF;
        data[idx+2] = (iv >> 16) & 0xFF;
        data[idx+3] = (iv >> 24) & 0xFF;
    }

    m_currentTab->rawData = m_rawData;
    m_hilbertApplied = true;
    m_hilbertBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("希尔伯特: 完成");
    QCoreApplication::processEvents();

    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::showDeconvolution()
{
    if (!requireOpenFile()) return;

    if (m_deconvDlg) {
        m_deconvDlg->raise();
        m_deconvDlg->activateWindow();
        return;
    }

    m_deconvDlg = new QDialog(this);
    m_deconvDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_deconvDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_deconvDlg->setWindowTitle(QString("反褶积-%1").arg(fi.completeBaseName()));

    connect(m_deconvDlg, &QDialog::finished, this, [this]() {
        m_deconvDlg = nullptr;
        m_deconvFilterLenSpin = nullptr;
        m_deconvPredStepSpin = nullptr;
        m_deconvBtnApply = nullptr;
        m_deconvApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_deconvDlg);

    // 参数设置 group
    QGroupBox *paramGroup = new QGroupBox("参数设置");
    QFormLayout *paramForm = new QFormLayout(paramGroup);
    m_deconvFilterLenSpin = new QSpinBox();
    m_deconvFilterLenSpin->setRange(3, 999);
    m_deconvFilterLenSpin->setValue(31);
    paramForm->addRow("滤波器长：", m_deconvFilterLenSpin);

    m_deconvPredStepSpin = new QSpinBox();
    m_deconvPredStepSpin->setRange(1, 999);
    m_deconvPredStepSpin->setValue(5);
    paramForm->addRow("预测步长：", m_deconvPredStepSpin);
    mainLayout->addWidget(paramGroup);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_deconvBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_deconvBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_deconvBtnApply, &QPushButton::clicked, this, [this]() {
        applyDeconvolution();
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_deconvApplied)
            applyDeconvolution();
        if (m_deconvApplied)
            saveProcessedFile();
        if (m_deconvDlg)
            m_deconvDlg->close();
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_deconvApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_deconvApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_deconvDlg)
            m_deconvDlg->close();
    });

    m_deconvDlg->show();
}

void MainWindow::applyDeconvolution()
{
    if (!requireOpenFile()) return;

    if (m_deconvApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = false;
        m_deconvApplied = false;
        m_deconvBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    int filterLen = m_deconvFilterLenSpin ? m_deconvFilterLenSpin->value() : 31;
    int predStep = m_deconvPredStepSpin ? m_deconvPredStepSpin->value() : 5;
    if (filterLen < 1) filterLen = 1;
    if (predStep < 1) predStep = 1;

    m_progressBar->setValue(0);
    m_progressBar->setFormat("反褶积: 读取数据...");
    m_progressBar->show();
    QCoreApplication::processEvents();

    const char *srcData = m_rawData.constData();
    std::vector<double> samples(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        int idx = i * 4;
        qint32 v = (static_cast<quint8>(srcData[idx+3]) << 24) |
                   (static_cast<quint8>(srcData[idx+2]) << 16) |
                   (static_cast<quint8>(srcData[idx+1]) << 8) |
                   (static_cast<quint8>(srcData[idx]));
        samples[i] = static_cast<double>(v);
    }

    // 计算 input RMS（用于输出幅度匹配）
    double inSqSum = 0.0;
    for (int i = 0; i < totalPixels; ++i) inSqSum += samples[i] * samples[i];
    double inRms = std::sqrt(inSqSum / totalPixels);

    m_progressBar->setValue(10);
    m_progressBar->setFormat("反褶积: 全局自相关... %p%");
    QCoreApplication::processEvents();

    // RADAN 风格 Wiener 预测反褶积
    // 关键改进：使用所有 trace 平均的"全局自相关"求一个公共滤波器，
    // 而非每条 trace 各自求——后者会导致横向不一致 + 弱信号 trace 噪声放大。
    //   1) 全局自相关 r[k] = mean_t( E[x_t[n] x_t[n+k]] )
    //   2) 解 Wiener-Hopf 方程 R f = g, R[i][j] = r[|i-j|], g[i] = r[predStep+i]
    //   3) 残差输出 e[n] = x[n] - sum_{k=0}^{L-1} f[k] * x[n - predStep - k]
    int acorrLen = filterLen + predStep;
    std::vector<double> acorr(acorrLen, 0.0);
    for (int t = 0; t < numTraces; ++t) {
        double *tr = &samples[t * samplesPerTrace];
        for (int k = 0; k < acorrLen; ++k) {
            double sum = 0.0;
            int count = samplesPerTrace - k;
            for (int n = 0; n < count; ++n) {
                sum += tr[n] * tr[n + k];
            }
            acorr[k] += (count > 0) ? sum / count : 0.0;
        }
        if (t % qMax(1, numTraces / 10) == 0) {
            m_progressBar->setValue(10 + 20 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }
    for (int k = 0; k < acorrLen; ++k) acorr[k] /= numTraces;

    // 白噪声加成 0.1%，保证 Toeplitz 矩阵正定可解
    if (acorr[0] > 0.0) acorr[0] *= (1.0 + 1e-3);

    m_progressBar->setValue(35);
    m_progressBar->setFormat("反褶积: Wiener 滤波器... %p%");
    QCoreApplication::processEvents();

    // 构造增广矩阵 [R | g]
    std::vector<std::vector<double>> A(filterLen, std::vector<double>(filterLen + 1, 0.0));
    for (int i = 0; i < filterLen; ++i) {
        for (int j = 0; j < filterLen; ++j) {
            A[i][j] = acorr[std::abs(i - j)];
        }
        A[i][filterLen] = acorr[predStep + i];
    }

    // 高斯消元 (部分主元)
    for (int p = 0; p < filterLen; ++p) {
        int maxRow = p;
        for (int r = p + 1; r < filterLen; ++r) {
            if (std::abs(A[r][p]) > std::abs(A[maxRow][p])) maxRow = r;
        }
        if (maxRow != p) std::swap(A[p], A[maxRow]);

        if (std::abs(A[p][p]) < 1e-18) continue;

        for (int r = p + 1; r < filterLen; ++r) {
            double f = A[r][p] / A[p][p];
            if (f == 0.0) continue;
            for (int c = p; c <= filterLen; ++c) {
                A[r][c] -= f * A[p][c];
            }
        }
    }

    // 回代
    std::vector<double> filter(filterLen, 0.0);
    for (int i = filterLen - 1; i >= 0; --i) {
        double s = A[i][filterLen];
        for (int j = i + 1; j < filterLen; ++j) {
            s -= A[i][j] * filter[j];
        }
        filter[i] = (std::abs(A[i][i]) > 1e-18) ? s / A[i][i] : 0.0;
    }

    m_progressBar->setValue(45);
    m_progressBar->setFormat("反褶积: 应用预测误差... %p%");
    QCoreApplication::processEvents();

    // 应用全局 filter 到所有 trace
    std::vector<double> out(totalPixels, 0.0);
    for (int t = 0; t < numTraces; ++t) {
        double *tr = &samples[t * samplesPerTrace];
        for (int n = 0; n < samplesPerTrace; ++n) {
            double pred = 0.0;
            for (int k = 0; k < filterLen; ++k) {
                int idx = n - predStep - k;
                if (idx >= 0) pred += filter[k] * tr[idx];
            }
            out[t * samplesPerTrace + n] = tr[n] - pred;
        }
        if (t % qMax(1, numTraces / 20) == 0) {
            m_progressBar->setValue(45 + 45 * t / numTraces);
            QCoreApplication::processEvents();
        }
    }

    // 输出幅度匹配：scale = input_RMS / output_RMS
    // 线性 min/max 归一化会把残差噪声拉伸到满量程，破坏相对幅度并放大噪声；
    // RMS 匹配保留弱/强反射之间的相对能量结构，整体亮度与原图一致。
    double outSqSum = 0.0;
    for (int i = 0; i < totalPixels; ++i) outSqSum += out[i] * out[i];
    double outRms = std::sqrt(outSqSum / totalPixels);
    double scale = (outRms > 1e-9) ? inRms / outRms : 1.0;

    char *data = m_rawData.data();
    for (int i = 0; i < totalPixels; ++i) {
        double v = out[i] * scale;
        // 显示 LUT 范围 ±2^23，钳位防止溢出
        if (v > 8388607.0) v = 8388607.0;
        if (v < -8388608.0) v = -8388608.0;
        qint32 iv = static_cast<qint32>(std::round(v));
        int idx = i * 4;
        data[idx]   = iv & 0xFF;
        data[idx+1] = (iv >> 8) & 0xFF;
        data[idx+2] = (iv >> 16) & 0xFF;
        data[idx+3] = (iv >> 24) & 0xFF;
    }

    m_currentTab->rawData = m_rawData;
    m_deconvApplied = true;
    m_deconvBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }

    m_progressBar->setValue(100);
    m_progressBar->setFormat("反褶积: 完成");
    QCoreApplication::processEvents();

    QTimer::singleShot(2000, this, [this]() {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    });
}

void MainWindow::showCorrectOffset()
{
    if (!requireOpenFile()) return;

    if (m_correctOffsetDlg) {
        m_correctOffsetDlg->raise();
        m_correctOffsetDlg->activateWindow();
        return;
    }

    m_correctOffsetDlg = new QDialog(this);
    m_correctOffsetDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_correctOffsetDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);
    QFileInfo fi(m_currentTab->filePath);
    m_correctOffsetDlg->setWindowTitle(QString("校正零偏-%1").arg(fi.completeBaseName()));

    connect(m_correctOffsetDlg, &QDialog::finished, this, [this]() {
        m_correctOffsetDlg = nullptr;
        m_correctTimeWindowSpin = nullptr;
        m_correctAntennaFreqSpin = nullptr;
        m_correctBtnApply = nullptr;
        m_correctApplied = false;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(m_correctOffsetDlg);

    // Row 1: 时窗
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->addWidget(new QLabel("时窗："));
    m_correctTimeWindowSpin = new QDoubleSpinBox();
    m_correctTimeWindowSpin->setRange(0.0, 99999.0);
    m_correctTimeWindowSpin->setDecimals(1);
    m_correctTimeWindowSpin->setValue(40.0);
    m_correctTimeWindowSpin->setSuffix(" ns");
    row1->addWidget(m_correctTimeWindowSpin);
    row1->addStretch();
    mainLayout->addLayout(row1);

    // Row 2: 天线频率
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->addWidget(new QLabel("天线频率："));
    m_correctAntennaFreqSpin = new QDoubleSpinBox();
    m_correctAntennaFreqSpin->setRange(1.0, 99999.0);
    m_correctAntennaFreqSpin->setDecimals(0);
    m_correctAntennaFreqSpin->setValue(900.0);
    m_correctAntennaFreqSpin->setSuffix(" MHz");
    row2->addWidget(m_correctAntennaFreqSpin);
    row2->addStretch();
    mainLayout->addLayout(row2);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_correctBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_correctBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    // Apply button
    connect(m_correctBtnApply, &QPushButton::clicked, this, [this]() {
        applyCorrectOffset();
    });

    // OK button
    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!m_correctApplied)
            applyCorrectOffset();
        if (m_correctApplied)
            saveProcessedFile();
        if (m_correctOffsetDlg)
            m_correctOffsetDlg->close();
    });

    // Cancel button
    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_correctApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_correctApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_correctOffsetDlg)
            m_correctOffsetDlg->close();
    });

    m_correctOffsetDlg->show();
}

void MainWindow::applyCorrectOffset()
{
    if (!requireOpenFile()) return;

    if (m_correctApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_correctApplied = false;
        m_correctBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    // Backup original data
    m_currentTab->originalRawData = m_rawData;

    // Dewow: sliding window mean removal per trace
    double timeWindowNs = m_correctTimeWindowSpin ? m_correctTimeWindowSpin->value() : 40.0;
    double antennaFreqMHz = m_correctAntennaFreqSpin ? m_correctAntennaFreqSpin->value() : 900.0;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    double timeRangeSec = m_currentTab->timeRange * 1e-9;
    double sampleInterval = timeRangeSec / samplesPerTrace;
    int windowSamples = qMax(1, static_cast<int>(timeWindowNs * 1e-9 / sampleInterval));
    int halfWin = windowSamples / 2;

    char *data = m_rawData.data();
    for (int t = 0; t < numTraces; ++t) {
        QVector<double> trace(samplesPerTrace);
        for (int s = 0; s < samplesPerTrace; ++s) {
            int idx = (t * samplesPerTrace + s) * 4;
            trace[s] = static_cast<double>(
                (static_cast<quint8>(data[idx+3]) << 24) |
                (static_cast<quint8>(data[idx+2]) << 16) |
                (static_cast<quint8>(data[idx+1]) << 8) |
                static_cast<quint8>(data[idx]));
        }
        QVector<double> cumsum(samplesPerTrace + 1, 0.0);
        for (int s = 0; s < samplesPerTrace; ++s)
            cumsum[s + 1] = cumsum[s] + trace[s];
        for (int s = 0; s < samplesPerTrace; ++s) {
            int left = qMax(0, s - halfWin);
            int right = qMin(samplesPerTrace - 1, s + halfWin);
            double localMean = (cumsum[right + 1] - cumsum[left]) / (right - left + 1);
            qint32 val = static_cast<qint32>(trace[s] - localMean);
            int idx = (t * samplesPerTrace + s) * 4;
            data[idx]   = val & 0xFF;
            data[idx+1] = (val >> 8) & 0xFF;
            data[idx+2] = (val >> 16) & 0xFF;
            data[idx+3] = (val >> 24) & 0xFF;
        }
    }

    m_currentTab->rawData = m_rawData;
    m_correctApplied = true;
    m_correctBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    // Sync one-click dialog reference chart
    if (m_oneClickDlg && m_oneClickDlg->isVisible()) {
        updateOneClickRefChart();
    }
}

void MainWindow::showOneClickProcess()
{
    if (!requireOpenFile()) return;

    if (m_oneClickDlg) {
        m_oneClickDlg->raise();
        m_oneClickDlg->activateWindow();
        updateOneClickRefChart();
        return;
    }

    m_oneClickDlg = new QDialog(this);
    m_oneClickDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_oneClickDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);
    QString fname = QFileInfo(m_currentTab->filePath).fileName();
    m_oneClickDlg->setWindowTitle(QString("一键处理 - %1").arg(fname));
    m_oneClickDlg->resize(800, 500);

    connect(m_oneClickDlg, &QDialog::finished, this, [this]() {
        m_oneClickDlg = nullptr;
        m_oneClickCorrectOffset = nullptr;
        m_oneClickAmpComp = nullptr;
        m_oneClickAdjZero = nullptr;
        m_oneClickAdjGain = nullptr;
        m_oneClickDigFilter = nullptr;
        m_oneClickBgRemove = nullptr;
        m_oneClickSmooth = nullptr;
        m_oneClickTimeWindowSpin = nullptr;
        m_oneClickAntennaFreqSpin = nullptr;
        m_oneClickAmpCompSpin = nullptr;
        m_oneClickZeroValueSpin = nullptr;
        m_oneClickBgWindowSpin = nullptr;
        m_oneClickSmoothWindowSpin = nullptr;
        m_oneClickBtnApply = nullptr;
        m_oneClickChart = nullptr;
        m_oneClickSeries = nullptr;
        m_oneClickChartView = nullptr;
        m_oneClickApplied = false;
    });

    QHBoxLayout *mainLayout = new QHBoxLayout(m_oneClickDlg);

    // === Left panel: 处理方法 ===
    QGroupBox *methodGroup = new QGroupBox("处理方法");
    QVBoxLayout *methodLayout = new QVBoxLayout(methodGroup);

    // 1. 校正零偏
    QHBoxLayout *row1 = new QHBoxLayout();
    m_oneClickCorrectOffset = new QCheckBox("校正零偏");
    m_oneClickCorrectOffset->setChecked(true);
    m_oneClickCorrectOffset->setMinimumWidth(80);
    row1->addWidget(m_oneClickCorrectOffset);
    QLabel *lblTimeWin = new QLabel("时窗:");
    lblTimeWin->setMinimumWidth(55);
    lblTimeWin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row1->addWidget(lblTimeWin);
    m_oneClickTimeWindowSpin = new QDoubleSpinBox();
    m_oneClickTimeWindowSpin->setRange(0.0, 99999.0);
    m_oneClickTimeWindowSpin->setDecimals(1);
    m_oneClickTimeWindowSpin->setValue(5.0);
    m_oneClickTimeWindowSpin->setSuffix(" ns");
    row1->addWidget(m_oneClickTimeWindowSpin);
    methodLayout->addLayout(row1);

    QHBoxLayout *row1b = new QHBoxLayout();
    row1b->addSpacing(80); // align with checkbox width
    QLabel *lblAntFreq = new QLabel("天线频率:");
    lblAntFreq->setMinimumWidth(55);
    lblAntFreq->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row1b->addWidget(lblAntFreq);
    m_oneClickAntennaFreqSpin = new QDoubleSpinBox();
    m_oneClickAntennaFreqSpin->setRange(1.0, 99999.0);
    m_oneClickAntennaFreqSpin->setDecimals(0);
    m_oneClickAntennaFreqSpin->setValue(900.0);
    m_oneClickAntennaFreqSpin->setSuffix(" MHz");
    row1b->addWidget(m_oneClickAntennaFreqSpin);
    methodLayout->addLayout(row1b);

    // 2. 幅度补偿
    QHBoxLayout *row2 = new QHBoxLayout();
    m_oneClickAmpComp = new QCheckBox("幅度补偿");
    m_oneClickAmpComp->setChecked(false);
    row2->addWidget(m_oneClickAmpComp);
    row2->addWidget(new QLabel("值:"));
    m_oneClickAmpCompSpin = new QSpinBox();
    m_oneClickAmpCompSpin->setRange(0, 100);
    m_oneClickAmpCompSpin->setValue(100);
    row2->addWidget(m_oneClickAmpCompSpin);
    row2->addStretch();
    methodLayout->addLayout(row2);

    // 3. 调节零点
    QHBoxLayout *row3 = new QHBoxLayout();
    m_oneClickAdjZero = new QCheckBox("调节零点");
    m_oneClickAdjZero->setChecked(false);
    row3->addWidget(m_oneClickAdjZero);
    row3->addWidget(new QLabel("值:"));
    m_oneClickZeroValueSpin = new QSpinBox();
    m_oneClickZeroValueSpin->setRange(0, 500);
    m_oneClickZeroValueSpin->setValue(40);
    row3->addWidget(m_oneClickZeroValueSpin);
    row3->addStretch();
    methodLayout->addLayout(row3);

    // 4. 调节增益
    QHBoxLayout *row4 = new QHBoxLayout();
    m_oneClickAdjGain = new QCheckBox("调节增益");
    m_oneClickAdjGain->setChecked(false);
    row4->addWidget(m_oneClickAdjGain);
    row4->addStretch();
    methodLayout->addLayout(row4);

    // 5. 数字滤波
    QHBoxLayout *row5 = new QHBoxLayout();
    m_oneClickDigFilter = new QCheckBox("数字滤波");
    m_oneClickDigFilter->setChecked(false);
    row5->addWidget(m_oneClickDigFilter);
    row5->addStretch();
    methodLayout->addLayout(row5);

    // 6. 背景消除(滑动法)
    QHBoxLayout *row6 = new QHBoxLayout();
    m_oneClickBgRemove = new QCheckBox("背景消除(滑动法)");
    m_oneClickBgRemove->setChecked(false);
    row6->addWidget(m_oneClickBgRemove);
    row6->addWidget(new QLabel("窗口:"));
    m_oneClickBgWindowSpin = new QSpinBox();
    m_oneClickBgWindowSpin->setRange(1, 9999);
    m_oneClickBgWindowSpin->setValue(200);
    row6->addWidget(m_oneClickBgWindowSpin);
    methodLayout->addLayout(row6);

    // 7. 滑动平均
    QHBoxLayout *row7 = new QHBoxLayout();
    m_oneClickSmooth = new QCheckBox("滑动平均");
    m_oneClickSmooth->setChecked(false);
    row7->addWidget(m_oneClickSmooth);
    row7->addWidget(new QLabel("窗口:"));
    m_oneClickSmoothWindowSpin = new QSpinBox();
    m_oneClickSmoothWindowSpin->setRange(1, 9999);
    m_oneClickSmoothWindowSpin->setValue(5);
    row7->addWidget(m_oneClickSmoothWindowSpin);
    methodLayout->addLayout(row7);

    methodLayout->addStretch();

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_oneClickBtnApply = new QPushButton("应用");
    QPushButton *btnOK = new QPushButton("确定");
    QPushButton *btnCancel = new QPushButton("取消");
    btnLayout->addWidget(m_oneClickBtnApply);
    btnLayout->addWidget(btnOK);
    btnLayout->addWidget(btnCancel);
    methodLayout->addLayout(btnLayout);

    mainLayout->addWidget(methodGroup, 1);

    // === Right panel: 处理后参考波形 ===
    QGroupBox *chartGroup = new QGroupBox("处理后参考波形");
    QVBoxLayout *chartLayout = new QVBoxLayout(chartGroup);

    m_oneClickChart = new QChart();
    m_oneClickChart->legend()->hide();

    m_oneClickSeries = new QLineSeries();
    m_oneClickSeries->setPen(QPen(Qt::blue, 1));

    m_oneClickChart->addSeries(m_oneClickSeries);

    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("幅度");
    m_oneClickChart->addAxis(axisX, Qt::AlignBottom);
    m_oneClickSeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("采样点");
    axisY->setRange(0, m_pixelsPerRow - 1);
    axisY->setLabelFormat("%d");
    axisY->setReverse(true);
    m_oneClickChart->addAxis(axisY, Qt::AlignLeft);
    m_oneClickSeries->attachAxis(axisY);

    // Populate with current X position data (will be updated by updateOneClickRefChart after connections)
    axisX->setRange(-8388608.0, 8388608.0);

    m_oneClickChartView = new CustomChartView();
    m_oneClickChartView->setChart(m_oneClickChart);
    m_oneClickChartView->setRenderHint(QPainter::Antialiasing);
    m_oneClickChartView->setLineSeries(m_oneClickSeries);
    m_oneClickChartView->setLineCount(8);
    m_oneClickChartView->setGainRange(-6.0f, 6.0f);
    m_oneClickChartView->setGainVisible(false);
    m_oneClickChartView->setYScale(1.0f);
    chartLayout->addWidget(m_oneClickChartView);

    mainLayout->addWidget(chartGroup, 2);

    // Connect buttons
    connect(m_oneClickBtnApply, &QPushButton::clicked, this, [this]() {
        applyOneClickProcess();
    });

    connect(btnOK, &QPushButton::clicked, this, [this]() {
        if (!requireOpenFile()) return;

        // 保存原 tab 指针和显示数据
        TabData *origTab = m_currentTab;
        QByteArray savedRawData = m_rawData;
        bool savedApplied = m_oneClickApplied;

        // 在临时数据上执行处理
        if (!m_oneClickApplied)
            applyOneClickProcess();

        if (m_oneClickApplied)
            saveProcessedFile();

        // 恢复原 tab 显示数据（确定不改变原文件图片显示）
        if (!savedApplied) {
            origTab->rawData = savedRawData;
            if (m_currentTab == origTab) {
                m_rawData = savedRawData;
            }
            m_oneClickApplied = false;
            m_oneClickBtnApply->setText("应用");
        }
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_oneClickApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_oneClickApplied = false;
            refreshImage();
            updateChart(m_lastChartX);
        }
        if (m_oneClickDlg)
            m_oneClickDlg->close();
    });

    // Real-time preview: dialog settings always update reference waveform
    connect(m_oneClickCorrectOffset, &QCheckBox::toggled, this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickTimeWindowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickAntennaFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickAmpComp, &QCheckBox::toggled, this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickAmpCompSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickAdjZero, &QCheckBox::toggled, this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickZeroValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        updateOneClickRefChart();
    });
    connect(m_oneClickAdjGain, &QCheckBox::toggled, this, [this]() {
        if (m_oneClickChartView) {
            m_oneClickChartView->setGainVisible(m_oneClickAdjGain->isChecked());
            m_oneClickChartView->update();
        }
        updateOneClickRefChart();
    });
    connect(m_oneClickChartView, &CustomChartView::gainChanged, this, [this](int idx, float val) {
        // Auto-expand gain range in 6dB steps
        if (m_oneClickChartView) {
            float maxAbs = qAbs(val);
            for (int i = 0; i < m_oneClickChartView->lineCount(); ++i) {
                float h = m_oneClickChartView->handleX(i);
                if (qAbs(h) > maxAbs) maxAbs = qAbs(h);
            }
            float n = std::ceil(maxAbs / 6.0f);
            if (n < 1.0f) n = 1.0f;
            float range = 6.0f * n;
            if (range > m_oneClickChartView->gainMax() || range < m_oneClickChartView->gainMax() - 6.0f) {
                m_oneClickChartView->setGainRange(-range, range);
                // Force QChart to recalculate layout for wider labels
                if (m_oneClickChart) {
                    QMargins mg = m_oneClickChart->margins();
                    // Top margin adapts to label height (font height + padding)
                    QFontMetrics fm(font());
                    int neededTop = fm.height() + 8;
                    if (neededTop > mg.top()) {
                        m_oneClickChart->setMargins(QMargins(mg.left(), neededTop, mg.right(), mg.bottom()));
                    }
                }
            }
        }
        updateOneClickRefChart();
    });

    // Initial preview with checked items (e.g. dewow)
    updateOneClickRefChart();

    m_oneClickDlg->show();
}

void MainWindow::applyOneClickProcess()
{
    if (!requireOpenFile()) return;

    if (m_oneClickApplied) {
        // Undo: only restore display (raw data + image + main chart)
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_oneClickApplied = false;
        m_oneClickBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);
        return;
    }

    // Backup original data
    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    char *data = m_rawData.data();

    // Step 1: 校正零偏 (Dewow - sliding window mean removal)
    if (m_oneClickCorrectOffset && m_oneClickCorrectOffset->isChecked()) {
        double timeWindowNs = m_oneClickTimeWindowSpin ? m_oneClickTimeWindowSpin->value() : 5.0;
        double timeRangeSec = m_currentTab->timeRange * 1e-9;
        double sampleInterval = timeRangeSec / samplesPerTrace;
        int windowSamples = qMax(1, static_cast<int>(timeWindowNs * 1e-9 / sampleInterval));
        int halfWin = windowSamples / 2;

        for (int t = 0; t < numTraces; ++t) {
            // Read trace into buffer and build cumulative sum
            QVector<double> trace(samplesPerTrace);
            for (int s = 0; s < samplesPerTrace; ++s) {
                int idx = (t * samplesPerTrace + s) * 4;
                trace[s] = static_cast<double>(
                    (static_cast<quint8>(data[idx+3]) << 24) |
                    (static_cast<quint8>(data[idx+2]) << 16) |
                    (static_cast<quint8>(data[idx+1]) << 8) |
                    static_cast<quint8>(data[idx]));
            }
            QVector<double> cumsum(samplesPerTrace + 1, 0.0);
            for (int s = 0; s < samplesPerTrace; ++s)
                cumsum[s + 1] = cumsum[s] + trace[s];

            // Subtract local mean (centered sliding window)
            for (int s = 0; s < samplesPerTrace; ++s) {
                int left = qMax(0, s - halfWin);
                int right = qMin(samplesPerTrace - 1, s + halfWin);
                double localMean = (cumsum[right + 1] - cumsum[left]) / (right - left + 1);
                qint32 val = static_cast<qint32>(trace[s] - localMean);
                int idx = (t * samplesPerTrace + s) * 4;
                data[idx]   = val & 0xFF;
                data[idx+1] = (val >> 8) & 0xFF;
                data[idx+2] = (val >> 16) & 0xFF;
                data[idx+3] = (val >> 24) & 0xFF;
            }
        }
    }

    // Step 2: 幅度补偿 (per-trace, segment-based with interpolation)
    if (m_oneClickAmpComp && m_oneClickAmpComp->isChecked()) {
        int compValue = m_oneClickAmpCompSpin ? m_oneClickAmpCompSpin->value() : 100;
        if (compValue > 0) {
            const int numSegs = 8;
            const int segSize = samplesPerTrace / numSegs;
            const int centers[8] = {32, 96, 160, 224, 288, 352, 416, 480};
            // Per-trace: compute segMax, interpolate, apply
            for (int t = 0; t < numTraces; ++t) {
                // Compute per-trace segMax from DEWOW'd data
                double segMax[numSegs];
                for (int seg = 0; seg < numSegs; ++seg) segMax[seg] = 0.0;
                for (int s = 0; s < samplesPerTrace; ++s) {
                    int idx = (t * samplesPerTrace + s) * 4;
                    qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                                 (static_cast<quint8>(data[idx+2]) << 16) |
                                 (static_cast<quint8>(data[idx+1]) << 8) |
                                 static_cast<quint8>(data[idx]);
                    int seg = qMin(s / segSize, numSegs - 1);
                    double av = qAbs(static_cast<double>(val));
                    if (av > segMax[seg]) segMax[seg] = av;
                }
                double coeff[numSegs];
                for (int seg = 0; seg < numSegs; ++seg) {
                    if (segMax[seg] < 1.0) segMax[seg] = 1.0;
                    coeff[seg] = 8388608.0 / segMax[seg] * (compValue / 100.0);
                }
                // Build interpolated gain table
                double gainTable[512];
                for (int i = 0; i < samplesPerTrace; ++i) {
                    if (i <= centers[0]) {
                        gainTable[i] = coeff[0];
                    } else if (i >= centers[7]) {
                        gainTable[i] = coeff[7];
                    } else {
                        for (int k = 0; k < 7; ++k) {
                            if (i <= centers[k + 1]) {
                                double t2 = (double)(i - centers[k]) / (centers[k + 1] - centers[k]);
                                gainTable[i] = coeff[k] + t2 * (coeff[k + 1] - coeff[k]);
                                break;
                            }
                        }
                    }
                }
                // Apply to this trace
                for (int s = 0; s < samplesPerTrace; ++s) {
                    int idx = (t * samplesPerTrace + s) * 4;
                    qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                                 (static_cast<quint8>(data[idx+2]) << 16) |
                                 (static_cast<quint8>(data[idx+1]) << 8) |
                                 static_cast<quint8>(data[idx]);
                    float result = static_cast<float>(val * gainTable[s]);
                    if (result > 8388607.0f) result = 8388607.0f;
                    if (result < -8388608.0f) result = -8388608.0f;
                    val = static_cast<qint32>(result);
                    data[idx]   = val & 0xFF;
                    data[idx+1] = (val >> 8) & 0xFF;
                    data[idx+2] = (val >> 16) & 0xFF;
                    data[idx+3] = (val >> 24) & 0xFF;
                }
            }
        }
    }

    // Step 3: 调节零点 (Zero-point adjustment - skip first N rows)
    if (m_oneClickAdjZero && m_oneClickAdjZero->isChecked()) {
        int zeroRows = m_oneClickZeroValueSpin ? m_oneClickZeroValueSpin->value() : 40;
        if (zeroRows > 0) {
            // Shift traces up by zeroRows, fill bottom with zeros
            for (int t = 0; t < numTraces; ++t) {
                int base = t * samplesPerTrace * 4;
                // Shift data up
                memmove(data + base, data + base + zeroRows * 4, (samplesPerTrace - zeroRows) * 4);
                // Zero fill bottom
                memset(data + base + (samplesPerTrace - zeroRows) * 4, 0, zeroRows * 4);
            }
        }
    }

    // Step 4: 调节增益 (apply interpolated gain from CustomChartView handles)
    if (m_oneClickAdjGain && m_oneClickAdjGain->isChecked() && m_oneClickChartView) {
        const int gN = m_pixelsPerRow;
        QVector<float> gainTable(gN);
        for (int x = 0; x < gN; ++x) {
            float rawGain = m_oneClickChartView->interpolatedGain(x);
            gainTable[x] = std::pow(10.0f, rawGain / 20.0f);
        }
        int totalPixels = m_rawData.size() / 4;
        for (int i = 0; i < totalPixels; ++i) {
            int idx = i * 4;
            qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                         (static_cast<quint8>(data[idx+2]) << 16) |
                         (static_cast<quint8>(data[idx+1]) << 8) |
                         static_cast<quint8>(data[idx]);
            float result = gainTable[i % gN] * static_cast<float>(val);
            if (result > 8388607.0f) result = 8388607.0f;
            if (result < -8388608.0f) result = -8388608.0f;
            val = static_cast<qint32>(result);
            data[idx]   = val & 0xFF;
            data[idx+1] = (val >> 8) & 0xFF;
            data[idx+2] = (val >> 16) & 0xFF;
            data[idx+3] = (val >> 24) & 0xFF;
        }
    }

    // Step 5: 数字滤波 (placeholder - user can use digital filter dialog)
    // m_oneClickDigFilter - no additional processing, filter is handled separately

    // Step 6: 背景消除(滑动法) (Background removal - sliding window)
    if (m_oneClickBgRemove && m_oneClickBgRemove->isChecked()) {
        int winSize = m_oneClickBgWindowSpin ? m_oneClickBgWindowSpin->value() : 200;
        int halfWin = winSize / 2;

        // Work on a copy for reading
        QByteArray copyData = m_rawData;
        const char *src = copyData.constData();

        for (int t = 0; t < numTraces; ++t) {
            for (int s = 0; s < samplesPerTrace; ++s) {
                // Compute average of surrounding traces
                int tStart = qMax(0, t - halfWin);
                int tEnd = qMin(numTraces - 1, t + halfWin);
                double avg = 0.0;
                int count = tEnd - tStart + 1;

                for (int tt = tStart; tt <= tEnd; ++tt) {
                    int idx = (tt * samplesPerTrace + s) * 4;
                    qint32 val = (static_cast<quint8>(src[idx+3]) << 24) |
                                 (static_cast<quint8>(src[idx+2]) << 16) |
                                 (static_cast<quint8>(src[idx+1]) << 8) |
                                 static_cast<quint8>(src[idx]);
                    avg += val;
                }
                avg /= count;

                int idx = (t * samplesPerTrace + s) * 4;
                qint32 val = (static_cast<quint8>(src[idx+3]) << 24) |
                             (static_cast<quint8>(src[idx+2]) << 16) |
                             (static_cast<quint8>(src[idx+1]) << 8) |
                             static_cast<quint8>(src[idx]);
                val -= static_cast<qint32>(avg);
                data[idx]   = val & 0xFF;
                data[idx+1] = (val >> 8) & 0xFF;
                data[idx+2] = (val >> 16) & 0xFF;
                data[idx+3] = (val >> 24) & 0xFF;
            }
        }
    }

    // Step 7: 滑动平均 (Moving average)
    if (m_oneClickSmooth && m_oneClickSmooth->isChecked()) {
        int winSize = m_oneClickSmoothWindowSpin ? m_oneClickSmoothWindowSpin->value() : 5;
        int halfWin = winSize / 2;

        QByteArray copyData = m_rawData;
        const char *src = copyData.constData();

        for (int t = 0; t < numTraces; ++t) {
            for (int s = 0; s < samplesPerTrace; ++s) {
                double avg = 0.0;
                int count = 0;
                int sStart = qMax(0, s - halfWin);
                int sEnd = qMin(samplesPerTrace - 1, s + halfWin);
                for (int ss = sStart; ss <= sEnd; ++ss) {
                    int idx = (t * samplesPerTrace + ss) * 4;
                    qint32 val = (static_cast<quint8>(src[idx+3]) << 24) |
                                 (static_cast<quint8>(src[idx+2]) << 16) |
                                 (static_cast<quint8>(src[idx+1]) << 8) |
                                 static_cast<quint8>(src[idx]);
                    avg += val;
                    count++;
                }
                avg /= count;

                int idx = (t * samplesPerTrace + s) * 4;
                qint32 result = static_cast<qint32>(avg);
                data[idx]   = result & 0xFF;
                data[idx+1] = (result >> 8) & 0xFF;
                data[idx+2] = (result >> 16) & 0xFF;
                data[idx+3] = (result >> 24) & 0xFF;
            }
        }
    }

    m_currentTab->rawData = m_rawData;
    m_oneClickApplied = true;
    m_oneClickBtnApply->setText("撤销");

    refreshImage();
    updateChart(m_lastChartX);

    // Update one-click reference chart
    if (m_oneClickSeries && !m_rawData.isEmpty()) {
        m_oneClickSeries->clear();
        for (int i = 0; i < samplesPerTrace; ++i) {
            qint32 val = getPixelValue(m_lastChartX, i);
            m_oneClickSeries->append(static_cast<qreal>(val), static_cast<qreal>(i));
        }
        auto axes = m_oneClickChart->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            QValueAxis *ax = qobject_cast<QValueAxis*>(axes.first());
            if (ax) ax->setRange(-8388608.0, 8388608.0);
        }
        if (m_oneClickChartView) m_oneClickChartView->update();
    }
}

void MainWindow::updateOneClickRefChart()
{
    if (!m_oneClickSeries || !m_currentTab) return;

    // Always read from original data so dialog preview is independent of apply/undo
    const QByteArray &srcData = (m_currentTab->gainApplied || m_oneClickApplied)
                                ? m_currentTab->originalRawData : m_rawData;
    if (srcData.isEmpty()) return;

    m_oneClickSeries->clear();

    int samplesPerTrace = m_pixelsPerRow;

    // Read raw 512 samples for current trace from source data
    QVector<qint32> samples(samplesPerTrace);
    for (int i = 0; i < samplesPerTrace; ++i) {
        int dataIdx = (m_lastChartX * samplesPerTrace + i) * 4;
        qint32 val = 0;
        if (dataIdx + 4 <= srcData.size()) {
            val = static_cast<qint32>(
                (static_cast<quint8>(srcData[dataIdx + 3]) << 24) |
                (static_cast<quint8>(srcData[dataIdx + 2]) << 16) |
                (static_cast<quint8>(srcData[dataIdx + 1]) << 8) |
                static_cast<quint8>(srcData[dataIdx]));
        }
        samples[i] = val;
    }

    // Preview: 校正零偏 (Dewow - sliding window mean removal)
    if (m_oneClickCorrectOffset && m_oneClickCorrectOffset->isChecked()) {
        double timeWindowNs = m_oneClickTimeWindowSpin ? m_oneClickTimeWindowSpin->value() : 5.0;
        double timeRangeSec = m_currentTab->timeRange * 1e-9;
        double sampleInterval = timeRangeSec / samplesPerTrace;
        int windowSamples = qMax(1, static_cast<int>(timeWindowNs * 1e-9 / sampleInterval));
        int halfWin = windowSamples / 2;
        QVector<double> cumsum(samplesPerTrace + 1, 0.0);
        for (int s = 0; s < samplesPerTrace; ++s)
            cumsum[s + 1] = cumsum[s] + samples[s];
        for (int s = 0; s < samplesPerTrace; ++s) {
            int left = qMax(0, s - halfWin);
            int right = qMin(samplesPerTrace - 1, s + halfWin);
            double localMean = (cumsum[right + 1] - cumsum[left]) / (right - left + 1);
            samples[s] -= static_cast<qint32>(localMean);
        }
    }

    // Preview: 幅度补偿 (per-trace, segment-based with interpolation)
    if (m_oneClickAmpComp && m_oneClickAmpComp->isChecked() && m_oneClickAmpCompSpin) {
        int compValue = m_oneClickAmpCompSpin->value();
        if (compValue > 0) {
            const int numSegs = 8;
            const int segSize = samplesPerTrace / numSegs;
            // Compute segMax from current trace's DEWOW'd samples
            double segMax[numSegs];
            for (int seg = 0; seg < numSegs; ++seg) segMax[seg] = 0.0;
            for (int i = 0; i < samplesPerTrace; ++i) {
                int seg = qMin(i / segSize, numSegs - 1);
                double av = qAbs(static_cast<double>(samples[i]));
                if (av > segMax[seg]) segMax[seg] = av;
            }
            double coeff[numSegs];
            for (int seg = 0; seg < numSegs; ++seg) {
                if (segMax[seg] < 1.0) segMax[seg] = 1.0;
                coeff[seg] = 8388608.0 / segMax[seg] * (compValue / 100.0);
            }
            // Build interpolated gain table (linear between segment centers)
            const int centers[8] = {32, 96, 160, 224, 288, 352, 416, 480};
            double gainTable[512];
            for (int i = 0; i < samplesPerTrace; ++i) {
                if (i <= centers[0]) {
                    gainTable[i] = coeff[0];
                } else if (i >= centers[7]) {
                    gainTable[i] = coeff[7];
                } else {
                    for (int k = 0; k < 7; ++k) {
                        if (i <= centers[k + 1]) {
                            double t = (double)(i - centers[k]) / (centers[k + 1] - centers[k]);
                            gainTable[i] = coeff[k] + t * (coeff[k + 1] - coeff[k]);
                            break;
                        }
                    }
                }
            }
            for (int i = 0; i < samplesPerTrace; ++i) {
                float result = static_cast<float>(samples[i] * gainTable[i]);
                if (result > 8388607.0f) result = 8388607.0f;
                if (result < -8388608.0f) result = -8388608.0f;
                samples[i] = static_cast<qint32>(result);
            }
        }
    }

    // Preview: 调节零点 (shift up)
    if (m_oneClickAdjZero && m_oneClickAdjZero->isChecked() && m_oneClickZeroValueSpin) {
        int zeroRows = m_oneClickZeroValueSpin->value();
        if (zeroRows > 0 && zeroRows < samplesPerTrace) {
            for (int i = 0; i < samplesPerTrace - zeroRows; ++i)
                samples[i] = samples[i + zeroRows];
            for (int i = samplesPerTrace - zeroRows; i < samplesPerTrace; ++i)
                samples[i] = 0;
        }
    }

    // Preview: 调节增益 (interpolated gain from CustomChartView handles)
    if (m_oneClickAdjGain && m_oneClickAdjGain->isChecked() && m_oneClickChartView) {
        const int gN = m_pixelsPerRow;
        QVector<float> gainTable(gN);
        for (int x = 0; x < gN; ++x) {
            float rawGain = m_oneClickChartView->interpolatedGain(x);
            gainTable[x] = std::pow(10.0f, rawGain / 20.0f);
        }
        for (int i = 0; i < samplesPerTrace; ++i) {
            float result = gainTable[i % gN] * static_cast<float>(samples[i]);
            if (result > 8388607.0f) result = 8388607.0f;
            if (result < -8388608.0f) result = -8388608.0f;
            samples[i] = static_cast<qint32>(result);
        }
    }

    // Display in chart
    for (int i = 0; i < samplesPerTrace; ++i)
        m_oneClickSeries->append(static_cast<qreal>(samples[i]), static_cast<qreal>(i));

    auto axes = m_oneClickChart->axes(Qt::Horizontal);
    if (!axes.isEmpty()) {
        QValueAxis *ax = qobject_cast<QValueAxis*>(axes.first());
        if (ax) ax->setRange(-8388608.0, 8388608.0);
    }
    if (m_oneClickChartView) {
        m_oneClickChartView->chart()->update();
        m_oneClickChartView->viewport()->update();
        m_oneClickChartView->update();
    }
}
