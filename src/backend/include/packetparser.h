#ifndef PACKETPARSER_H
#define PACKETPARSER_H

#include <QByteArray>
#include <QList>

#include "telemetryframe.h"

/*
 * packetparser.h
 * Purpose:
 *   Turns the raw byte stream into validated TelemetryFrames. Bytes can
 *   arrive in any chunk size, so partial lines are kept until the rest
 *   arrives. Bad lines are counted and dropped.
 *
 * Wire format, one line per packet: the rocket's 27 CSV columns as
 * scaled integers (see telemetryColumns), no checksum. The radio's own
 * CRC catches corruption on air, and every line is still checked for
 * the right column count and valid integers. A header line (starting
 * with "timestamp") is skipped, not counted as an error.
 */

class PacketParser
{
public:
    QList<TelemetryFrame> feed(const QByteArray &bytes);
    quint64 rejectedCount() const { return m_rejected; }

    // The line the rocket would send for this frame; used by the simulator.
    static QByteArray encode(const TelemetryFrame &frame);

private:
    bool parseLine(const QByteArray &line, TelemetryFrame *frame) const;

    QByteArray m_buffer;
    quint64 m_rejected = 0;
};

#endif // PACKETPARSER_H
