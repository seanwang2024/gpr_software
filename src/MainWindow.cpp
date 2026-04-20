#include "MainWindow.h"
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_label = new QLabel("请点击按钮选择图片", centralWidget);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setMinimumSize(400, 300);
    m_label->setStyleSheet("border: 2px dashed gray; background-color: #f0f0f0;");

    m_button = new QPushButton("选择图片", centralWidget);

    layout->addWidget(m_label);
    layout->addWidget(m_button);

    setCentralWidget(centralWidget);
    resize(500, 400);

    connect(m_button, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
}

MainWindow::~MainWindow() {}

void MainWindow::onButtonClicked() {
    QString fileName = QFileDialog::getOpenFileName(this,
                                                   "选择图片文件",
                                                   "",
                                                   "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");

    if (!fileName.isEmpty()) {
        QPixmap pixmap(fileName);
        if (!pixmap.isNull()) {
            m_label->setPixmap(pixmap.scaled(m_label->size(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
            m_label->setStyleSheet("");
        } else {
            QMessageBox::warning(this, "错误", "无法加载图片文件");
        }
    }
}