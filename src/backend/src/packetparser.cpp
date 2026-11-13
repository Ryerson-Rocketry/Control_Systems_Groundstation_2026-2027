#include "packetparser.h"

#include <cmath>
#include <limits>

namespace {
constexpr int kMaxLineBytes = 512;
constexpr qint64 kMinWireValue = std::numeric_limits<qint32>::min();
constexpr qint64 kMaxWireValue = std::numeric_limits<quint32>::max();
constexpr qsizetype kMinCallsignLength = 3;
constexpr qsizetype kMaxCallsignLength = 10;

// An amateur callsign such as "VE3HIV" or "VA3ABC/P": letters and digits
// with at least one letter, so it can't be mistaken for a number.
bool isCallsign(const QByteArray &field)
{
    if (field.size() < kMinCallsignLength || field.size() > kMaxCallsignLength)
        return false;

    bool hasLetter = false;
    for (const char c : field) {
        const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!letter && !(c >= '0' && c <= '9') && c != '/')
            return false;
        hasLetter = hasLetter || letter;
    }
    return hasLetter;
}
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

QByteArray PacketParser::encode(const TelemetryFrame &frame, LineFormat format, const QByteArray &callsign)
{
    const QList<TelemetryColumn> columns = telemetryColumns();
    const QList<double> values = telemetryValues(frame);

    QByteArray line = callsign;
    for (const int column : lineColumns(format)) {
        if (!line.isEmpty())
            line += ',';
        const double value = hasValue(values[column]) ? values[column] : 0.0;
        line += QByteArray::number(std::llround(value * columns[column].scale));
    }
    return line + '\n';
}

bool PacketParser::parseLine(const QByteArray &line, TelemetryFrame *frame) const
{
    QList<QByteArray> fields = line.split(',');
    if (isCallsign(fields.first().trimmed()))
        fields.removeFirst();

    QList<int> layout;
    for (const LineFormat format : lineFormats()) {
        const QList<int> candidate = lineColumns(format);
        if (candidate.size() == fields.size())
            layout = candidate;
    }
    if (layout.isEmpty())
        return false;

    const QList<TelemetryColumn> columns = telemetryColumns();
    QList<double> values(columns.size(), std::numeric_limits<double>::quiet_NaN());
    for (qsizetype index = 0; index < fields.size(); ++index) {
        bool ok = false;
        const qint64 raw = fields[index].trimmed().toLongLong(&ok);
        if (!ok || raw < kMinWireValue || raw > kMaxWireValue)
            return false;
        const int column = layout[index];
        values[column] = static_cast<double>(raw) / columns[column].scale;
    }
    return setTelemetryValues(values, frame);
}
