#include "mainwindow.h"

/*
 * mainwindow.cpp
 * Purpose:
 *   Implements the MainWindow class, the top-level window of the
 *   Groundstation app. Everything the user sees (panels, telemetry
 *   readouts, plots, logs, controls) lives inside or is created by
 *   this window.
 *
 * What it does:
 *   - Builds the UI in the constructor: window title, size, central
 *     widget, dock panels, and status bar.
 *   - Owns the child widgets, so Qt cleans them up automatically when
 *     the window closes.
 *   - Will connect signals and slots, such as incoming telemetry
 *     updating the labels and charts, and button clicks sending
 *     commands.
 *
 */

 
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    setWindowTitle("Groundstation");
    resize(800, 800);
}

MainWindow::~MainWindow()
{
    delete ui;
}
