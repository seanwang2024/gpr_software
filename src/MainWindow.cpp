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

ImageLabel::ImageLabel(QWidget *parent)
    : QLabel(parent)
    , m_showCrosshair(false)
{
    setAlignment(Qt::AlignCenter);
    setMinimumSize(400, 400);
    setText("No image loaded");
    setStyleSheet("border: 1px solid gray;");
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

void ImageLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    if (m_showCrosshair && !m_image.isNull()) {
        QPainter painter(this);
        QPen pen(Qt::white, 2);
        painter.setPen(pen);

        int x = m_crosshairPos.x();
        int y = m_crosshairPos.y();

        int size = 10;
        painter.drawLine(x - size, y, x + size, y);
        painter.drawLine(x, y - size, x, y + size);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dataOffset(0)
    , m_pixelsPerRow(512)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    imageLabel = new ImageLabel(this);

    scrollArea = new QScrollArea(this);

    scrollArea->setWidget(imageLabel);

    scrollArea->setWidgetResizable(false);        // 禁止自动缩放，让 label 保持原始尺寸
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 禁用垂直滚动条
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // 水平滚动条按需显示

    //mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(scrollArea);
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
             image.setPixel(y, x, qRgb(127 + pixelValue/(256*256*2),127 + pixelValue/(256*256*2),127 + pixelValue/(256*256*2)));

            dataIdx += 4;
        }
    }
    image = image.convertToFormat(QImage::Format_Grayscale8);
    return image;
}
