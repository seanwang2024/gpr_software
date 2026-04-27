#include "MainWindow.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QScrollArea>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    imageLabel = new QLabel(this);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(400, 400);
    imageLabel->setText("No image loaded");
    imageLabel->setStyleSheet("border: 1px solid gray;");

    scrollArea = new QScrollArea(this);

    scrollArea->setWidget(imageLabel);

    scrollArea->setWidgetResizable(false);        // 禁止自动缩放，让 label 保持原始尺寸
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 禁用垂直滚动条
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // 水平滚动条按需显示

    //mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(scrollArea);
    openButton = new QPushButton("Open DZT File", this);


    mainLayout->addWidget(openButton);


    setCentralWidget(centralWidget);
    setWindowTitle("DZT Image Viewer");

    //setCentralWidget(scrollArea);

    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
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

    //QImage image(1024,512,  QImage::Format_Grayscale8);    //// first 1024 colunm for test and x yx reverse
    //image.setPixel(100, 100, qRgb(255, 255, 255));
    //image.fill(255); //

    QImage image = loadDZTFile(fileName);
    
     if (image.isNull()) {
         QMessageBox::warning(this, "Error", "Failed to load DZT file.");
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);



    imageLabel->setPixmap(pixmap);
    // 关键：将 QLabel 的大小设置为图片的原始尺寸
    imageLabel->resize(pixmap.size());


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

    if (!file.seek(dataOffset)) {
        QMessageBox::warning(this, "Error", "open file failed.");
        return QImage();
    }

    QByteArray data = file.readAll();
    int dataSize = data.size();
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
                (static_cast<quint8>(data[dataIdx + 3]) << 24) |
                (static_cast<quint8>(data[dataIdx + 2]) << 16) |
                (static_cast<quint8>(data[dataIdx + 1]) << 8) |
                (static_cast<quint8>(data[dataIdx]))
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
