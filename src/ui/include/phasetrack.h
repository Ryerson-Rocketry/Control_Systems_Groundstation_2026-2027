#ifndef PHASETRACK_H
#define PHASETRACK_H

/*
 * phasetrack.h
 * Purpose:
 *   The rail under the header showing the flight states. Each state has
 *   a ghost above its label: passed states are green with a dead ghost,
 *   the current one is yellow with Pac-Man just to the left of its ghost,
 *   and upcoming ghosts are faded. Once the flight is over, Pac-Man stands
 *   to the right of the last ghost.
 */

#include "telemetry.h"

#include <QElapsedTimer>
#include <QWidget>

class PhaseTrack : public QWidget
{
    Q_OBJECT

public:
    explicit PhaseTrack(QWidget *parent = nullptr);

    // finished: the rocket has landed, so the last state is passed too
    void setState(qint32 state, bool finished);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qint32 m_state = StateIdle;
    bool m_finished = false;
    QElapsedTimer m_clock;  // drives Pac-Man's chomp
};

#endif // PHASETRACK_H
