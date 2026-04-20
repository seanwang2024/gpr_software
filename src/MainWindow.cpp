#include "MainWindow.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_label = new QLabel("Hello, Qt!", centralWidget);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("font-size: 24px; font-weight: bold;");

    m_button = new QPushButton("Click Me", centralWidget);

    layout->addWidget(m_label);
    layout->addWidget(m_button);

    setCentralWidget(centralWidget);
    resize(400, 300);

    connect(m_button, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
}

MainWindow::~MainWindow() {}

void MainWindow::onButtonClicked() {
    static int clickCount = 0;
    clickCount++;
    m_label->setText(QString("Button clicked %1 times!").arg(clickCount));
}
