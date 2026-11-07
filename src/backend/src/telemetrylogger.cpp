#include "telemetrylogger.h"

#include <cmath>

#include <QDateTime>
#include <QDir>
#include <QStorageInfo>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
const QString kLogFileName = QStringLiteral("telemetry.csv");

QByteArray csvHeader()
{
    QByteArray header("received_utc");
    const QList<TelemetryColumn> columns = telemetryColumns();
    for (const TelemetryColumn &column : columns)
        header += ',' + column.name + " (" + column.unit + ')';
    return header + '\n';
}

// A scale of 100 means two decimal places carry all the precision sent.
int decimalsFor(int scale)
{
    return static_cast<int>(std::lround(std::log10(scale)));
}
}

TelemetryLogger::TelemetryLogger(const QString &rootDir, QObject *parent)
    : QObject(parent), m_rootDir(rootDir)
{
}

bool TelemetryLogger::startSession()
{
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QDir root(m_rootDir);
    if (!root.mkpath(QStringLiteral("."))) {
        emit writeFailed(QStringLiteral("Cannot create log folder %1").arg(m_rootDir));
        return false;
    }

    QString name = stamp;
    for (int suffix = 1; root.exists(name); ++suffix)
        name = QStringLiteral("%1-%2").arg(stamp).arg(suffix);

    if (!root.mkdir(name)) {
        emit writeFailed(QStringLiteral("Cannot create log folder in %1").arg(m_rootDir));
        return false;
    }
    m_sessionDir = root.filePath(name);

    m_file.setFileName(QDir(m_sessionDir).filePath(kLogFileName));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit writeFailed(QStringLiteral("Cannot open %1: %2").arg(m_file.fileName(), m_file.errorString()));
        return false;
    }
    m_file.write(csvHeader());
    m_file.flush();
    return true;
}

void TelemetryLogger::log(const TelemetryFrame &frame)
{
    if (!m_file.isOpen())
        return;

    const QList<TelemetryColumn> columns = telemetryColumns();
    const QList<double> values = telemetryValues(frame);

    QByteArray row = frame.receivedAtUtc.toString(Qt::ISODateWithMs).toUtf8();
    for (qsizetype index = 0; index < values.size(); ++index)
        row += ',' + QByteArray::number(values[index], 'f', decimalsFor(columns[index].scale));
    row += '\n';

    const bool ok = m_file.write(row) == row.size() && m_file.flush();
    if (!ok && !m_failing)
        emit writeFailed(QStringLiteral("Telemetry log write failed: %1").arg(m_file.errorString()));
    m_failing = !ok;
}

bool TelemetryLogger::exportTo(const QString &volumeRoot, QString *error)
{
    if (m_sessionDir.isEmpty()) {
        *error = QStringLiteral("No log session to export");
        return false;
    }

    const QDir session(m_sessionDir);
    const QString relative = QStringLiteral("GroundstationLogs/") + session.dirName();
    QDir volume(volumeRoot);
    if (!volume.mkpath(relative)) {
        *error = QStringLiteral("Cannot write to %1").arg(volumeRoot);
        return false;
    }

    const QDir target(volume.filePath(relative));
    const QStringList files = session.entryList(QDir::Files);
    for (const QString &name : files) {
        const QString destination = target.filePath(name);
        QFile::remove(destination); // repeat exports replace the older copy
        if (!QFile::copy(session.filePath(name), destination)) {
            *error = QStringLiteral("Copy failed: %1").arg(name);
            return false;
        }
    }

#ifdef Q_OS_UNIX
    // The operator will pull the drive as soon as this reports success.
    ::sync();
#endif
    return true;
}

QStringList TelemetryLogger::usbVolumes()
{
    QStringList volumes;
    const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &info : mounted) {
        const QString path = info.rootPath();
        const bool removable = path.startsWith(QStringLiteral("/media/"))
            || path.startsWith(QStringLiteral("/run/media/"));
        if (info.isValid() && info.isReady() && !info.isReadOnly() && removable)
            volumes.append(path);
    }
    return volumes;
}
