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
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QMenuBar>
#include <QQuickItem>
#include <QTabWidget>
#include <QToolButton>
#include <QFrame>
#include <QGridLayout>
#include <QScrollBar>
#include <QFontMetrics>
#include <QTimer>
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
    m_lineCount = qBound(0, count, 9);
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

qreal CustomChartView::mapChartToWidgetX(float x)
{
    if (!m_series || !chart()) return 0;
    return chart()->mapToPosition(QPointF(x, 0), m_series).x();
}

float CustomChartView::mapWidgetToChartX(qreal widgetX)
{
    if (!m_series || !chart()) return 0;
    return static_cast<float>(chart()->mapToValue(QPointF(widgetX, 0), m_series).x());
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

    for (int i = 0; i < m_lineCount; ++i) {
        qreal wy = mapChartToWidgetY(m_lineY[i]);
        qreal hx = mapChartToWidgetX(m_handleX[i]);

        QPen pen(Qt::red, 1);
        painter.setPen(pen);
        painter.drawLine(QPointF(plotArea.left(), wy), QPointF(plotArea.right(), wy));

        painter.setBrush(Qt::red);
        painter.drawRect(QRectF(hx - 3, wy - 3, 6, 6));
    }
}

void CustomChartView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();
        for (int i = 0; i < m_lineCount; ++i) {
            qreal wy = mapChartToWidgetY(m_lineY[i]);
            qreal hx = mapChartToWidgetX(m_handleX[i]);
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
        m_handleX[m_draggingIdx] = mapWidgetToChartX(event->pos().x());
        update();
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
    setMinimumSize(400, 400);
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

    // Minor ticks every 10 traces
    int firstMinor = (startTrace / 10) * 10;
    if (firstMinor < startTrace) firstMinor += 10;

    for (int val = firstMinor; val <= endTrace; val += 10) {
        int x = val - m_offset;
        if (val % 100 != 0)
            p.drawLine(x, h - 5, x, h);
    }

    // Major ticks + labels every 100 traces
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
    setMinimumHeight(100);
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
    int imgH = m_imageHeight;

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
        // Left ruler: major tick every 2.5ns, 10 minor ticks between majors
        double majorInterval = 2.5;
        double minorInterval = majorInterval / 10.0;  // 0.25ns

        // Minor ticks
        double firstMinor = ceil(m_minVal / minorInterval) * minorInterval;
        for (double val = firstMinor; val <= m_maxVal; val += minorInterval) {
            double fraction = (val - m_minVal) / range;
            int y = (int)(fraction * imgH);
            bool isMajor = qAbs(fmod(val, majorInterval)) < 1e-9
                        || qAbs(fmod(val, majorInterval) - majorInterval) < 1e-9;
            if (!isMajor)
                p.drawLine(w - 4, y, w, y);
        }

        // Major ticks + labels
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
        // Right ruler: major tick every 0.25m, 10 minor ticks between majors
        double majorInterval = 0.25;
        double minorInterval = majorInterval / 10.0;  // 0.025m

        // Minor ticks
        double firstMinor = ceil(m_minVal / minorInterval) * minorInterval;
        for (double val = firstMinor; val <= m_maxVal; val += minorInterval) {
            double fraction = (val - m_minVal) / range;
            int y = (int)(fraction * imgH);
            bool isMajor = qAbs(fmod(val, majorInterval)) < 1e-9
                        || qAbs(fmod(val, majorInterval) - majorInterval) < 1e-9;
            if (!isMajor)
                p.drawLine(0, y, 4, y);
        }

        // Major ticks + labels
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dataOffset(0)
    , m_pixelsPerRow(512)
    , m_gain(1.0f)
    , m_transformMode(0)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 创建水平布局，左边放图表，右边放图片
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // 创建滚动区域
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(false);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    imageLabel = new ImageLabel(this);
    scrollArea->setWidget(imageLabel);

    // 创建垂直图表：Y轴为Y坐标（从上到下），X轴为像素值
    chartView = new CustomChartView();
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet("border: none; background-color: transparent;");
    chartView->setMinimumWidth(200);
    chartView->setMaximumWidth(300);

    chartSeries = new QLineSeries();
    chartView->chart()->addSeries(chartSeries);
    chartView->chart()->legend()->hide();
    chartView->setLineSeries(chartSeries);

    // X轴：像素值（动态更新范围）
    QValueAxis *axisX = new QValueAxis();
    axisX->setRange(-100, 100);
    axisX->setLabelFormat("%d");
    axisX->setTickCount(5);

    // Y轴：Y坐标（反转，0在顶部，511在底部，与图片对齐）
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 511);
    axisY->setTickCount(6);
    axisY->setLabelFormat("%d");
    axisY->setReverse(true);

    chartView->chart()->setAxisX(axisX, chartSeries);
    chartView->chart()->setAxisY(axisY, chartSeries);

    chartView->chart()->setAnimationOptions(QChart::NoAnimation);

    // 增益调整表格（2列x10行）
    gainTable = new QTableWidget(10, 2);
    gainTable->horizontalHeader()->hide();
    gainTable->verticalHeader()->setVisible(false);
    gainTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gainTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gainTable->setSelectionMode(QAbstractItemView::NoSelection);
    gainTable->setMaximumWidth(300);
    gainTable->setMinimumWidth(200);
    gainTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 预填充增益行（行1-9）
    for (int i = 1; i <= 9; ++i) {
        QTableWidgetItem *labelItem = new QTableWidgetItem("");
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
        gainTable->setItem(i, 0, labelItem);
        gainTable->setItem(i, 1, new QTableWidgetItem(""));
    }

    // 第一行：点数 + SpinBox（默认0，最大9）
    gainTable->setItem(0, 0, new QTableWidgetItem("点数"));
    QSpinBox *pointSpinBox = new QSpinBox();
    pointSpinBox->setRange(0, 9);
    pointSpinBox->setValue(0);
    gainTable->setCellWidget(0, 1, pointSpinBox);

    // 点数变化时更新增益行显示和chart水平线
    connect(pointSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int count) {
        for (int i = 1; i <= 9; ++i) {
            if (i <= count) {
                gainTable->item(i, 0)->setText(QString("增益%1").arg(i));
            } else {
                gainTable->item(i, 0)->setText("");
                gainTable->item(i, 1)->setText("");
            }
        }
        chartView->setLineCount(count);
    });

    // 左侧增益表格
    gainTable->setMaximumWidth(250);
    gainTable->setMinimumWidth(200);

    // 右侧chart（拉满高度）
    chartView->setMinimumWidth(200);
    chartView->setMaximumWidth(300);

    // Create rulers
    m_topRuler = new HRulerWidget(this);
    m_leftRuler = new VRulerWidget(VRulerWidget::Left, this);
    m_rightRuler = new VRulerWidget(VRulerWidget::Right, this);

    m_leftRuler->setLabel(QString::fromUtf8("时间标尺(ns)"));
    m_rightRuler->setLabel(QString::fromUtf8("深度标尺(m)"));

    // Grid layout: rulers + scrollArea
    QGridLayout *imageGrid = new QGridLayout();
    imageGrid->setSpacing(0);
    imageGrid->setContentsMargins(0, 0, 0, 0);

    topLeftCorner = new QWidget();
    topLeftCorner->setFixedSize(60, 40);
    topLeftCorner->setStyleSheet("background: #f5f5f5;");
    topRightCorner = new QWidget();
    topRightCorner->setFixedSize(60, 40);
    topRightCorner->setStyleSheet("background: #f5f5f5;");

    imageGrid->addWidget(topLeftCorner, 0, 0);
    imageGrid->addWidget(m_topRuler, 0, 1);
    imageGrid->addWidget(topRightCorner, 0, 2);
    imageGrid->addWidget(m_leftRuler, 1, 0, Qt::AlignTop);
    imageGrid->addWidget(scrollArea, 1, 1);
    imageGrid->addWidget(m_rightRuler, 1, 2, Qt::AlignTop);

    imageGrid->setColumnStretch(1, 1);

    // Welcome image (replaces the three data controls area)
    welcomeLabel = new QLabel(this);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLabel->setStyleSheet("background: #2b2b2b;");
    QPixmap welcomePix(":/icons/resources/welcome.png");
    welcomeLabel->setPixmap(welcomePix.scaled(800, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    contentLayout->addWidget(gainTable);
    contentLayout->addWidget(welcomeLabel, 1);
    contentLayout->addLayout(imageGrid);
    contentLayout->addWidget(chartView);

    // Initially hide data controls, show welcome
    gainTable->hide();
    m_topRuler->hide();
    m_leftRuler->hide();
    m_rightRuler->hide();
    topLeftCorner->hide();
    topRightCorner->hide();
    scrollArea->hide();
    chartView->hide();

    welcomeLabel->show();

    // QQuickWidget 已移除，扩大图片显示区域

    mainLayout->addLayout(contentLayout);

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

    // 创建菜单栏
    createMenuBar();

    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(imageLabel, &ImageLabel::imageClicked, this, &MainWindow::onImageClicked);
    connect(imageLabel, &ImageLabel::transformSelected, this, [this](int mode) {
        m_transformMode = mode;
        refreshImage();
    });
    connect(imageLabel, &ImageLabel::gainSelected, this, [this](float gainDb) {
        m_gain = pow(10.0f, gainDb / 20.0f);
        m_transformMode = 0;
        refreshImage();
    });
    connect(scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_topRuler, &HRulerWidget::setOffset);
}

MainWindow::~MainWindow()
{
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open DZT File", "",
        "DZT Files (*.dzt);;All Files (*)");

    if (fileName.isEmpty()) {
        return;
    }

    QImage image = loadDZTFile(fileName);

    if (image.isNull()) {
        QMessageBox::warning(this, "Error", "Failed to load DZT file.");
        return;
    }

    imageLabel->setImage(image);
    coordinateLabel->setText("点击图片查看坐标");

    // Hide welcome, show data controls FIRST
    welcomeLabel->hide();
    gainTable->show();
    m_topRuler->show();
    m_leftRuler->show();
    m_rightRuler->show();
    topLeftCorner->show();
    topRightCorner->show();
    scrollArea->show();
    chartView->show();

    updateRulers();
    resizeImageLabel();
}

void MainWindow::onImageClicked(const QPoint &pos)
{
    updateCoordinateLabel(pos.x(), pos.y());
}

void MainWindow::updateCoordinateLabel(int x, int y)
{
    if (m_rawData.isEmpty()) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    qint32 pixelValue = getPixelValue(x, y);
    double normalizedValue = static_cast<double>(pixelValue) / (256.0 * 256.0 * 2.0);

    coordinateLabel->setText(QString("坐标: X=%1, Y=%2 | Pixel值: %3 | 归一化值: %4")
                           .arg(x)
                           .arg(y)
                           .arg(pixelValue)
                           .arg(normalizedValue, 0, 'f', 6));

    // 更新图表显示
    updateChart(x);
}

void MainWindow::updateChart(int xValue)
{
    if (m_rawData.isEmpty()) {
        return;
    }

    chartSeries->clear();

    const int maxPoints = 512;
    qint32 minVal = 0, maxVal = 0;

    for (int y = 0; y < maxPoints; ++y) {
        qint32 pixelValue = getPixelValue(xValue, y);
        chartSeries->append(static_cast<qreal>(pixelValue), static_cast<qreal>(y));
        if (y == 0 || pixelValue < minVal) minVal = pixelValue;
        if (y == 0 || pixelValue > maxVal) maxVal = pixelValue;
    }

    // 动态更新X轴范围
  // QValueAxis *axisX = qobject_cast<QValueAxis*>(chartView->chart()->axisX(chartSeries));
  // if (axisX) {
  //     qint32 margin = qMax<qint32>(1, (maxVal - minVal) / 10);
  //     axisX->setRange(static_cast<qreal>(minVal - margin), static_cast<qreal>(maxVal + margin));
  // }

    QValueAxis *axisX = qobject_cast<QValueAxis*>(chartView->chart()->axisX(chartSeries));
   // if (axisX) {
    //    qint32 margin = qMax<qint32>(1, (maxVal - minVal) / 10);
        axisX->setRange(-256*256*256/2, 256*256*256/2);
  //  }
}

qint32 MainWindow::getPixelValue(int x, int y)
{
    if (m_rawData.isEmpty()) {
        return 0;
    }

    const int bytesPerPixel = 4;
    int dataIdx = (x * m_pixelsPerRow + y) * bytesPerPixel;

    if (dataIdx + 4 > m_rawData.size()) {
        return 0;
    }

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
    //QFile file(filePath);
    QFile file("D:/gpr_software/specs/1103_010.DZT");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "open file failed.");
        return QImage();
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    //const qint64 dataOffset = 0x8200;  // fix bug
    const qint64 dataOffset = 0x20000;  // fix bug
    const int bytesPerPixel = 4;
    const int pixelsPerRow = 512;

    // 存储原始数据供点击时使用
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
    qDebug() << "dataSize = "  <<  dataSize;
    qDebug() << "row = "  <<  rows;

    //if (rows <= 0 || totalPixels % pixelsPerRow != 0) {
    //    QMessageBox::warning(this, "Error", "open file failed ##3.");
    //    return QImage();
   // }

    //QImage image(pixelsPerRow, rows, QImage::Format_Grayscale8);
    QImage image(rows,pixelsPerRow,  QImage::Format_Grayscale8);    //// first 1024 colunm for test and x yx reverse
    
    qDebug() << image.format();

    float gain = m_gain;

    int pixelValue_display;

    int dataIdx = 0;
    //for (int y = 0; y < rows; ++y) {
    for (int y = 0; y < rows; ++y) {        // first 1024 colunm for test
        for (int x = 0; x < pixelsPerRow; ++x) {
            if (dataIdx + 4 > dataSize) {
                return QImage();
             }
            qint32 pixelValue = static_cast<qint32>(
                (static_cast<quint8>(m_rawData[dataIdx + 3]) << 24) |
                (static_cast<quint8>(m_rawData[dataIdx + 2]) << 16) |
                (static_cast<quint8>(m_rawData[dataIdx + 1]) << 8) |
                (static_cast<quint8>(m_rawData[dataIdx]))
            );

            //quint8 grayValue = static_cast<quint8>(qBound(0, pixelValue, 255));
            //image.setPixel(x, y, grayValue);   //
            /*    处理以127为中心 0 最黑       */
            //image.setPixel(y, x, 127 + grayValue/(256*256*2));   //   x y reverse  ??? 是否是127 需要测试
            //image.setPixel(y, x, 127 + pixelValue/(256*256*2));   //   x y reverse  ??? 是否是127 需要测试
            //image.setPixel(y, x, 127 + data[dataIdx + 2]/2);   //   x y reverse  ??? 是否是127 需要测试
            //image.setPixel(y, x, qRgb(127 + data[dataIdx + 2]/2,127 + data[dataIdx + 2]/2,127 + data[dataIdx + 2]/2));   //   x y reverse  ??? 是否是127 需要测试
            if(gain*pixelValue>=256*256*256/2)
                pixelValue_display = 256*256*256/2 -1 ;
            else if (gain*pixelValue<=-256*256*256/2)
                pixelValue_display = -256*256*256/2 +1 ;
            else
                pixelValue_display = gain*pixelValue;

             //image.setPixel(y, x, qRgb(127 + gain*pixelValue/(256*256),127 + gain*pixelValue/(256*256),127 + gain*pixelValue/(256*256)));
             image.setPixel(y, x, qRgb(127 + pixelValue_display/(256*256), 127+ pixelValue_display/(256*256), 127 + pixelValue_display/(256*256)));

            dataIdx += 4;
        }
    }
    image = image.convertToFormat(QImage::Format_Grayscale8);
    return image;
}

void MainWindow::refreshImage()
{
    if (m_rawData.isEmpty()) return;

    const int pixelsPerRow = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int rows = totalPixels / pixelsPerRow;

    QImage image(rows, pixelsPerRow, QImage::Format_Grayscale8);

    if (m_transformMode == 3) {
        // FFT模式：每列512点FFT，显示前256个频率分量的幅度
        for (int col = 0; col < rows; ++col) {
            std::vector<std::complex<double>> data(512);
            for (int y = 0; y < 512; ++y)
                data[y] = std::complex<double>(getPixelValue(col, y), 0.0);

            fft(data);

            // 找最大幅度用于归一化
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
        // 非FFT模式：原始渲染逻辑
        int dataSize = m_rawData.size();
        float gain = m_gain;
        int pixelValue_display;
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

                if (gain * pixelValue >= 256 * 256 * 256 / 2)
                    pixelValue_display = 256 * 256 * 256 / 2 - 1;
                else if (gain * pixelValue <= -256 * 256 * 256 / 2)
                    pixelValue_display = -256 * 256 * 256 / 2 + 1;
                else
                    pixelValue_display = gain * pixelValue;

                if (m_transformMode == 1)
                    pixelValue_display = qAbs(pixelValue_display);
                else if (m_transformMode == 2)
                    pixelValue_display = -pixelValue_display;

                image.setPixel(y, x, qRgb(127 + pixelValue_display / (256 * 256),
                                           127 + pixelValue_display / (256 * 256),
                                           127 + pixelValue_display / (256 * 256)));
                dataIdx += 4;
            }
        }
    }

    image = image.convertToFormat(QImage::Format_Grayscale8);
    imageLabel->setImage(image);
}

void MainWindow::updateRulers()
{
    if (m_rawData.isEmpty()) return;

    int totalPixels = m_rawData.size() / 4;
    m_traceCount = totalPixels / m_pixelsPerRow;

    m_timeRange = 20.0;   // 512 samples = 20.0 ns
    m_depthRange = 1.25;  // 20.0 ns corresponds to 1.25 m

    m_topRuler->setDataRange(m_traceCount);

    m_leftRuler->setRange(0, m_timeRange);
    m_rightRuler->setRange(0, m_depthRange);
}

void MainWindow::resizeImageLabel()
{
    if (m_rawData.isEmpty()) return;

    int viewH = scrollArea->viewport()->height();
    if (viewH <= 0) viewH = m_pixelsPerRow;

    imageLabel->setFixedSize(m_traceCount, viewH);
    scrollArea->horizontalScrollBar()->setRange(0, qMax(0, m_traceCount - scrollArea->viewport()->width()));
    scrollArea->horizontalScrollBar()->setPageStep(scrollArea->viewport()->width());

    m_leftRuler->setImageHeight(viewH);
    m_leftRuler->setFixedHeight(viewH);
    m_rightRuler->setImageHeight(viewH);
    m_rightRuler->setFixedHeight(viewH);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    resizeImageLabel();
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

    // Helper lambda: create a group with a frame and label
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
        m_rawData.clear();
        imageLabel->clear();
        chartSeries->clear();
        coordinateLabel->setText("点击图片查看坐标");

        welcomeLabel->show();
        gainTable->hide();
        m_topRuler->hide();
        m_leftRuler->hide();
        m_rightRuler->hide();
        topLeftCorner->hide();
        topRightCorner->hide();
        scrollArea->hide();
        chartView->hide();
    });

    // 图像缩放 group
    QVBoxLayout *zoomGroup = addGroup(startLayout, "图像缩放");
    QHBoxLayout *zoomBtns = qobject_cast<QHBoxLayout*>(zoomGroup->itemAt(0)->layout());
    zoomBtns->addWidget(makeBtn(":/icons/resources/hzoomin.png", "水平放大"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/hzoomout.png", "水平缩小"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/restore.png", "图像还原"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/stack.png", "堆积图"));
    zoomBtns->addWidget(makeBtn(":/icons/resources/palette.png", "调色板"));

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

    // --- Tab: 数据处理 (placeholder) ---
    QWidget *dataPage = new QWidget();
    ribbonTab->addTab(dataPage, "数据处理");

    // Insert ribbon at top of main layout
    qobject_cast<QVBoxLayout*>(centralWidget()->layout())->insertWidget(0, ribbonTab);
}
