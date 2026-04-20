#include "MainWindow.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QDataStream>

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

    openButton = new QPushButton("Open DZT File", this);

    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(openButton);

    setCentralWidget(centralWidget);
    setWindowTitle("DZT Image Viewer");

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

    QImage image = loadDZTFile(fileName);
    if (image.isNull()) {
        QMessageBox::warning(this, "Error", "Failed to load DZT file.");
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    imageLabel->setPixmap(pixmap.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QImage MainWindow::loadDZTFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    const qint64 dataOffset = 0x200;
    const int bytesPerPixel = 4;
    const int pixelsPerRow = 512;

    if (!file.seek(dataOffset)) {
        return QImage();
    }

    QByteArray data = file.readAll();
    int dataSize = data.size();
    int totalPixels = dataSize / bytesPerPixel;
    int rows = totalPixels / pixelsPerRow;

    if (rows <= 0 || totalPixels % pixelsPerRow != 0) {
        return QImage();
    }

    QImage image(pixelsPerRow, rows, QImage::Format_Grayscale8);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < pixelsPerRow; ++x) {
            qint32 pixelValue;
            in >> pixelValue;
            quint8 grayValue = static_cast<quint8>(qBound(0, pixelValue, 255));
            image.setPixel(x, y, grayValue);
        }
    }

    return image;
}
