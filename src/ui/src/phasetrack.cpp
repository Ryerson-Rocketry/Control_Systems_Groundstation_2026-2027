#include "phasetrack.h"
#include "sprites.h"
#include "theme.h"

#include <QPainter>

namespace {

QString ghostFor(FlightState state)
{
    switch (state) {
    case StateIdle:             return Sprites::GhostRed;
    case StateBoosterBurn:      return Sprites::GhostPink;
    case StateCanardActivation: return Sprites::GhostCyan;
    case StateApogee:           return Sprites::GhostOrange;
    case StateDescent:          return Sprites::GhostBlue;
    }
    return {};
}

} // namespace

PhaseTrack::PhaseTrack(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(54);
}

void PhaseTrack::setState(qint32 state, bool finished)
{
    if (state == m_state && finished == m_finished)
        return;
    m_state = state;
    m_finished = finished;
    update();
}

void PhaseTrack::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setFont(Theme::pixelFont(12));

    const int margin = 70;
    const int railY = height() - 13;
    const qreal step = (width() - 2 * margin) / qreal(kStateCount - 1);
    // Once the flight is over the last state counts as passed too
    const int current = qBound(0, m_state, kStateCount - 1) + (m_finished ? 1 : 0);

    // Rail: thick ink line with a wood-coloured core
    p.setPen(QPen(Theme::Ink, 7));
    p.drawLine(10, railY, width() - 10, railY);
    p.setPen(QPen(QColor("#d9822b"), 3));
    p.drawLine(10, railY, width() - 10, railY);

    for (int i = 0; i < kStateCount; ++i) {
        const int cx = static_cast<int>(margin + i * step);
        const QString label = flightStateName(i);

        QColor fill = Qt::white;
        QColor text = Theme::Grey;
        if (i < current) {
            fill = Theme::Lime;
            text = Theme::Ink;
        } else if (i == current) {
            fill = Theme::Yellow;
            text = Theme::Ink;
        }

        QRect box(0, 0, p.fontMetrics().horizontalAdvance(label) + 16, 22);
        box.moveCenter(QPoint(cx, railY));

        // Ghost above the label: dead once passed, faded until reached.
        // Pac-Man waits just left of the current ghost, which also puts
        // him to the right of the states he's passed. After the last
        // state he stands to the right of its dead ghost.
        // 28 px = exactly 2x the 14 px art, so the pixels stay even.
        const int spriteSize = 28;
        const QRect spot(cx - spriteSize / 2, box.top() - spriteSize, spriteSize, spriteSize);
        p.save();
        if (i > current)
            p.setOpacity(0.4);
        Sprites::draw(p, spot, i < current ? Sprites::GhostDead : ghostFor(static_cast<FlightState>(i)));
        p.restore();
        if (i == current)
            Sprites::draw(p, spot.translated(-spriteSize - 2, 0), Sprites::Runner);
        else if (m_finished && i == kStateCount - 1)
            Sprites::draw(p, spot.translated(spriteSize + 2, 0), Sprites::Runner);

        p.setPen(QPen(Theme::Ink, 3));
        p.setBrush(fill);
        p.drawRect(box);
        p.setPen(text);
        p.drawText(box, Qt::AlignCenter, label);
    }
}
