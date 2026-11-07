#include <QApplication>
#include <QCommandLineParser>
#include <QStatusBar>

#include "groundstationbackend.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MetRocketryControlSystems"));
    QCoreApplication::setApplicationName(QStringLiteral("Control Systems Groundstation"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption simOption(QStringLiteral("sim"), QStringLiteral("Use simulated telemetry."));
    const QCommandLineOption i2cOption(QStringLiteral("i2c"),
                                       QStringLiteral("Read the radio board's microcontroller over I2C."));
    const QCommandLineOption serialOption(QStringLiteral("serial"),
                                          QStringLiteral("Read a radio on a serial port."),
                                          QStringLiteral("port"));
    parser.addOption(simOption);
    parser.addOption(i2cOption);
    parser.addOption(serialOption);
    parser.process(app);

    // No options: the RFM9x on the Pi's SPI bus, so autostart needs no arguments.
    BackendConfig config;
    if (parser.isSet(simOption)) {
        config.input = TelemetryInput::Simulated;
    } else if (parser.isSet(i2cOption)) {
        config.input = TelemetryInput::I2c;
    } else if (parser.isSet(serialOption)) {
        config.input = TelemetryInput::Serial;
        config.portName = parser.value(serialOption);
    }

    MainWindow window;
    GroundstationBackend backend(config);

    QObject::connect(&backend, &GroundstationBackend::linkChanged, &window,
                     [&window](bool, const QString &description) {
        window.statusBar()->showMessage(description);
    });
    QObject::connect(&backend, &GroundstationBackend::logFailed, &window,
                     [&window](const QString &message) {
        window.statusBar()->showMessage(message);
    });

    QObject::connect(&backend, &GroundstationBackend::udpFailed, &window,
                     [&window](const QString &message) {
        window.statusBar()->showMessage(message);
    });

    backend.start();
    window.show();
    return app.exec();
}
