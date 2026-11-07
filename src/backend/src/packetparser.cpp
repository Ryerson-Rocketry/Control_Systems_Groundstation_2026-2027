#include "packetparser.h"

#include <cmath>
#include <limits>

namespace {
constexpr int kMaxLineBytes = 512;
constexpr qint64 kMinWireValue = std::numeric_limits<qint32>::min();
constexpr qint64 kMaxWireValue = std::numeric_limits<quint32>::max();
}

QList<TelemetryFrame> PacketParser::feed(const QByteArray &bytes)
{
    QList<TelemetryFrame> frames;
    m_buffer.append(bytes);

    qsizetype start = 0;
    qsizetype newline = m_buffer.indexOf('\n', start);
    while (newline >= 0) {
        const QByteArray line = m_buffer.mid(start, newline - start).trimmed();
        start = newline + 1;
        newline = m_buffer.indexOf('\n', start);

        if (line.isEmpty() || line.startsWith("timestamp"))
            continue;

        TelemetryFrame frame;
        if (parseLine(line, &frame)) {
            frame.receivedAtUtc = QDateTime::currentDateTimeUtc();
            frames.append(frame);
        } else {
            ++m_rejected;
        }
    }
    m_buffer.remove(0, start);

    // Line noise with no newline must not grow the buffer forever.
    if (m_buffer.size() > kMaxLineBytes) {
        m_buffer.clear();
        ++m_rejected;
    }
    return frames;
}

QByteArray PacketParser::encode(const TelemetryFrame &frame)
{
    const QList<TelemetryColumn> columns = telemetryColumns();
    const QList<double> values = telemetryValues(frame);

    QByteArray line;
    for (qsizetype index = 0; index < values.size(); ++index) {
        if (index > 0)
            line += ',';
        line += QByteArray::number(std::llround(values[index] * columns[index].scale));
    }
    return line + '\n';
}

bool PacketParser::parseLine(const QByteArray &line, TelemetryFrame *frame) const
{
    const QList<QByteArray> fields = line.split(',');
    const QList<TelemetryColumn> columns = telemetryColumns();
    if (fields.size() != columns.size())
        return false;

    QList<double> values;
    values.reserve(fields.size());
    for (qsizetype index = 0; index < fields.size(); ++index) {
        bool ok = false;
        const qint64 raw = fields[index].trimmed().toLongLong(&ok);
        if (!ok || raw < kMinWireValue || raw > kMaxWireValue)
            return false;
        values.append(static_cast<double>(raw) / columns[index].scale);
    }
    return setTelemetryValues(values, frame);
}
