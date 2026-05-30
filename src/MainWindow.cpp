#include "MainWindow.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QScrollArea>
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
#include <functional>
#include <cmath>
#include <complex>
#include <vector>

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
            m_lineY[i] = 511.0f * i / (m_lineCount - 1);
        if (m_handleX[i] == 0) m_handleX[i] = 0;
    }
    update();
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
    QChartView::paintEvent(event);

    if (!chart()) return;
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

void ImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull()) {
        QLabel::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_crosshairPos = event->pos();
        m_showCrosshair = true;
        int clampedY = qBound(0, event->pos().y(), height() - 1);
        int origY = (height() > 1) ? clampedY * (m_originalSize.height() - 1) / (height() - 1) : 0;
        QPoint origPos(event->pos().x(), origY);
        emit imageClicked(origPos);
        update();
    }
    QLabel::mousePressEvent(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_showCrosshair && !m_image.isNull() && (event->buttons() & Qt::LeftButton)) {
        m_crosshairPos = event->pos();
        int clampedY = qBound(0, event->pos().y(), height() - 1);
        int origY = (height() > 1) ? clampedY * (m_originalSize.height() - 1) / (height() - 1) : 0;
        QPoint origPos(event->pos().x(), origY);
        emit imageClicked(origPos);
        update();
    }
    QLabel::mouseMoveEvent(event);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
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
        QPen pen(Qt::white, 1);
        painter.setPen(pen);

        int x = m_crosshairPos.x();
        int y = m_crosshairPos.y();

        painter.drawLine(0, y, width(), y);
        painter.drawLine(x, 0, x, height());
    }
}

void ImageLabel::contextMenuEvent(QContextMenuEvent *event)
{
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

    if (m_dataWidth <= 0) return;

    p.drawLine(0, h - 1, w, h - 1);

    int startTrace = m_offset;
    int endTrace = m_offset + w;

    int firstMinor = (startTrace / 10) * 10;
    if (firstMinor < startTrace) firstMinor += 10;

    for (int val = firstMinor; val <= endTrace; val += 10) {
        int x = val - m_offset;
        if (val % 100 != 0)
            p.drawLine(x, h - 5, x, h);
    }

    int firstMajor = (startTrace / 100) * 100;
    if (firstMajor < startTrace) firstMajor += 100;

    for (int val = firstMajor; val <= endTrace; val += 100) {
        int x = val - m_offset;
        p.drawLine(x, h - 10, x, h);
        int textX = qMax(0, x - 30);
        p.drawText(textX, 14, 60, h - 24, Qt::AlignLeft,
                   QString::number(val));
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
        double majorInterval = 2.5;
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
                       QString::number(val, 'f', 1));
        }
    } else {
        double majorInterval = 0.25;
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
                       QString::number(val, 'f', 2));
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

    // 结束 (固定值，不可编辑)
    QTreeWidgetItem *endItem = new QTreeWidgetItem(sampleItem, QStringList() << "结束" << "511");
    endItem->setFlags(endItem->flags() & ~Qt::ItemIsEditable);

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
    welcomeLabel->setStyleSheet("background: #2b2b2b;");
    QPixmap welcomePix(":/icons/resources/welcome.png");
    welcomeLabel->setPixmap(welcomePix.scaled(800, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // --- Shared: document tab widget ---
    m_docTabWidget = new QTabWidget(this);
    m_docTabWidget->setTabsClosable(true);
    m_docTabWidget->setDocumentMode(true);
    m_docTabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { background: #e0e0e0; padding: 6px 16px; border: 1px solid #c0c0c0; min-width: 80px; }"
        "QTabBar::tab:selected { background: #ffffff; }"
    );

    // Left panel: stacked gain / zero-point pages
    m_leftPanel = new QWidget;
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
    gainBtnLayout->addWidget(m_btnOK);
    gainBtnLayout->addWidget(m_btnCancel);
    gainBtnLayout->addWidget(m_btnApply);
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
        m_leftPanel->hide();
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
        if (!m_currentTab) return;

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

        // 打开新文件作为新 tab
        QImage image = loadDZTFile(outPath);
        if (!image.isNull()) {
            createTab(outPath, image);
        }
    });

    connect(zeroBtnApply, &QPushButton::clicked, this, [this]() {
        if (!m_currentTab) return;
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
    m_leftPanel->setMinimumWidth(260);
    contentLayout->addWidget(welcomeLabel, 1);
    contentLayout->addWidget(m_docTabWidget, 1);

    // Initially show welcome, hide others
    m_leftPanel->hide();
    m_docTabWidget->hide();
    welcomeLabel->show();

    mainLayout->addLayout(contentLayout);

    // --- Bottom bar ---
    coordinateLabel = new QLabel("点击图片查看坐标", this);
    coordinateLabel->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc; padding: 5px; font-family: monospace;");

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(22);
    m_progressBar->hide();

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_progressBar);
    buttonLayout->addWidget(coordinateLabel);

    mainLayout->addLayout(buttonLayout);

    setCentralWidget(centralWidget);
    setWindowTitle("DZT Image Viewer");
    resize(1200, 700);

    createMenuBar();
    loadLUT(12);
    connect(m_docTabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_docTabWidget, &QTabWidget::currentChanged, this, &MainWindow::switchToTab);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_tabs);
}

// --- Tab management ---

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
    tab->chartView->chart()->addSeries(tab->chartSeries);
    tab->chartView->chart()->legend()->hide();
    tab->chartView->setLineSeries(tab->chartSeries);

    QValueAxis *axisX = new QValueAxis();
    axisX->setRange(-100, 100);
    axisX->setLabelFormat("%d");
    axisX->setTickCount(5);

    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 511);
    axisY->setTickCount(6);
    axisY->setLabelFormat("%d");
    axisY->setReverse(true);

    // If signalPosition < 0, set axis range only (data offset handled in updateChart)
    if (m_signalPos < 0.0f) {
        axisY->setRange(0, 20.0 + m_signalPos);
        axisY->setLabelFormat("%.1f");
    }

    tab->chartView->chart()->setAxisX(axisX, tab->chartSeries);
    tab->chartView->chart()->setAxisY(axisY, tab->chartSeries);
    tab->chartView->chart()->setAnimationOptions(QChart::NoAnimation);

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

    // Page layout: imageGrid + chartView
    pageLayout->addLayout(tab->imageGrid, 1);
    pageLayout->addWidget(tab->chartView);

    // Per-tab signal connections
    connect(tab->imageLabel, &ImageLabel::imageClicked, this, &MainWindow::onImageClicked);

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

    // Add to doc tab widget
    QString tabTitle = QFileInfo(filePath).fileName();
    int idx = m_docTabWidget->addTab(tab->page, tabTitle);
    m_docTabWidget->setCurrentIndex(idx);

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
        m_timeRange = 20.0;
        m_depthRange = 1.25;
        return;
    }

    QWidget *page = m_docTabWidget->widget(index);
    TabData *tab = nullptr;
    for (auto *t : m_tabs) {
        if (t->page == page) { tab = t; break; }
    }
    if (!tab) return;

    m_currentTab = tab;

    m_rawData = tab->rawData;
    m_dataOffset = tab->dataOffset;
    m_pixelsPerRow = tab->pixelsPerRow;
    m_gain = tab->gain;
    m_transformMode = tab->transformMode;
    m_traceCount = tab->traceCount;
    m_timeRange = tab->timeRange;
    m_depthRange = tab->depthRange;

    scrollArea = tab->scrollArea;
    imageLabel = tab->imageLabel;
    chartView = tab->chartView;
    chartSeries = tab->chartSeries;

    // Sync button state
    m_btnApply->setText(tab->gainApplied ? "撤销" : "应用");
    m_btnApply->setEnabled(true);
    m_btnOK->setEnabled(true);
    m_btnCancel->setEnabled(true);

    // Defer resize until the tab page layout is settled
    QTimer::singleShot(0, this, [this]() {
        if (m_currentTab) {
            updateRulers();
            resizeImageLabel();
        }
    });
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_docTabWidget->count()) return;

    QWidget *page = m_docTabWidget->widget(index);
    TabData *tab = nullptr;
    for (auto *t : m_tabs) {
        if (t->page == page) { tab = t; break; }
    }
    if (!tab) return;

    m_tabs.removeOne(tab);
    m_docTabWidget->removeTab(index);

    delete tab->page;
    delete tab;

    if (m_tabs.isEmpty()) {
        showWelcome();
    }
}

void MainWindow::showWelcome()
{
    m_leftPanel->hide();
    m_docTabWidget->hide();
    welcomeLabel->show();
    m_btnApply->setEnabled(false);
    m_btnOK->setEnabled(false);
    m_btnCancel->setEnabled(false);
}

void MainWindow::hideWelcome()
{
    welcomeLabel->hide();
    m_docTabWidget->show();
}

void MainWindow::showFileHeader()
{
    if (!m_currentTab) return;

    // Read header from DZT file
    QFile file(m_currentTab->filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray hdr = file.read(1024);
    file.close();

    if (hdr.size() < 128) return;

    // Helper: read little-endian short at offset
    auto rdShort = [&hdr](int off) -> qint16 {
        return static_cast<qint16>(
            (static_cast<quint8>(hdr[off+1]) << 8) |
            static_cast<quint8>(hdr[off]));
    };
    // Helper: read little-endian float at offset
    auto rdFloat = [&hdr](int off) -> float {
        float val;
        memcpy(&val, hdr.constData() + off, 4);
        return val;
    };
    // Helper: decode tagRFDate (4 bytes at offset) → "Mon,DD YYYY,HH:MM:SS"
    auto rdDate = [&hdr](int off) -> QString {
        const char *months[] = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        quint32 val = static_cast<quint8>(hdr[off])
                    | (static_cast<quint8>(hdr[off+1]) << 8)
                    | (static_cast<quint8>(hdr[off+2]) << 16)
                    | (static_cast<quint8>(hdr[off+3]) << 24);
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

    // Parse header fields
    QString fileName = QString::fromLatin1(hdr.mid(114, 12)).trimmed();
    fileName = fileName.left(fileName.indexOf(QLatin1Char('\0')));
    QString createDate = rdDate(32);
    QString modDate = rdDate(36);
    // Byte 113 bitfield: [rh_version:3 | rh_system:5] — high 5 bits = system code
    // DZT rh_system control unit table (from RADAN DZT File Format spec):
    //   2=SIR 2000, 3=SIR 3000, 4=TerraVision, 6=SIR 20,
    //   7=SS Mini, 8=SIR 4000, 9=SIR 30, 12=UtilityScan DF
    int systemCode = (static_cast<quint8>(hdr[113]) >> 3) & 0x1F;
    QString systemName;
    switch (systemCode) {
        case 2: systemName = "SIR-2000"; break;
        case 3: systemName = "SIR-3000"; break;
        case 4: systemName = "TerraVision"; break;
        case 6: systemName = "SIR-20"; break;
        case 7: systemName = "SS Mini"; break;
        case 8: systemName = "SIR-4000"; break;
        case 9: systemName = "SIR-30"; break;
        case 12: systemName = "UtilityScan DF"; break;
        default: systemName = QString("Unknown(%1)").arg(systemCode); break;
    }
    int nchan = rdShort(52);
    float sps = rdFloat(10);
    float spm = rdFloat(14);
    float mpm = rdFloat(18);
    int nsamp = rdShort(4);
    int bits = rdShort(6);
    float epsr = rdFloat(54);
    QString antName = QString::fromLatin1(hdr.mid(98, 14)).trimmed();
    antName = antName.left(antName.indexOf(QLatin1Char('\0')));
    float position = rdFloat(22);
    float range = rdFloat(26);
    float top = rdFloat(58);
    float depth = rdFloat(62);
    int repeatsSample = rdShort(8);
    int npass = rdShort(30);

    // Processing history: {short typeCode, float value} records, 6 bytes each
    // typeCode: low byte = processing type (0x4D=77), high byte = record index
    QVector<QPair<int, float>> procRecords;  // (index, value)
    qint16 procOff = rdShort(48);
    qint16 procSize = rdShort(50);
    if (procOff > 0 && procSize > 0 && procOff + procSize <= hdr.size()) {
        int nRec = procSize / 6;
        for (int r = 0; r < nRec; ++r) {
            int off = procOff + r * 6;
            quint16 typeCode = static_cast<quint16>(rdShort(off));
            int recIdx = typeCode >> 8;  // high byte = record index
            float val;
            memcpy(&val, hdr.constData() + off + 2, 4);
            procRecords.append({recIdx, val});
        }
    }

    // --- Build dialog ---
    QDialog dlg(this);
    dlg.setWindowTitle("文件头信息 - " + QFileInfo(m_currentTab->filePath).fileName());
    dlg.setMinimumSize(420, 560);
    dlg.resize(420, 560);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QTreeWidget *tree = new QTreeWidget();
    tree->setHeaderHidden(true);
    tree->setColumnCount(2);
    tree->setRootIsDecorated(true);
    tree->setIndentation(20);
    tree->setAnimated(true);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setFocusPolicy(Qt::NoFocus);
    tree->header()->setStretchLastSection(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->setStyleSheet(
        "QTreeWidget { border: none; font-size: 12px; }"
        "QTreeWidget::item { padding: 2px 0; }"
        "QTreeWidget::item:selected { background: transparent; color: inherit; }"
    );

    auto addRow = [&tree](QTreeWidgetItem *parent, const QString &label, const QString &value) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parent, QStringList() << label << "");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        QLabel *valLabel = new QLabel(value);
        valLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        tree->setItemWidget(item, 1, valLabel);
        return item;
    };

    // 头文件参数
    QTreeWidgetItem *headerRoot = new QTreeWidgetItem(tree, QStringList() << "头文件参数" << "");
    addRow(headerRoot, "文件原始名称", fileName);
    addRow(headerRoot, "创建", createDate);
    addRow(headerRoot, "编辑时间", modDate);
    addRow(headerRoot, "地质雷达系统", systemName);
    addRow(headerRoot, "通道数", QString::number(nchan));

    // 水平参数
    QTreeWidgetItem *horiRoot = new QTreeWidgetItem(tree, QStringList() << "水平参数" << "");
    addRow(horiRoot, "扫描/秒", QString::number(sps, 'f', 2));
    addRow(horiRoot, "扫描/单位(cm)", QString::number(spm * 100.0, 'f', 3));
    addRow(horiRoot, "单位/标记(m)", QString::number(mpm, 'f', 3));

    // 垂直参数
    QTreeWidgetItem *vertRoot = new QTreeWidgetItem(tree, QStringList() << "垂直参数" << "");
    addRow(vertRoot, "采样点数/扫描", QString::number(nsamp));
    addRow(vertRoot, "位/采样", QString::number(bits));
    addRow(vertRoot, "介电常数", QString::number(epsr, 'f', 2));

    // 通道信息
    QTreeWidgetItem *chanRoot = new QTreeWidgetItem(tree, QStringList() << "通道信息" << "");
    addRow(chanRoot, "通道", QString::number(nchan));
    addRow(chanRoot, "天线类型", antName);
    addRow(chanRoot, "天线序列号", "0");
    addRow(chanRoot, "信号位置 (ns)", QString::number(position, 'f', 2));
    addRow(chanRoot, "记录长度(ns)", QString::number(range, 'f', 2));
    addRow(chanRoot, "顶面(cm)", QString::number(top * 100.0, 'f', 2));
    addRow(chanRoot, "深度(cm)", QString::number(depth * 100.0, 'f', 2));
    addRow(chanRoot, "# 采样叠加", QString::number(repeatsSample));
    addRow(chanRoot, "# 扫描叠加", QString::number(npass));

    // 处理记录
    QTreeWidgetItem *procRoot = new QTreeWidgetItem(tree, QStringList() << "处理记录" << "");
    for (const auto &rec : procRecords) {
        addRow(procRoot, QString("处理记录%1#").arg(rec.first),
               QString::number(rec.second, 'f', 2));
    }

    tree->expandAll();
    layout->addWidget(tree);

    QPushButton *btnOK = new QPushButton("确定");
    btnOK->setFixedWidth(80);
    connect(btnOK, &QPushButton::clicked, &dlg, &QDialog::accept);
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(btnOK);
    layout->addLayout(btnLayout);

    dlg.exec();
}

// --- File operations ---

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open DZT File", "",
        "DZT Files (*.dzt);;All Files (*)");

    if (fileName.isEmpty()) return;

    QImage image = loadDZTFile(fileName);
    if (image.isNull()) {
        QMessageBox::warning(this, "Error", "Failed to load DZT file.");
        return;
    }

    coordinateLabel->setText("点击图片查看坐标");

    if (m_tabs.isEmpty()) hideWelcome();

    createTab(fileName, image);
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

    qint32 pixelValue = getPixelValue(x, y);
    double normalizedValue = static_cast<double>(pixelValue) / (256.0 * 256.0 * 2.0);

    coordinateLabel->setText(QString("坐标: X=%1, Y=%2 | Pixel值: %3 | 归一化值: %4")
                           .arg(x)
                           .arg(y)
                           .arg(pixelValue)
                           .arg(normalizedValue, 0, 'f', 6));

    updateChart(x);
}

void MainWindow::updateChart(int xValue)
{
    if (m_rawData.isEmpty() || !chartSeries) return;

    m_lastChartX = xValue;
    chartSeries->clear();

    const int maxPoints = 512;
    qint32 minVal = 0, maxVal = 0;
    float yscale = chartView ? chartView->yScale() : 1.0f;

    bool isLinear = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
    bool isZeroMode = (yscale != 1.0f);

    // Signal position offset from file header (e.g., -2.79)
    float sigPos = m_currentTab ? m_currentTab->signalPosition : 0.0f;
    int sigPad = (sigPos < 0.0f) ? qRound(maxPoints * (-sigPos) / 20.0f) : 0;

    // Zero-point manual overrides
    double zeroOff = 0.0;
    int zeroPad = 0;
    if (isZeroMode) {
        if (m_zeroRangePctSpin)
            zeroOff = -m_zeroRangePctSpin->value() * 0.2;
        if (m_zeroOffsetSpin && m_zeroOffsetSpin->value() > 0)
            zeroPad = qRound(maxPoints * m_zeroOffsetSpin->value() / 20.0);
    }

    int totalOff = sigPad + zeroPad;
    double displayRange = isZeroMode ? 20.0 : (20.0 + sigPos);

    for (int i = 0; i < maxPoints; ++i) {
        qint32 displayValue = 0;
        int srcY = totalOff + i;
        if (srcY < maxPoints) {
            qint32 pixelValue = getPixelValue(xValue, srcY);
            float rawGain = (chartView) ? chartView->interpolatedGain(srcY) : m_gain;
            float rowGainLinear = isLinear ? rawGain : std::pow(10.0f, rawGain / 20.0f);
            displayValue = static_cast<qint32>(rowGainLinear * pixelValue);
        }
        qreal yCoord;
        if (isZeroMode)
            yCoord = zeroOff + static_cast<qreal>(i) * (20.0 / 511.0);
        else if (sigPad > 0)
            yCoord = static_cast<qreal>(i) * (displayRange / 511.0);
        else
            yCoord = static_cast<qreal>(i);
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
        } else if (sigPos < 0.0f) {
            axisY->setRange(0, 20.0 + sigPos);
            axisY->setLabelFormat("%.1f");
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
    const int pixelsPerRow = 512;

    m_dataOffset = dataOffset;
    m_pixelsPerRow = pixelsPerRow;

    // Read signal position (rhf_position) at offset 22
    file.seek(22);
    m_signalPos = 0.0f;
    file.read(reinterpret_cast<char*>(&m_signalPos), 4);
    qDebug() << "loadDZTFile: signalPos=" << m_signalPos;

    if (!file.seek(dataOffset)) {
        QMessageBox::warning(this, "Error", "open file failed.");
        return QImage();
    }

    m_rawData = file.readAll();
    int dataSize = m_rawData.size();
    int totalPixels = dataSize / bytesPerPixel;
    int rows = totalPixels / pixelsPerRow;
    int sigPad = (m_signalPos < 0.0f) ? qRound(pixelsPerRow * (-m_signalPos) / 20.0f) : 0;
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

void MainWindow::applyGain()
{
    if (!m_currentTab) return;

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
        float gainTable[512];
        for (int x = 0; x < 512; ++x) {
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
            val = static_cast<qint32>(gainTable[i % 512] * static_cast<float>(val));
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
    if (!m_currentTab) return;

    // 确保增益已应用
    if (!m_currentTab->gainApplied) {
        // 先应用增益
        m_currentTab->originalRawData = m_rawData;
        bool isLinear2 = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 2;
        float gainTable[512];
        for (int x = 0; x < 512; ++x) {
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
            val = static_cast<qint32>(gainTable[i % 512] * static_cast<float>(val));
            data[idx]   = val & 0xFF;
            data[idx+1] = (val >> 8) & 0xFF;
            data[idx+2] = (val >> 16) & 0xFF;
            data[idx+3] = (val >> 24) & 0xFF;
        }
        m_currentTab->rawData = m_rawData;
        m_currentTab->gainApplied = true;
        m_btnApply->setText("撤销");
    }

    // 创建 proc 目录
    QFileInfo fi(m_currentTab->filePath);
    QString procDir = fi.absolutePath() + "/proc";
    QDir().mkpath(procDir);

    // 找到可用的文件名 P_N.DZT
    int N = 1;
    QString outPath;
    do {
        outPath = procDir + QString("/P_%1.DZT").arg(N++);
    } while (QFile::exists(outPath));

    // 写文件：0x20000 头部 + 处理后的数据
    QFile srcFile(m_currentTab->filePath);
    QFile outFile(outPath);
    if (!srcFile.open(QIODevice::ReadOnly) || !outFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to save processed file.");
        return;
    }
    QByteArray header = srcFile.read(m_currentTab->dataOffset);
    outFile.write(header);
    srcFile.close();
    outFile.write(m_rawData);
    outFile.close();

    // 打开新文件作为新 tab
    QImage image = loadDZTFile(outPath);
    if (!image.isNull()) {
        createTab(outPath, image);
    }
}

void MainWindow::refreshImage()
{
    if (!m_currentTab || m_rawData.isEmpty()) return;

    const int pixelsPerRow = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int rows = totalPixels / pixelsPerRow;
    int sigPad = (m_currentTab->signalPosition < 0.0f)
        ? qRound(pixelsPerRow * (-m_currentTab->signalPosition) / 20.0f) : 0;
    int skipRows = sigPad + (m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0);
    int drawRows = pixelsPerRow - skipRows;

    QImage image(rows, drawRows, QImage::Format_RGB32);

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

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < drawRows; ++x) {
                int srcX = x + skipRows;
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
                pixelValue_display = static_cast<int>(m_gain * pixelValue_display);

                int lutIdx = pixelValue_display / (256 * 256) + 128;
                if (lutIdx < 0) lutIdx = 0;
                if (lutIdx > 255) lutIdx = 255;
                image.setPixel(y, x, m_lut[lutIdx]);
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

    // Time range based on signal position
    float sigPos = m_currentTab->signalPosition;
    m_timeRange = 20.0 + sigPos;  // e.g. 20 + (-2.79) = 17.21
    m_depthRange = 1.25 * m_timeRange / 20.0;
    m_currentTab->timeRange = m_timeRange;
    m_currentTab->depthRange = m_depthRange;

    int sigPad = (sigPos < 0.0f) ? qRound(m_pixelsPerRow * (-sigPos) / 20.0f) : 0;
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

    int sigPad = (m_currentTab->signalPosition < 0.0f)
        ? qRound(m_pixelsPerRow * (-m_currentTab->signalPosition) / 20.0f) : 0;
    int skipRows = sigPad + (m_currentTab->zeroApplied ? m_currentTab->zeroSkipRows : 0);
    int drawRows = m_pixelsPerRow - skipRows;
    int viewH = m_currentTab->scrollArea->viewport()->height();
    if (viewH <= 0) viewH = drawRows;

    m_currentTab->imageLabel->setFixedSize(m_traceCount, viewH);

    int maxVal = qMax(0, m_traceCount - m_currentTab->scrollArea->viewport()->width());
    m_currentTab->extHScrollBar->setRange(0, maxVal);
    m_currentTab->extHScrollBar->setPageStep(m_currentTab->scrollArea->viewport()->width());
    m_currentTab->extHScrollBar->setVisible(maxVal > 0);

    m_currentTab->leftRuler->update();
    m_currentTab->rightRuler->update();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize && m_currentTab) {
        if (watched == m_currentTab->scrollArea->viewport()) {
            resizeImageLabel();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    resizeImageLabel();
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
    ribbonTab->setFixedHeight(120);
    ribbonTab->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #c0c0c0; background: #f0f0f0; }"
        "QTabBar::tab { background: #e0e0e0; padding: 6px 16px; border: 1px solid #c0c0c0; }"
        "QTabBar::tab:selected { background: #ffffff; border-bottom: 2px solid #0078d7; }"
    );

    // --- Tab: 开始 ---
    QWidget *startPage = new QWidget();
    QHBoxLayout *startLayout = new QHBoxLayout(startPage);
    startLayout->setContentsMargins(4, 2, 4, 2);
    startLayout->setSpacing(8);

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

    auto makeBtn = [](const QString &iconPath, const QString &text) -> QToolButton* {
        QToolButton *btn = new QToolButton();
        btn->setIcon(QIcon(iconPath));
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIconSize(QSize(32, 32));
        btn->setFixedSize(56, 64);
        btn->setStyleSheet(
            "QToolButton { border: none; border-radius: 3px; background: transparent; font-size: 11px; }"
            "QToolButton:hover { background: #dce7f5; }"
            "QToolButton:pressed { background: #b8d0ea; }"
        );
        return btn;
    };

    // 文件设置 group
    QVBoxLayout *fileGroup = addGroup(startLayout, "文件设置");
    QHBoxLayout *fileBtns = qobject_cast<QHBoxLayout*>(fileGroup->itemAt(0)->layout());
    QToolButton *btnOpen = makeBtn(":/icons/resources/open.png", "打开");
    QToolButton *btnSave = makeBtn(":/icons/resources/save.png", "保存");
    QToolButton *btnClose = makeBtn(":/icons/resources/close.png", "关闭");
    QToolButton *btnHeader = makeBtn("", "文件头");
    fileBtns->addWidget(btnOpen);
    fileBtns->addWidget(btnSave);
    fileBtns->addWidget(btnClose);
    fileBtns->addWidget(btnHeader);

    connect(btnOpen, &QToolButton::clicked, this, &MainWindow::onOpenFile);
    connect(btnHeader, &QToolButton::clicked, this, &MainWindow::showFileHeader);
    connect(btnClose, &QToolButton::clicked, this, [this]() {
        if (!m_tabs.isEmpty()) {
            int idx = m_docTabWidget->currentIndex();
            if (idx >= 0) closeTab(idx);
        }
    });

    // 图像缩放 group
    QVBoxLayout *zoomGroup = addGroup(startLayout, "图像缩放");
    QHBoxLayout *zoomBtns = qobject_cast<QHBoxLayout*>(zoomGroup->itemAt(0)->layout());
    zoomBtns->addWidget(makeBtn(":/icons/resources/hzoomin.png", "水平放大"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/hzoomout.png", "水平缩小"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/restore.png", "图像还原"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/stack.png", "堆积图"));

    // 调色板 button with dropdown menu
    {
        QToolButton *paletteBtn = makeBtn(":/icons/resources/palette.png", "调色板\n▾");
        paletteBtn->setPopupMode(QToolButton::InstantPopup);
        paletteBtn->setStyleSheet(
            "QToolButton { border: none; border-radius: 3px; background: transparent; font-size: 11px; }"
            "QToolButton:hover { background: #dce7f5; }"
            "QToolButton:pressed { background: #b8d0ea; }"
            "QToolButton::menu-indicator { image: none; }"
            "QToolButton::down-arrow { image: none; }"
        );
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
            if (m_currentTab) refreshImage();
        });

        paletteBtn->setMenu(paletteMenu);
        zoomBtns->addWidget(paletteBtn);
    }

    // 简易处理 group
    QVBoxLayout *processGroup = addGroup(startLayout, "简易处理");
    QHBoxLayout *processBtns = qobject_cast<QHBoxLayout*>(processGroup->itemAt(0)->layout());
    QToolButton *btnOneClickStart = makeBtn(":/icons/resources/autoprocess.png", "一键处理");
    connect(btnOneClickStart, &QToolButton::clicked, this, &MainWindow::showOneClickProcess);
    processBtns->addWidget(btnOneClickStart);
    QToolButton *btnAdjZero = makeBtn(":/icons/resources/adjustzero.png", "调节零点");
    connect(btnAdjZero, &QToolButton::clicked, this, [this]() {
        if (m_tabs.isEmpty()) return;
        m_leftStack->setCurrentWidget(m_zeroPage);
        m_leftPanel->show();
        if (chartView) {
            chartView->setGainVisible(false);
            chartView->setYScale(20.0f / 511.0f);
            float zeroOff = m_zeroRangePctSpin ? -m_zeroRangePctSpin->value() * 0.2f : 0.0f;
            chartView->setZeroOffset(zeroOff);
            updateChart(m_lastChartX);
        }
    });
    processBtns->addWidget(btnAdjZero);
    QToolButton *btnCorrectOffsetStart = makeBtn(":/icons/resources/correctoffset.png", "校正零偏");
    connect(btnCorrectOffsetStart, &QToolButton::clicked, this, &MainWindow::showCorrectOffset);
    processBtns->addWidget(btnCorrectOffsetStart);
    QToolButton *btnBgRemoveStart = makeBtn(":/icons/resources/bgremove.png", "背景消除");
    connect(btnBgRemoveStart, &QToolButton::clicked, this, &MainWindow::showBackgroundRemoval);
    processBtns->addWidget(btnBgRemoveStart);
    QToolButton *btnAdjGainStart = makeBtn(":/icons/resources/adjustgain.png", "调节增益");
    connect(btnAdjGainStart, &QToolButton::clicked, this, [this]() {
        if (m_tabs.isEmpty()) return;
        m_leftStack->setCurrentWidget(m_gainPage);
        m_leftPanel->setVisible(!m_leftPanel->isVisible());
        if (m_leftPanel->isVisible() && chartView) {
            chartView->setGainVisible(true);
            chartView->setYScale(1.0f);
            QValueAxis *axisY = qobject_cast<QValueAxis*>(chartView->chart()->axisY(chartSeries));
            if (axisY) {
                axisY->setRange(0, 511);
                axisY->setLabelFormat("%d");
            }
            updateChart(m_lastChartX);
        }
    });
    processBtns->addWidget(btnAdjGainStart);
    QToolButton *btnDigFilterStart = makeBtn(":/icons/resources/filter.png", "数字滤波");
    processBtns->addWidget(btnDigFilterStart);
    connect(btnDigFilterStart, &QToolButton::clicked, this, &MainWindow::showDigitalFilter);
    processBtns->addWidget(makeBtn(":/icons/resources/batch.png", "批处理"));

    startLayout->addStretch();
    ribbonTab->addTab(startPage, "开始");

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
        if (m_tabs.isEmpty()) return;
        m_leftStack->setCurrentWidget(m_zeroPage);
        m_leftPanel->show();
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
        if (m_tabs.isEmpty()) return;
        m_leftStack->setCurrentWidget(m_gainPage);
        m_leftPanel->setVisible(!m_leftPanel->isVisible());
        if (m_leftPanel->isVisible() && chartView) {
            chartView->setGainVisible(true);
            chartView->setYScale(1.0f);
            QValueAxis *axisY = qobject_cast<QValueAxis*>(chartView->chart()->axisY(chartSeries));
            if (axisY) {
                axisY->setRange(0, 511);
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
    g2row2->addWidget(makeTextBtn("滑动平均"));
    g2row2->addWidget(makeTextBtn("道间均衡"));
    g2->insertLayout(1, g2row2);

    // Group 3: 其他处理
    QVBoxLayout *g3 = addGroup(dataLayout, "其他处理");
    QHBoxLayout *g3row1 = qobject_cast<QHBoxLayout*>(g3->itemAt(0)->layout());
    g3row1->addWidget(makeTextBtn("数字运算"));
    g3row1->addWidget(makeTextBtn("反褶积"));
    g3row1->addWidget(makeTextBtn("希尔伯特"));
    QHBoxLayout *g3row2 = new QHBoxLayout();
    g3row2->setSpacing(2);
    g3row2->addWidget(makeTextBtn("克西霍夫"));
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
    QSpinBox *startTraceSpin = new QSpinBox();
    startTraceSpin->setRange(0, 99999);
    startTraceSpin->setValue(0);
    startTraceSpin->setFixedWidth(70);
    rangeRow1->addWidget(startTraceSpin);
    rangeRow1->addWidget(new QLabel("终止道"));
    QSpinBox *endTraceSpin = new QSpinBox();
    endTraceSpin->setRange(0, 99999);
    endTraceSpin->setValue(99999);
    endTraceSpin->setFixedWidth(70);
    rangeRow1->addWidget(endTraceSpin);

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

    rangeLayout->addWidget(rangeLabel);
    rangeLayout->addLayout(rangeRow1);
    rangeLayout->addLayout(rangeRow2);
    dataLayout->addWidget(rangeFrame);

    dataLayout->addStretch();
    ribbonTab->addTab(dataPage, "数据处理");

    qobject_cast<QVBoxLayout*>(centralWidget()->layout())->insertWidget(0, ribbonTab);
}

void MainWindow::showDigitalFilter()
{
    if (!m_currentTab) return;

    // If dialog already exists, bring it to front
    if (m_filterDlg) {
        m_filterDlg->raise();
        m_filterDlg->activateWindow();
        return;
    }

    m_filterDlg = new QDialog(this);
    m_filterDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_filterDlg->setWindowFlags(Qt::Tool | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    m_filterDlg->setWindowTitle("数字滤波");
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
        if (!m_currentTab) return;

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

    connect(btnOK, &QPushButton::clicked, m_filterDlg, &QDialog::accept);

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
    if (!m_currentTab) return;

    if (m_bgRemovalDlg) {
        m_bgRemovalDlg->raise();
        m_bgRemovalDlg->activateWindow();
        return;
    }

    m_bgRemovalDlg = new QDialog(this);
    m_bgRemovalDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_bgRemovalDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

    QFileInfo fi(m_currentTab->filePath);
    m_bgRemovalDlg->setWindowTitle(QString("背景消除 - %1").arg(fi.fileName()));

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
    if (!m_currentTab) return;

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

void MainWindow::showCorrectOffset()
{
    if (!m_currentTab) return;

    if (m_correctOffsetDlg) {
        m_correctOffsetDlg->raise();
        m_correctOffsetDlg->activateWindow();
        return;
    }

    m_correctOffsetDlg = new QDialog(this);
    m_correctOffsetDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_correctOffsetDlg->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);
    m_correctOffsetDlg->setWindowTitle("校正参数");

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
    m_correctTimeWindowSpin->setValue(2.0);
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
    if (!m_currentTab) return;

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

    // Dewow: remove DC offset per trace using a time window
    double timeWindowNs = m_correctTimeWindowSpin ? m_correctTimeWindowSpin->value() : 2.0;
    double antennaFreqMHz = m_correctAntennaFreqSpin ? m_correctAntennaFreqSpin->value() : 900.0;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    // Calculate how many samples the time window covers
    double timeRangeSec = m_currentTab->timeRange * 1e-9;
    double sampleInterval = timeRangeSec / samplesPerTrace;
    int windowSamples = qMax(1, static_cast<int>(timeWindowNs * 1e-9 / sampleInterval));

    char *data = m_rawData.data();
    for (int t = 0; t < numTraces; ++t) {
        // Compute mean of first windowSamples in this trace
        double mean = 0.0;
        for (int s = 0; s < windowSamples && s < samplesPerTrace; ++s) {
            int idx = (t * samplesPerTrace + s) * 4;
            qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                         (static_cast<quint8>(data[idx+2]) << 16) |
                         (static_cast<quint8>(data[idx+1]) << 8) |
                         (static_cast<quint8>(data[idx]));
            mean += val;
        }
        mean /= qMin(windowSamples, samplesPerTrace);

        // Subtract mean from entire trace
        for (int s = 0; s < samplesPerTrace; ++s) {
            int idx = (t * samplesPerTrace + s) * 4;
            qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                         (static_cast<quint8>(data[idx+2]) << 16) |
                         (static_cast<quint8>(data[idx+1]) << 8) |
                         (static_cast<quint8>(data[idx]));
            val -= static_cast<qint32>(mean);
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
    if (!m_currentTab) return;

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
    row1->addWidget(m_oneClickCorrectOffset);
    row1->addWidget(new QLabel("时窗:"));
    m_oneClickTimeWindowSpin = new QDoubleSpinBox();
    m_oneClickTimeWindowSpin->setRange(0.0, 99999.0);
    m_oneClickTimeWindowSpin->setDecimals(1);
    m_oneClickTimeWindowSpin->setValue(40.0);
    m_oneClickTimeWindowSpin->setSuffix(" ns");
    row1->addWidget(m_oneClickTimeWindowSpin);
    methodLayout->addLayout(row1);

    QHBoxLayout *row1b = new QHBoxLayout();
    row1b->addStretch();
    row1b->addWidget(new QLabel("天线频率:"));
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
    m_oneClickAmpCompSpin->setValue(20);
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

    // === Right panel: 参考波形 ===
    QGroupBox *chartGroup = new QGroupBox("参考波形");
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
    axisY->setRange(0, 511);
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
        if (!m_oneClickApplied)
            applyOneClickProcess();
        if (m_oneClickApplied)
            saveProcessedFile();
        if (m_oneClickDlg)
            m_oneClickDlg->close();
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

    // Real-time preview: any checkbox/spinbox change updates reference waveform
    connect(m_oneClickCorrectOffset, &QCheckBox::toggled, this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickTimeWindowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickAntennaFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickAmpComp, &QCheckBox::toggled, this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickAmpCompSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickAdjZero, &QCheckBox::toggled, this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickZeroValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickAdjGain, &QCheckBox::toggled, this, [this]() {
        if (m_oneClickChartView) {
            m_oneClickChartView->setGainVisible(m_oneClickAdjGain->isChecked());
            m_oneClickChartView->update();
        }
        if (!m_oneClickApplied) updateOneClickRefChart();
    });
    connect(m_oneClickChartView, &CustomChartView::gainChanged, this, [this]() {
        if (!m_oneClickApplied) updateOneClickRefChart();
    });

    // Initial preview with checked items (e.g. dewow)
    updateOneClickRefChart();

    m_oneClickDlg->show();
}

void MainWindow::applyOneClickProcess()
{
    if (!m_currentTab) return;

    if (m_oneClickApplied) {
        // Undo
        m_rawData = m_currentTab->originalRawData;
        m_currentTab->rawData = m_rawData;
        m_oneClickApplied = false;
        m_oneClickBtnApply->setText("应用");
        refreshImage();
        updateChart(m_lastChartX);

        // Sync reference chart on undo
        if (m_oneClickSeries && !m_rawData.isEmpty()) {
            m_oneClickSeries->clear();
            int spt = m_pixelsPerRow;
            for (int i = 0; i < spt; ++i) {
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
        return;
    }

    // Backup original data
    m_currentTab->originalRawData = m_rawData;

    int samplesPerTrace = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int numTraces = totalPixels / samplesPerTrace;
    if (numTraces == 0 || samplesPerTrace == 0) return;

    char *data = m_rawData.data();

    // Step 1: 校正零偏 (Dewow)
    if (m_oneClickCorrectOffset && m_oneClickCorrectOffset->isChecked()) {
        double timeWindowNs = m_oneClickTimeWindowSpin ? m_oneClickTimeWindowSpin->value() : 40.0;
        double timeRangeSec = m_currentTab->timeRange * 1e-9;
        double sampleInterval = timeRangeSec / samplesPerTrace;
        int windowSamples = qMax(1, static_cast<int>(timeWindowNs * 1e-9 / sampleInterval));

        for (int t = 0; t < numTraces; ++t) {
            double mean = 0.0;
            for (int s = 0; s < windowSamples && s < samplesPerTrace; ++s) {
                int idx = (t * samplesPerTrace + s) * 4;
                qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                             (static_cast<quint8>(data[idx+2]) << 16) |
                             (static_cast<quint8>(data[idx+1]) << 8) |
                             static_cast<quint8>(data[idx]);
                mean += val;
            }
            mean /= qMin(windowSamples, samplesPerTrace);

            for (int s = 0; s < samplesPerTrace; ++s) {
                int idx = (t * samplesPerTrace + s) * 4;
                qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                             (static_cast<quint8>(data[idx+2]) << 16) |
                             (static_cast<quint8>(data[idx+1]) << 8) |
                             static_cast<quint8>(data[idx]);
                val -= static_cast<qint32>(mean);
                data[idx]   = val & 0xFF;
                data[idx+1] = (val >> 8) & 0xFF;
                data[idx+2] = (val >> 16) & 0xFF;
                data[idx+3] = (val >> 24) & 0xFF;
            }
        }
    }

    // Step 2: 幅度补偿 (Amplitude Compensation - exponential gain with depth)
    if (m_oneClickAmpComp && m_oneClickAmpComp->isChecked()) {
        int compValue = m_oneClickAmpCompSpin ? m_oneClickAmpCompSpin->value() : 20;
        if (compValue > 0) {
            // Exponential gain: G(y) = exp(alpha * y * V / 100)
            // alpha chosen so that at y=511, V=100 gives ~10x gain
            double alpha = 2.3 / 511.0; // ln(10) ≈ 2.3, so at V=100, y=511 gives ~10x
            for (int t = 0; t < numTraces; ++t) {
                for (int s = 0; s < samplesPerTrace; ++s) {
                    int idx = (t * samplesPerTrace + s) * 4;
                    qint32 val = (static_cast<quint8>(data[idx+3]) << 24) |
                                 (static_cast<quint8>(data[idx+2]) << 16) |
                                 (static_cast<quint8>(data[idx+1]) << 8) |
                                 static_cast<quint8>(data[idx]);
                    double gain = std::exp(alpha * s * compValue / 100.0);
                    val = static_cast<qint32>(val * gain);
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

    // Step 4: 调节增益 (placeholder - user can use gain panel for this)
    // m_oneClickAdjGain - no additional processing, gain is handled separately

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
    if (!m_oneClickSeries || m_rawData.isEmpty() || !m_currentTab) return;

    if (m_oneClickApplied) return; // after apply, show actual processed data

    m_oneClickSeries->clear();

    int samplesPerTrace = m_pixelsPerRow;

    // Read raw 512 samples for current trace
    QVector<qint32> samples(samplesPerTrace);
    for (int i = 0; i < samplesPerTrace; ++i)
        samples[i] = getPixelValue(m_lastChartX, i);

    // Preview: 校正零偏 (Dewow)
    if (m_oneClickCorrectOffset && m_oneClickCorrectOffset->isChecked()) {
        double timeWindowNs = m_oneClickTimeWindowSpin ? m_oneClickTimeWindowSpin->value() : 40.0;
        double timeRangeSec = m_currentTab->timeRange * 1e-9;
        double sampleInterval = timeRangeSec / samplesPerTrace;
        int windowSamples = qMax(1, static_cast<int>(timeWindowNs * 1e-9 / sampleInterval));
        double mean = 0.0;
        for (int s = 0; s < windowSamples && s < samplesPerTrace; ++s)
            mean += samples[s];
        mean /= qMin(windowSamples, samplesPerTrace);
        for (int s = 0; s < samplesPerTrace; ++s)
            samples[s] -= static_cast<qint32>(mean);
    }

    // Preview: 幅度补偿 (Amplitude Compensation)
    if (m_oneClickAmpComp && m_oneClickAmpComp->isChecked() && m_oneClickAmpCompSpin) {
        int compValue = m_oneClickAmpCompSpin->value();
        if (compValue > 0) {
            const double alpha = 4.60517 / 511.0; // ln(100)/511
            for (int i = 0; i < samplesPerTrace; ++i) {
                double gain = std::exp(alpha * i * compValue / 100.0);
                samples[i] = static_cast<qint32>(samples[i] * gain);
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
        for (int i = 0; i < samplesPerTrace; ++i) {
            float g = m_oneClickChartView->interpolatedGain(i);
            float gainLinear = std::pow(10.0f, g / 20.0f);
            samples[i] = static_cast<qint32>(samples[i] * gainLinear);
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
    if (m_oneClickChartView) m_oneClickChartView->update();
}
