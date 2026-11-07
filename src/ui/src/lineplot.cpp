#include "lineplot.h"
#include "sprites.h"
#include "theme.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kWindow = 180;  // s of history on screen

const QColor kSky("#64b0ff");
const QColor kGrass("#80d010");
const QColor kGrassDark("#008800");
const QColor kDirt("#886818");

// Grass strip with dark tufts poking up from it
void drawGrass(QPainter &p, int top, int width)
{
    p.fillRect(0, top, width, 10, kGrass);
    p.setPen(QPen(kGrassDark, 2));
    for (int x = 1; x < width; x += 5) {
        const int tuft = 3 + (x * 7) % 6;
        p.drawLine(x, top + 6, x + 1, top + 6 - tuft);
    }
}

} // namespace

LinePlot::LinePlot(const QString &title, double yMin, double yMax, QWidget *parent)
    : QWidget(parent),
      m_title(title),
      m_yMin(yMin),
      m_yMax(yMax),
      m_defaultYMin(yMin),
      m_defaultYMax(yMax)
{
    setMinimumSize(180, 100);
}

int LinePlot::addSeries(const QString &name, const QColor &color, Qt::PenStyle style)
{
    m_series.append({name, color, style, {}});
    return m_series.size() - 1;
}

void LinePlot::append(int series, double time, double value)
{
    auto &points = m_series[series].points;
    points.append(QPointF(time, value));

    // Drop samples and events that have scrolled off the left edge
    while (!points.isEmpty() && points.first().x() < time - kWindow - 5)
        points.removeFirst();
    while (!m_events.isEmpty() && m_events.first().time < time - kWindow - 5)
        m_events.removeFirst();

    // Grow the range to the next multiple of 10 past the value
    if (value > m_yMax)
        m_yMax = std::ceil(value * 1.1 / 10) * 10;
    if (value < m_yMin)
        m_yMin = std::floor(value * 1.1 / 10) * 10;

    m_latestTime = std::max(m_latestTime, time);
    update();
}

void LinePlot::addEvent(double time, const QString &label)
{
    m_events.append({time, label});
    update();
}

void LinePlot::setFlying(bool flying)
{
    if (flying == m_flying)
        return;
    m_flying = flying;
    update();
}

void LinePlot::clear()
{
    for (Series &series : m_series)
        series.points.clear();
    m_events.clear();
    m_yMin = m_defaultYMin;
    m_yMax = m_defaultYMax;
    m_latestTime = 0;
    update();
}

void LinePlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), kSky);

    const QRect plot(42, 26, width() - 42 - 14, height() - 26 - 22);
    const double tStart = std::max(0.0, m_latestTime - kWindow);
    auto toScreen = [&](const QPointF &point) {
        return QPointF(plot.left() + (point.x() - tStart) / kWindow * plot.width(),
                       plot.bottom() - (point.y() - m_yMin) / (m_yMax - m_yMin) * plot.height());
    };

    // Grid and y labels at the bottom, middle and top
    p.setFont(Theme::pixelFont(10));
    for (int i = 0; i <= 2; ++i) {
        const double value = m_yMin + (m_yMax - m_yMin) * i / 2;
        const int y = static_cast<int>(plot.bottom() - plot.height() * i / 2.0);
        p.setPen(QPen(QColor(255, 255, 255, 110), 1, Qt::DotLine));
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(Theme::Ink);
        p.drawText(QRect(0, y - 7, plot.left() - 6, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(value, 'g', 4));
    }

    // Lines
    p.save();
    p.setClipRect(plot.adjusted(1, 0, 0, -1));
    p.setRenderHint(QPainter::Antialiasing);
    for (const Series &series : m_series) {
        if (series.points.size() < 2)
            continue;
        QPolygonF line;
        for (const QPointF &point : series.points)
            line << toScreen(point);
        p.setPen(QPen(series.color, 2.5, series.style));
        p.drawPolyline(line);
    }
    p.restore();

    // Phase changes: a dog pops out of the grass with the phase on a sign
    // above it; the grass is drawn afterwards so it hides the dog's legs.
    // Phases close together (ascent, apogee, descent are seconds apart)
    // would pile up, so signs drop to the next row down when they'd
    // overlap (or slide sideways once the rows run out), and a dog is
    // skipped if one's already standing there.
    const int grassTop = plot.bottom() - 6;
    const int signHeight = 13;
    const int maxRows = std::max(1, (plot.height() - 40) / (signHeight + 1));
    QVector<int> rowRight;  // right edge of the last sign in each row
    int lastDogRight = -1000;
    p.setFont(Theme::pixelFont(9));
    for (const Event &event : m_events) {
        const int x = static_cast<int>(toScreen(QPointF(event.time, 0)).x());
        if (x < plot.left() || x > plot.right())
            continue;

        QRect sign(0, 0, p.fontMetrics().horizontalAdvance(event.label) + 8, signHeight);
        sign.moveCenter(QPoint(x, 0));
        sign.moveLeft(std::clamp(sign.left(), plot.left() + 1, plot.right() - sign.width()));
        // First row where it fits; if every row is taken, slide it right
        // along whichever row frees up soonest
        int row = 0;
        while (row < rowRight.size() && sign.left() <= rowRight[row] + 2)
            ++row;
        if (row >= maxRows) {
            row = static_cast<int>(std::min_element(rowRight.begin(), rowRight.end()) - rowRight.begin());
            sign.moveLeft(std::min(rowRight[row] + 3, plot.right() - sign.width()));
        } else if (row == rowRight.size()) {
            rowRight.append(0);
        }
        rowRight[row] = sign.right();
        sign.moveTop(plot.top() + 1 + row * (signHeight + 1));

        const QRect dog(x - 12, grassTop - 16, 24, 20);
        p.setPen(QPen(Qt::white, 1, Qt::DashLine));
        p.drawLine(x, sign.bottom() + 1, x, dog.top());

        p.setPen(QPen(Theme::Ink, 1));
        p.setBrush(Qt::white);
        p.drawRect(sign);
        p.drawText(sign, Qt::AlignCenter, event.label);

        if (dog.left() > lastDogRight) {
            Sprites::draw(p, dog, Sprites::Dog);
            lastDogRight = dog.right();
        }
    }

    // Ground: grass, then dirt holding the x labels
    drawGrass(p, grassTop, width());
    p.fillRect(QRect(0, plot.bottom() + 3, width(), height() - plot.bottom() - 3), kDirt);

    p.setFont(Theme::pixelFont(10));
    p.setPen(Qt::white);
    for (int i = 0; i <= 2; ++i) {
        const double time = tStart + kWindow * i / 2;
        const int x = static_cast<int>(plot.left() + plot.width() * i / 2.0);
        QString text = QString::number(time, 'f', 0);
        if (i == 2)
            text += QStringLiteral(" s");
        const Qt::Alignment align = i == 0 ? Qt::AlignLeft : (i == 1 ? Qt::AlignHCenter : Qt::AlignRight);
        QRect box(0, plot.bottom() + 5, 80, 14);
        if (i == 0)
            box.moveLeft(x);
        else if (i == 1)
            box.moveCenter(QPoint(x, box.center().y()));
        else
            box.moveRight(x);
        p.drawText(box, align | Qt::AlignVCenter, text);
    }

    // Birds fly at the head of each line while the rocket is up
    if (m_flying) {
        for (const Series &series : m_series) {
            if (series.points.isEmpty())
                continue;
            // Kept inside the plot so it never pokes into the title or frame
            const QPointF head = toScreen(series.points.last());
            QRect bird(static_cast<int>(head.x()) - 16, static_cast<int>(head.y()) - 9, 32, 18);
            bird.moveLeft(std::clamp(bird.left(), plot.left() + 1, plot.right() - bird.width()));
            bird.moveTop(std::clamp(bird.top(), plot.top() + 1, plot.bottom() - bird.height()));
            Sprites::draw(p, bird, Sprites::Bird);
        }
    }

    // Title with a hard shadow, plus a legend when there's more than one line
    p.setFont(Theme::pixelFont(12));
    p.setPen(Theme::Ink);
    p.drawText(QRect(1, 5, width(), 16), Qt::AlignHCenter | Qt::AlignVCenter, m_title);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 4, width(), 16), Qt::AlignHCenter | Qt::AlignVCenter, m_title);
    if (m_series.size() > 1) {
        p.setFont(Theme::pixelFont(10));
        int x = width() - 10;
        for (int i = m_series.size() - 1; i >= 0; --i) {
            const Series &series = m_series[i];
            const int textWidth = p.fontMetrics().horizontalAdvance(series.name);
            x -= textWidth;
            p.setPen(series.color);
            p.drawText(QRect(x, 6, textWidth, 14), Qt::AlignVCenter, series.name);
            x -= 18;
            p.setPen(QPen(series.color, 2, series.style));
            p.drawLine(x, 13, x + 14, 13);
            x -= 6;
        }
    }

    // Frame
    p.setPen(QPen(Theme::Ink, 3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(1, 1, -2, -2));
}
