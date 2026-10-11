#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    setWindowTitle("Groundstation");
    resize(800, 800);
}

MainWindow::~MainWindow()
{
    delete ui;
}
