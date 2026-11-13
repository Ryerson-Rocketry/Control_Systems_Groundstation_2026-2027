#ifndef HEADERBAR_H
#define HEADERBAR_H

/*
 * headerbar.h
 * Purpose:
 *   The top strip: logo, mission clock and link status on the left, the title
 *   in the middle, packet counts and the SIM/FLIGHT mode box on the right.
 */

#include "telemetry.h"

#include <QWidget>

class QLabel;

class HeaderBar : public QWidget
{
    Q_OBJECT

public:
    explicit HeaderBar(QWidget *parent = nullptr);

    void updateTelemetry(const Telemetry &telemetry);
    // Packets that arrived but could not be read. Shown at once: when every
    // packet is unreadable there is never a good one to update the header.
    void setLost(quint64 lost);
    // One line under the mission clock: link state, or what went wrong
    void setStatus(const QString &text);

private:
    QLabel *m_missionTime;
    QLabel *m_status;
    QLabel *m_received;
    QLabel *m_lost;
    QLabel *m_mode;
};

#endif // HEADERBAR_H
