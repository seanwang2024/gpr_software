#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QScrollArea>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();

private:
    QImage loadDZTFile(const QString &filePath);

    QScrollArea *scrollArea;
    QLabel *imageLabel;
    QPushButton *openButton;
};

#endif
