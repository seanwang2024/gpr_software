#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QScrollArea>
#include <QPoint>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSlider>
#include <QContextMenuEvent>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QTableWidget>
#include <QQuickWidget>

QT_BEGIN_NAMESPACE
class QChart;
QT_END_NAMESPACE

class CustomChartView : public QChartView
{
    Q_OBJECT

public:
    CustomChartView(QWidget *parent = nullptr);
    void setLineSeries(QLineSeries *series);
    void setLineCount(int count);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_lineCount;
    QVector<qreal> m_handleX;
    QVector<float> m_lineY;
    int m_draggingIdx;
    QLineSeries *m_series;
    qreal mapChartToWidgetX(float x);
    float mapWidgetToChartX(qreal widgetX);
    qreal mapChartToWidgetY(float y);
};

class ImageLabel : public QLabel
{
    Q_OBJECT

public:
    ImageLabel(QWidget *parent = nullptr);
    void setImage(const QImage &img);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

signals:
    void imageClicked(const QPoint &pos);
    void gainSelected(float gain);

private:
    QImage m_image;
    QPoint m_crosshairPos;
    bool m_showCrosshair;
    float m_currentGainDb;
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
    void updateChart(int xValue);
    void refreshImage();

    QScrollArea *scrollArea;
    ImageLabel *imageLabel;
    QPushButton *openButton;
    QLabel *coordinateLabel;
    CustomChartView *chartView;
    QLineSeries *chartSeries;
    QTableWidget *gainTable;
    QQuickWidget *quickWidget;
    QByteArray m_rawData;
    qint64 m_dataOffset;
    int m_pixelsPerRow;
    float m_gain;
};

#endif
