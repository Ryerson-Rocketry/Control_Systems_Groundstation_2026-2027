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
 * Wire format, one CSV line per packet, of scaled integers with no
 * checksum. The rocket sends more than one layout (see LineFormat): the
 * full 27-column row, and a shorter radio packet. A line may start with
 * the transmitting station's callsign, as the radio packet does. The
 * layout is picked by the number of fields, and columns it leaves out
 * decode as NaN.
 *
 * The radio's own CRC catches corruption on air, and every line is still
 * checked for a known field count and valid integers. A header line
 * (starting with "timestamp") is skipped, not counted as an error.
 */

class PacketParser
{
public:
    QList<TelemetryFrame> feed(const QByteArray &bytes);
    quint64 rejectedCount() const { return m_rejected; }

    // The line the rocket would send for this frame; used by the simulator.
    // A column the frame lacks goes out as 0, as it would from the rocket.
    static QByteArray encode(const TelemetryFrame &frame, LineFormat format = LineFormat::FullRow,
                             const QByteArray &callsign = {});

private:
    bool parseLine(const QByteArray &line, TelemetryFrame *frame) const;

    QByteArray m_buffer;
    quint64 m_rejected = 0;
};

#endif // PACKETPARSER_H
