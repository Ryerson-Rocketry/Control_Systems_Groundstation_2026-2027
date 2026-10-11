#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MetRocketryControlSystems"));
    QCoreApplication::setApplicationName(QStringLiteral("Control Systems Groundstation"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    MainWindow window;
    window.show();
    return app.exec();
}