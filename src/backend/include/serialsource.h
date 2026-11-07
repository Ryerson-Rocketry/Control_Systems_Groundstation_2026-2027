#ifndef SERIALSOURCE_H
#define SERIALSOURCE_H

#include <QSerialPort>
#include <QTimer>

#include "telemetrysource.h"

/*
 * serialsource.h
 * Purpose:
 *   Reads the radio over a serial port. There is no operator to
 *   reconnect anything, so it keeps retrying until the port opens and
 *   recovers by itself if the radio is unplugged.
 *
 * An empty port name means "use the first serial port found".
 */

class SerialSource : public TelemetrySource
{
    Q_OBJECT

public:
    SerialSource(const QString &portName, qint32 baudRate, QObject *parent = nullptr);

    void start() override;
    void stop() override;

private:
    void tryOpen();
    void reportLink(bool up, const QString &description);

    QSerialPort m_port;
    QTimer m_retryTimer;
    QString m_portName;
    qint32 m_baudRate;
    QString m_lastDescription;
};

#endif // SERIALSOURCE_H
