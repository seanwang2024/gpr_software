#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QScrollArea>
#include <QPoint>

class ImageLabel : public QLabel
{
    Q_OBJECT

public:
    ImageLabel(QWidget *parent = nullptr);
    void setImage(const QImage &img);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

signals:
    void imageClicked(const QPoint &pos);

private:
    QImage m_image;
    QPoint m_crosshairPos;
    bool m_showCrosshair;
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

    QScrollArea *scrollArea;
    ImageLabel *imageLabel;
    QPushButton *openButton;
    QLabel *coordinateLabel;
    QByteArray m_rawData;
    qint64 m_dataOffset;
    int m_pixelsPerRow;
};

#endif
