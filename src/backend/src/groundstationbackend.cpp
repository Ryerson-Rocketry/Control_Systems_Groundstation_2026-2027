#include "groundstationbackend.h"

#include <QDir>
#include <QStandardPaths>

#include "backendworker.h"

GroundstationBackend::GroundstationBackend(const BackendConfig &config, QObject *parent)
    : QObject(parent), m_config(config)
{
    qRegisterMetaType<TelemetryFrame>();

    connect(&m_receiver, &UdpTelemetryReceiver::frameReceived, &m_hub, &TelemetryHub::addFrame);

    if (m_config.logRoot.isEmpty()) {
        m_config.logRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                               .filePath(QStringLiteral("logs"));
    }
}

GroundstationBackend::~GroundstationBackend()
{
    stop();
}

void GroundstationBackend::start()
{
    if (m_worker)
        return;

    // Telemetry keeps logging even if the display path can't start.
    BackendConfig workerConfig = m_config;
    if (m_receiver.start(m_config.udpPort)) {
        workerConfig.udpPort = m_receiver.port();
    } else {
        emit udpFailed(QStringLiteral("Cannot open telemetry port %1: %2")
                           .arg(m_config.udpPort)
                           .arg(m_receiver.errorString()));
    }

    m_worker = new BackendWorker(workerConfig);
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &BackendWorker::linkChanged, this, &GroundstationBackend::linkChanged);
    connect(m_worker, &BackendWorker::signalQuality, this, &GroundstationBackend::signalQuality);
    connect(m_worker, &BackendWorker::packetRejected, this, &GroundstationBackend::packetRejected);
    connect(m_worker, &BackendWorker::logFailed, this, &GroundstationBackend::logFailed);
    connect(m_worker, &BackendWorker::udpFailed, this, &GroundstationBackend::udpFailed);
    connect(m_worker, &BackendWorker::exportFinished, this, &GroundstationBackend::exportFinished);

    m_thread.start();
    QMetaObject::invokeMethod(m_worker, [worker = m_worker] { worker->start(); }, Qt::QueuedConnection);
}

void GroundstationBackend::stop()
{
    if (!m_worker)
        return;

    QMetaObject::invokeMethod(m_worker, [worker = m_worker] { worker->stop(); },
                              Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
    m_receiver.stop();
    m_worker = nullptr;
}

void GroundstationBackend::exportLogs(const QString &volumeRoot)
{
    if (!m_worker) {
        emit exportFinished(false, QStringLiteral("Backend not running"));
        return;
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, volumeRoot] {
        worker->exportLogs(volumeRoot);
    }, Qt::QueuedConnection);
}
