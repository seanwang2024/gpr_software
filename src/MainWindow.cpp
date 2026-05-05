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
#include <QTabWidget>
#include <QToolButton>
#include <QFrame>
#include <QGridLayout>
#include <QScrollBar>
#include <QFontMetrics>
#include <QTimer>
#include <QFileInfo>
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

    // --- Shared: gain table ---
    gainTable = new QTableWidget(10, 2);
    gainTable->horizontalHeader()->hide();
    gainTable->verticalHeader()->setVisible(false);
    gainTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gainTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gainTable->setSelectionMode(QAbstractItemView::NoSelection);
    gainTable->setMaximumWidth(250);
    gainTable->setMinimumWidth(200);
    gainTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (int i = 1; i <= 9; ++i) {
        QTableWidgetItem *labelItem = new QTableWidgetItem("");
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
        gainTable->setItem(i, 0, labelItem);
        gainTable->setItem(i, 1, new QTableWidgetItem(""));
    }

    gainTable->setItem(0, 0, new QTableWidgetItem("点数"));
    QSpinBox *pointSpinBox = new QSpinBox();
    pointSpinBox->setRange(0, 9);
    pointSpinBox->setValue(0);
    gainTable->setCellWidget(0, 1, pointSpinBox);

    connect(pointSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int count) {
        for (int i = 1; i <= 9; ++i) {
            if (i <= count) {
                gainTable->item(i, 0)->setText(QString("增益%1").arg(i));
            } else {
                gainTable->item(i, 0)->setText("");
                gainTable->item(i, 1)->setText("");
            }
        }
        if (chartView) chartView->setLineCount(count);
    });

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

    contentLayout->addWidget(gainTable);
    contentLayout->addWidget(welcomeLabel, 1);
    contentLayout->addWidget(m_docTabWidget, 1);

    // Initially show welcome, hide others
    gainTable->hide();
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
    gainTable->hide();
    m_docTabWidget->hide();
    welcomeLabel->show();
}

void MainWindow::hideWelcome()
{
    welcomeLabel->hide();
    gainTable->show();
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

    chartSeries->clear();

    const int maxPoints = 512;
    qint32 minVal = 0, maxVal = 0;

    for (int y = 0; y < maxPoints; ++y) {
        qint32 pixelValue = getPixelValue(xValue, y);
        chartSeries->append(static_cast<qreal>(pixelValue), static_cast<qreal>(y));
        if (y == 0 || pixelValue < minVal) minVal = pixelValue;
        if (y == 0 || pixelValue > maxVal) maxVal = pixelValue;
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

    QImage image(rows, pixelsPerRow, QImage::Format_Grayscale8);

    qDebug() << image.format();

    float gain = m_gain;

    int pixelValue_display;

    int dataIdx = 0;
    for (int y = 0; y < rows; ++y) {
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

            if(gain*pixelValue>=256*256*256/2)
                pixelValue_display = 256*256*256/2 -1 ;
            else if (gain*pixelValue<=-256*256*256/2)
                pixelValue_display = -256*256*256/2 +1 ;
            else
                pixelValue_display = gain*pixelValue;

            image.setPixel(y, x, qRgb(127 + pixelValue_display/(256*256), 127+ pixelValue_display/(256*256), 127 + pixelValue_display/(256*256)));

            dataIdx += 4;
        }
    }
    image = image.convertToFormat(QImage::Format_Grayscale8);
    return image;
}

void MainWindow::refreshImage()
{
    if (!m_currentTab || m_rawData.isEmpty()) return;

    const int pixelsPerRow = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / 4;
    int rows = totalPixels / pixelsPerRow;

    QImage image(rows, pixelsPerRow, QImage::Format_Grayscale8);

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

    // --- Tab: 数据处理 ---
    QWidget *dataPage = new QWidget();
    ribbonTab->addTab(dataPage, "数据处理");

    qobject_cast<QVBoxLayout*>(centralWidget()->layout())->insertWidget(0, ribbonTab);
}
