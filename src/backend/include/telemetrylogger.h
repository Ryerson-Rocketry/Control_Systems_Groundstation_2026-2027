#ifndef TELEMETRYLOGGER_H
#define TELEMETRYLOGGER_H

#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>

#include "telemetryframe.h"

/*
 * telemetrylogger.h
 * Purpose:
 *   Records every frame, in real units, to telemetry.csv in a new folder
 *   for each run. Each row is flushed as it is written so a power loss
 *   costs almost nothing. exportTo copies the run to a USB drive.
 */

class TelemetryLogger : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryLogger(const QString &rootDir, QObject *parent = nullptr);

    bool startSession();
    void log(const TelemetryFrame &frame);
    QString sessionDir() const { return m_sessionDir; }

    bool exportTo(const QString &volumeRoot, QString *error);

    // Mounted removable drives, for the UI to offer as export targets.
    static QStringList usbVolumes();

signals:
    void writeFailed(const QString &message);

private:
    QString m_rootDir;
    QString m_sessionDir;
    QFile m_file;
    bool m_failing = false;
};

#endif // TELEMETRYLOGGER_H
