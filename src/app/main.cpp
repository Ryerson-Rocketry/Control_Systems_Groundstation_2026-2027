#include "mainwindow.h"

#include <QApplication>
using namespace Qt::Literals::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("MetRocketryControlSystems"_L1);
    QCoreApplication::setApplicationName("Control Systems Groundstation"_L1);
    QCoreApplication::setApplicationVersion("1.0"_L1);

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
