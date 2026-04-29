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
        setPixmap(QPixmap::fromImage(img));
        resize(img.size());
    } else {
        clear();
        setText("No image loaded");
    }
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
        emit imageClicked(m_crosshairPos);
        update();
    }
    QLabel::mousePressEvent(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_showCrosshair && !m_image.isNull() && (event->buttons() & Qt::LeftButton)) {
        m_crosshairPos = event->pos();
        emit imageClicked(m_crosshairPos);
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
    QLabel::paintEvent(event);

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

    menu.addAction("2 变换");

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
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dataOffset(0)
    , m_pixelsPerRow(512)
    , m_gain(4.0f)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 创建水平布局，左边放图表，右边放图片
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // 创建滚动区域
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(false);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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
    axisX->setTitleText("Value");
    axisX->setRange(-100, 100);
    axisX->setLabelFormat("%d");
    axisX->setTickCount(5);

    // Y轴：Y坐标（反转，0在顶部，511在底部，与图片对齐）
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Y");
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

    // 左侧垂直布局：chart（上半） + 表格（下半，同高度）
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftLayout->addWidget(chartView, 1);
    leftLayout->addWidget(gainTable, 1);

    contentLayout->addLayout(leftLayout);
    contentLayout->addWidget(scrollArea);

    // QQuickWidget 显示浮动球体
    quickWidget = new QQuickWidget(this);
    quickWidget->setSource(QUrl::fromLocalFile("D:/gpr_software/qml/Sphere.qml"));
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setFixedWidth(200);

    contentLayout->addWidget(quickWidget);

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

    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(imageLabel, &ImageLabel::imageClicked, this, &MainWindow::onImageClicked);
    connect(imageLabel, &ImageLabel::gainSelected, this, [this](float gainDb) {
        m_gain = pow(10.0f, gainDb / 20.0f);
        refreshImage();
    });
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

    chartView->chart()->setTitle(QString("X = %1").arg(xValue));
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

    const int bytesPerPixel = 4;
    const int pixelsPerRow = m_pixelsPerRow;
    int totalPixels = m_rawData.size() / bytesPerPixel;
    int rows = totalPixels / pixelsPerRow;
    int dataSize = m_rawData.size();

    QImage image(rows, pixelsPerRow, QImage::Format_Grayscale8);
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

            image.setPixel(y, x, qRgb(127 + pixelValue_display / (256 * 256),
                                       127 + pixelValue_display / (256 * 256),
                                       127 + pixelValue_display / (256 * 256)));
            dataIdx += 4;
        }
    }
    image = image.convertToFormat(QImage::Format_Grayscale8);
    imageLabel->setImage(image);
}
