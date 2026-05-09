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
#include <QScrollBar>
#include <QFontMetrics>
#include <QTimer>
#include <QFileInfo>
#include <functional>
#include <cmath>
#include <complex>
#include <vector>

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

    // Draw gain scale ticks at top of plot
    QPen tickPen(Qt::gray, 1, Qt::DotLine);
    painter.setPen(tickPen);
    QFontMetrics fm(painter.font());
    int tickCount = static_cast<int>((m_gainMax - m_gainMin) / 6);
    if (tickCount < 1) tickCount = 1;
    for (int t = -tickCount; t <= tickCount; ++t) {
        qreal tx = mapGainToWidgetX(6.0f * t);
        if (tx >= plotArea.left() && tx <= plotArea.right()) {
            painter.drawLine(QPointF(tx, plotArea.top()), QPointF(tx, plotArea.bottom()));
            QString label = QString::number(6 * t);
            painter.setPen(Qt::black);
            painter.drawText(QPointF(tx - fm.horizontalAdvance(label) / 2, plotArea.top() - 2), label);
            painter.setPen(tickPen);
        }
    }

    for (int i = 0; i < m_lineCount; ++i) {
        qreal wy = mapChartToWidgetY(m_lineY[i]);
        qreal hx = mapGainToWidgetX(m_handleX[i]);

        QPen pen(Qt::red, 1);
        painter.setPen(pen);
        painter.drawLine(QPointF(plotArea.left(), wy), QPointF(plotArea.right(), wy));

        painter.setBrush(Qt::red);
        painter.drawRect(QRectF(hx - 4, wy - 4, 8, 8));

        // Connect handle N to handle N+1
        if (i > 0) {
            qreal prevWy = mapChartToWidgetY(m_lineY[i - 1]);
            qreal prevHx = mapGainToWidgetX(m_handleX[i - 1]);
            painter.drawLine(QPointF(prevHx, prevWy), QPointF(hx, wy));
        }
    }
}

void CustomChartView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();
        for (int i = 0; i < m_lineCount; ++i) {
            qreal wy = mapChartToWidgetY(m_lineY[i]);
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
        m_handleX[m_draggingIdx] = mapWidgetToGainX(event->pos().x());
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

    p.save();
    if (m_direction == Left) {
        p.translate(12, imgH / 2);
        p.rotate(-90);
    } else {
        p.translate(w - 12, imgH / 2);
        p.rotate(90);
    }
    p.drawText(QRect(-150, -8, 300, 16), Qt::AlignCenter, m_label);
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
        float maxAbs = 6.0f;
        // Check overall gain spinbox (index 0)
        if (m_gainSpinBoxes[0]) {
            float val = static_cast<float>(qAbs(m_gainSpinBoxes[0]->value()));
            if (val > maxAbs) maxAbs = val;
        }
        // Check individual gain spinboxes (index 1..16)
        for (int i = 1; i <= 16; ++i) {
            if (m_gainSpinBoxes[i] && !m_gainSpinBoxes[i]->isHidden()) {
                float val = static_cast<float>(qAbs(m_gainSpinBoxes[i]->value()));
                if (val > maxAbs) maxAbs = val;
            }
        }
        float n = std::ceil(maxAbs / 6.0f);
        if (n < 1.0f) n = 1.0f;
        float range = 6.0f * n;
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
            overallGainItem->setHidden(!isAuto);
            if (isAuto) {
                // 自动模式：隐藏所有增益#行
                for (int i = 1; i <= 16; ++i)
                    gainItems[i]->setHidden(true);
            } else {
                int count = pointSpinBox->value();
                for (int i = 1; i <= 16; ++i)
                    gainItems[i]->setHidden(i > count);
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

    // Left panel: gain tree + buttons
    m_leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(gainTree);

    // Buttons row
    QHBoxLayout *gainBtnLayout = new QHBoxLayout;
    m_btnApply = new QPushButton("应用");
    m_btnOK = new QPushButton("确定");
    m_btnCancel = new QPushButton("取消");
    m_btnApply->setEnabled(false);
    m_btnOK->setEnabled(false);
    m_btnCancel->setEnabled(false);
    gainBtnLayout->addWidget(m_btnApply);
    gainBtnLayout->addWidget(m_btnOK);
    gainBtnLayout->addWidget(m_btnCancel);
    leftLayout->addLayout(gainBtnLayout);

    connect(m_btnApply, &QPushButton::clicked, this, &MainWindow::applyGain);
    connect(m_btnOK, &QPushButton::clicked, this, &MainWindow::saveProcessedFile);
    connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
        if (m_currentTab && m_currentTab->gainApplied) {
            m_rawData = m_currentTab->originalRawData;
            m_currentTab->rawData = m_rawData;
            m_currentTab->gainApplied = false;
            m_btnApply->setText("应用");
            refreshImage();
            updateChart(m_lastChartX);
        }
    });

    contentLayout->addWidget(m_leftPanel);
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

    openButton = new QPushButton("Open DZT File", this);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(coordinateLabel);

    mainLayout->addLayout(buttonLayout);

    setCentralWidget(centralWidget);
    setWindowTitle("DZT Image Viewer");
    resize(1200, 700);

    createMenuBar();
    loadLUT(12);

    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
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

    tab->chartView->chart()->setAxisX(axisX, tab->chartSeries);
    tab->chartView->chart()->setAxisY(axisY, tab->chartSeries);
    tab->chartView->chart()->setAnimationOptions(QChart::NoAnimation);

    // Drag handle → update spinbox (auto mode: sync all handles)
    connect(tab->chartView, &CustomChartView::gainChanged, this, [this](int idx, float val) {
        bool isAuto = m_gainTypeCombo && m_gainTypeCombo->currentIndex() == 0;
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
        } else {
            if (idx >= 0 && idx < 16 && m_gainSpinBoxes[idx + 1]) {
                m_gainSpinBoxes[idx + 1]->blockSignals(true);
                m_gainSpinBoxes[idx + 1]->setValue(static_cast<double>(val));
                m_gainSpinBoxes[idx + 1]->blockSignals(false);
            }
        }
        // Auto-expand gain range from ALL spinbox max value
        if (chartView) {
            float maxAbs = 6.0f;
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
            float n = std::ceil(maxAbs / 6.0f);
            if (n < 1.0f) n = 1.0f;
            chartView->setGainRange(-6.0f * n, 6.0f * n);
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

    // Initialize chart handles with current point count
    if (tab->chartView) {
        // pointSpinBox is local to constructor; read current setting from gainTree
        int pointCount = 1;  // default
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
        float range = 6.0f * pointCount;
        tab->chartView->setGainRange(-range, range);
        for (int i = 0; i < actual; ++i)
            tab->chartView->setHandleX(i, 0.0f);
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
    m_leftPanel->show();
    m_docTabWidget->show();
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

    for (int y = 0; y < maxPoints; ++y) {
        qint32 pixelValue = getPixelValue(xValue, y);
        float rowGainDb = (chartView) ? chartView->interpolatedGain(y) : m_gain;
        float rowGainLinear = std::pow(10.0f, rowGainDb / 20.0f);
        qint32 displayValue = static_cast<qint32>(rowGainLinear * pixelValue);
        chartSeries->append(static_cast<qreal>(displayValue), static_cast<qreal>(y));
        if (y == 0 || displayValue < minVal) minVal = displayValue;
        if (y == 0 || displayValue > maxVal) maxVal = displayValue;
    }

    QValueAxis *axisX = qobject_cast<QValueAxis*>(chartView->chart()->axisX(chartSeries));
    axisX->setRange(-256*256*256/2, 256*256*256/2);
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

    if (!file.seek(dataOffset)) {
        QMessageBox::warning(this, "Error", "open file failed.");
        return QImage();
    }

    m_rawData = file.readAll();
    int dataSize = m_rawData.size();
    int totalPixels = dataSize / bytesPerPixel;
    int rows = totalPixels / pixelsPerRow;
    qDebug() << "dataSize = " << dataSize;
    qDebug() << "row = " << rows;

    QImage image(rows, pixelsPerRow, QImage::Format_RGB32);

    qDebug() << image.format();

    float gain = m_gain;

    qDebug() << "gain = " << gain;

    int dataIdx = 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < pixelsPerRow; ++x) {
            if (dataIdx + 4 > dataSize) {
                return QImage();
            }
            qint32 pixelValue;
            if (x == 0 || x == 1) {
                pixelValue = 0;
                m_rawData[dataIdx + 3] = 0;
                m_rawData[dataIdx + 2] = 0;
                m_rawData[dataIdx + 1] = 0;
                m_rawData[dataIdx] = 0;
            } else {
                pixelValue = static_cast<qint32>(
                    (static_cast<quint8>(m_rawData[dataIdx + 3]) << 24) |
                    (static_cast<quint8>(m_rawData[dataIdx + 2]) << 16) |
                    (static_cast<quint8>(m_rawData[dataIdx + 1]) << 8) |
                    (static_cast<quint8>(m_rawData[dataIdx]))
                );
            }

            int pixelValue_display = pixelValue;
            int lutIdx = pixelValue_display / (256 * 256) + 128;
            if (lutIdx < 0) lutIdx = 0;
            if (lutIdx > 255) lutIdx = 255;
            image.setPixel(y, x, m_lut[lutIdx]);

            dataIdx += 4;
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

        float gainTable[512];
        for (int x = 0; x < 512; ++x) {
            float dbVal = (chartView) ? chartView->interpolatedGain(x) : m_gain;
            gainTable[x] = std::pow(10.0f, dbVal / 20.0f);
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
        float gainTable[512];
        for (int x = 0; x < 512; ++x) {
            float dbVal = (chartView) ? chartView->interpolatedGain(x) : m_gain;
            gainTable[x] = std::pow(10.0f, dbVal / 20.0f);
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

    QImage image(rows, pixelsPerRow, QImage::Format_RGB32);

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

        int dataIdx = 0;
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < pixelsPerRow; ++x) {
                if (dataIdx + 4 > dataSize) return;
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

                int lutIdx = pixelValue_display / (256 * 256) + 128;
                if (lutIdx < 0) lutIdx = 0;
                if (lutIdx > 255) lutIdx = 255;
                image.setPixel(y, x, m_lut[lutIdx]);
                dataIdx += 4;
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

    m_timeRange = 20.0;
    m_depthRange = 1.25;
    m_currentTab->timeRange = m_timeRange;
    m_currentTab->depthRange = m_depthRange;

    m_currentTab->topRuler->setDataRange(m_traceCount);
    m_currentTab->leftRuler->setRange(0, m_timeRange);
    m_currentTab->rightRuler->setRange(0, m_depthRange);
}

void MainWindow::resizeImageLabel()
{
    if (!m_currentTab || m_rawData.isEmpty()) return;

    int viewH = m_currentTab->scrollArea->viewport()->height();
    if (viewH <= 0) viewH = m_pixelsPerRow;

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
    fileBtns->addWidget(btnOpen);
    fileBtns->addWidget(btnSave);
    fileBtns->addWidget(btnClose);

    connect(btnOpen, &QToolButton::clicked, this, &MainWindow::onOpenFile);
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
    processBtns->addWidget(makeBtn(":/icons/resources/autoprocess.png", "一键处理"));
    processBtns->addWidget(makeBtn(":/icons/resources/adjustzero.png", "调节零点"));
    processBtns->addWidget(makeBtn(":/icons/resources/correctoffset.png", "校正零偏"));
    processBtns->addWidget(makeBtn(":/icons/resources/bgremove.png", "背景消除"));
    processBtns->addWidget(makeBtn(":/icons/resources/adjustgain.png", "调节增益"));
    processBtns->addWidget(makeBtn(":/icons/resources/filter.png", "数字滤波"));
    processBtns->addWidget(makeBtn(":/icons/resources/batch.png", "批处理"));

    startLayout->addStretch();
    ribbonTab->addTab(startPage, "开始");

    // --- Tab: 数据处理 ---
    QWidget *dataPage = new QWidget();
    ribbonTab->addTab(dataPage, "数据处理");

    qobject_cast<QVBoxLayout*>(centralWidget()->layout())->insertWidget(0, ribbonTab);
}
