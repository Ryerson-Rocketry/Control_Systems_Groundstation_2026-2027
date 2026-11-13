#include "altitudescene.h"
#include "sprites.h"
#include "theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kAxisWidth = 44;
constexpr int kGroundHeight = 22;
constexpr int kStatusHeight = 20;          // status bar across the top
constexpr double kMinAltitudeScale = 800;  // m at the top of the scene
constexpr double kMinTimeScale = 180;      // s across the scene
constexpr int kFrameMs = 33;               // ~30 fps
constexpr double kScrollPerMps = 2.5;      // px/s of sky scroll per m/s of vertical speed
constexpr double kMaxScroll = 320;         // px/s, so the ascent doesn't blur

const QColor kSkyTop("#3d9de8");
const QColor kSkyBottom("#9adcff");
const QColor kGrass("#7ed957");
const QColor kGrassDark("#3f9a3a");

// One star in the scrolling sky. Positions are fractions of the sky so
// the field fills any size; depth < 1 scrolls slower (further away).
struct Star
{
    double x;
    double y;
    double depth;
    int size;        // px; 4+ draws a five-point star, smaller a dot
    QColor color;
    double twinkle;  // phase offset
};

const QVector<Star> &stars()
{
    static const QVector<Star> field = [] {
        QRandomGenerator rng(2027);  // fixed seed: same sky every run
        const QColor colors[] = {Qt::white, Qt::white, Qt::white, QColor("#fff27a"), QColor("#ffb3e6"),
                                 QColor("#b8ffb0")};
        QVector<Star> result;
        for (int i = 0; i < 70; ++i) {
            const bool big = i % 7 == 0;
            result.append({rng.generateDouble(), rng.generateDouble(), 0.4 + rng.generateDouble() * 0.6,
                           big ? 5 + rng.bounded(3) : 1 + rng.bounded(2), colors[rng.bounded(6)],
                           rng.generateDouble() * 6.28});
        }
        return result;
    }();
    return field;
}

void drawStarShape(QPainter &p, const QPointF &centre, double radius, const QColor &color)
{
    QPolygonF star;
    for (int i = 0; i < 10; ++i) {
        const double angle = -M_PI / 2 + i * M_PI / 5;
        const double r = i % 2 ? radius * 0.45 : radius;
        star << centre + QPointF(std::cos(angle) * r, std::sin(angle) * r);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(star);
}

// White wind streak with a loop in it, like the Kirby sky
void drawSwirl(QPainter &p, const QPointF &at, double size)
{
    QPainterPath path(at);
    path.cubicTo(at + QPointF(size * 0.6, -size * 0.1), at + QPointF(size * 1.2, -size * 0.5),
                 at + QPointF(size * 1.0, -size * 0.7));
    path.cubicTo(at + QPointF(size * 0.8, -size * 0.9), at + QPointF(size * 0.6, -size * 0.4),
                 at + QPointF(size * 1.1, -size * 0.3));
    path.cubicTo(at + QPointF(size * 1.5, -size * 0.2), at + QPointF(size * 2.0, -size * 0.3),
                 at + QPointF(size * 2.4, -size * 0.2));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 90), 7, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(path);
    p.setPen(QPen(QColor(255, 255, 255, 230), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(path);
}

// Dream Land ground: rounded green hills behind a scalloped grass strip
void drawGround(QPainter &p, const QRect &sky, const QRect &ground)
{
    p.setPen(QPen(kGrassDark, 2));
    p.setBrush(kGrass.lighter(108));
    p.drawEllipse(QRectF(sky.left() + sky.width() * 0.55, sky.bottom() - 34, sky.width() * 0.32, 70));
    p.drawEllipse(QRectF(sky.left() + sky.width() * 0.76, sky.bottom() - 22, sky.width() * 0.3, 50));

    p.fillRect(ground, kGrass);
    p.setPen(Qt::NoPen);
    p.setBrush(kGrass);
    for (int x = ground.left() - 6; x < ground.right(); x += 14)
        p.drawEllipse(QPoint(x, ground.top()), 8, 6);
    p.setPen(QPen(kGrassDark, 2));
    for (int x = ground.left() + 4; x < ground.right(); x += 18)
        p.drawLine(x, ground.top() + 8, x + 4, ground.bottom() - 4);
}

} // namespace

AltitudeScene::AltitudeScene(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(240, 180);

    m_animation = new QTimer(this);
    m_animation->setInterval(kFrameMs);
    connect(m_animation, &QTimer::timeout, this, &AltitudeScene::animate);
    m_clock.start();
    m_animation->start();
}

void AltitudeScene::animate()
{
    const qint64 now = m_clock.elapsed();
    const double dt = (now - m_lastFrameMs) / 1000.0;
    m_lastFrameMs = now;

    // Rising: the sky slides down past Kirby. Falling: it slides up.
    const double speed = std::clamp(m_speed * kScrollPerMps, -kMaxScroll, kMaxScroll);
    m_scroll += speed * dt;
    update();
}

void AltitudeScene::addSample(const Telemetry &telemetry)
{
    // The rocket can sit on the pad for ages before launch, so the trail
    // starts from liftoff rather than from power-on. Checks onGround too:
    // the flight computer is still in Pad during the booster's burn.
    if (telemetry.flightState == StateIdle && telemetry.onGround()) {
        m_trail.clear();
        m_launchTime = telemetry.missionTime;
    }

    // The radio packet may not carry altitude: then there is nothing to
    // plot, and the sky stays still rather than scroll at a made-up speed.
    m_hasAltitude = telemetry.hasAltitude();
    if (m_hasAltitude) {
        m_trail.append(QPointF(telemetry.missionTime - m_launchTime, telemetry.altitude));
        m_maxAltitude = std::max(m_maxAltitude, telemetry.altitude);
    }
    m_speed = m_hasAltitude ? telemetry.velocity : 0.0;
    m_state = telemetry.flightState;
    m_onGround = telemetry.onGround();
}

void AltitudeScene::setLastEvent(const QString &text)
{
    m_lastEvent = text;
}

void AltitudeScene::clear()
{
    m_trail.clear();
    m_maxAltitude = 0;
    m_speed = 0;
    m_state = StateIdle;
    m_onGround = true;
    m_hasAltitude = true;
    m_lastEvent.clear();
}

void AltitudeScene::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const qint64 now = m_clock.elapsed();

    const QRect status(0, 0, width(), kStatusHeight);
    const QRect axis(0, kStatusHeight, kAxisWidth, height() - kGroundHeight - kStatusHeight);
    const QRect sky(kAxisWidth, kStatusHeight, width() - kAxisWidth, height() - kGroundHeight - kStatusHeight);
    const QRect ground(0, height() - kGroundHeight, width(), kGroundHeight);

    // Scales grow if the flight goes higher or longer than the defaults
    const double altitudeScale = std::max(kMinAltitudeScale, std::ceil(m_maxAltitude * 1.15 / 200) * 200);
    const double timeScale = std::max(kMinTimeScale, m_trail.isEmpty() ? 0.0 : m_trail.last().x());
    auto yFor = [&](double altitude) {
        return sky.bottom() - altitude / altitudeScale * (sky.height() - 12);
    };
    auto xFor = [&](double time) {
        return sky.left() + 30 + time / timeScale * (sky.width() - 70);
    };

    // Sky
    QLinearGradient gradient(sky.topLeft(), sky.bottomLeft());
    gradient.setColorAt(0, kSkyTop);
    gradient.setColorAt(1, kSkyBottom);
    p.fillRect(sky, gradient);
    p.fillRect(axis, Theme::SkyAxis);

    // Scrolling layers: stars, then wind swirls a little faster (closer)
    p.save();
    p.setClipRect(sky);
    p.setRenderHint(QPainter::Antialiasing);
    for (const Star &star : stars()) {
        const double y = std::fmod(star.y * sky.height() + m_scroll * star.depth, sky.height());
        const QPointF at(sky.left() + star.x * sky.width(), sky.top() + (y < 0 ? y + sky.height() : y));
        const double twinkle = 0.55 + 0.45 * std::sin(now / 400.0 + star.twinkle);
        p.setOpacity(twinkle);
        if (star.size >= 4) {
            drawStarShape(p, at, star.size, star.color);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(star.color);
            p.drawEllipse(at, star.size, star.size);
        }
    }
    p.setOpacity(1);
    const double swirlBand = sky.height() + 60;
    for (int i = 0; i < 3; ++i) {
        const double base = (0.15 + i * 0.36) * swirlBand;
        double y = std::fmod(base + m_scroll * 1.3, swirlBand);
        if (y < 0)
            y += swirlBand;
        const double x = sky.left() + sky.width() * (i == 1 ? 0.55 : 0.05 + i * 0.12);
        drawSwirl(p, QPointF(x, sky.top() + y - 30), 26 + i * 6);
    }
    p.restore();

    drawGround(p, sky, ground);

    // Altitude axis
    p.setFont(Theme::pixelFont(11));
    for (int i = 0; i <= 4; ++i) {
        const double altitude = altitudeScale * i / 4;
        const int y = static_cast<int>(yFor(altitude));
        p.setPen(QPen(Theme::Ink, 3));
        p.drawLine(axis.right() - 10, y, axis.right(), y);
        p.setPen(Theme::Ink);
        const QString text = i == 0 ? QStringLiteral("0 m") : QString::number(altitude, 'f', 0);
        p.drawText(QRect(2, std::max(axis.top() + 2, y - 14), kAxisWidth - 6, 14), Qt::AlignLeft | Qt::AlignBottom, text);
    }
    p.setPen(QPen(Theme::Ink, 3));
    p.drawLine(axis.right() + 1, axis.top(), axis.right() + 1, axis.bottom());

    // Status bar: what's happening now on the left, the last event on the
    // right. It sits above the sky so Kirby and his trail never cover it.
    QString message;
    switch (m_state) {
    case StateIdle:
        message = QStringLiteral("READY ON PAD");
        break;
    case StateDescent:
        message = m_onGround ? QStringLiteral("TOUCHDOWN!")
                             : QStringLiteral("PARASOL OPEN! %1 m/s").arg(std::abs(m_speed), 0, 'f', 1);
        break;
    default:
        message = QStringLiteral("%1 %2 m/s").arg(stateShortName(m_state)).arg(m_speed, 0, 'f', 1);
        break;
    }
    if (!m_hasAltitude)
        message = QStringLiteral("NO ALTITUDE DATA");
    p.fillRect(status, Qt::white);
    p.setPen(QPen(Theme::Ink, 3));
    p.drawLine(status.left(), status.bottom(), status.right(), status.bottom());
    p.setPen(Theme::Ink);
    p.setFont(Theme::pixelFont(11));
    p.drawText(status.adjusted(8, 2, -8, -2), Qt::AlignLeft | Qt::AlignVCenter, message);
    p.setFont(Theme::pixelFont(10, false));
    p.drawText(status.adjusted(8, 2, -8, -2), Qt::AlignRight | Qt::AlignVCenter, m_lastEvent);

    p.setRenderHint(QPainter::Antialiasing);

    // High-water mark
    if (m_maxAltitude > 1) {
        const int y = static_cast<int>(yFor(m_maxAltitude));
        p.setPen(QPen(Theme::Ink, 2, Qt::DashLine));
        p.drawLine(sky.left(), y, sky.right(), y);
        p.setPen(Theme::Ink);
        p.drawText(QRect(sky.left(), y - 16, sky.width() - 8, 14), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("HI %1m").arg(m_maxAltitude, 0, 'f', 0));
    }

    // Trail of white sparkle dots
    if (m_trail.size() > 1) {
        QPolygonF line;
        for (const QPointF &point : m_trail)
            line << QPointF(xFor(point.x()), yFor(point.y()));
        p.setPen(QPen(QColor(255, 255, 255, 220), 3, Qt::DotLine, Qt::RoundCap));
        p.drawPolyline(line);
    }

    // Kirby at the latest position: standing on the ground before launch
    // and after landing, floating puffed up on the way up, swaying under
    // the parasol on the way down
    if (!m_trail.isEmpty()) {
        const QPointF last = m_trail.last();
        const bool parasol = m_state == StateDescent && !m_onGround;
        const bool floating = !parasol && !m_onGround;
        const QSize size = parasol ? QSize(34, 50) : QSize(36, 36);

        double x = xFor(last.x());
        double feet = yFor(last.y());
        if (floating)
            feet += 3 * std::sin(now / 120.0);  // flapping bob
        if (parasol)
            x += 4 * std::sin(now / 500.0);     // gentle sway

        QRect sprite(0, 0, size.width(), size.height());
        sprite.moveCenter(QPoint(static_cast<int>(x), 0));
        sprite.moveBottom(static_cast<int>(feet));
        if (sprite.top() < sky.top() + 2)
            sprite.moveTop(sky.top() + 2);

        QString frame;
        if (parasol)
            frame = Sprites::frameAt(Sprites::KirbyParasol, now, 10, true);
        else if (floating)
            frame = Sprites::frameAt(Sprites::KirbyFloat, now, 10);  // the GIF runs at 100 ms a frame
        else
            frame = Sprites::frameAt(Sprites::KirbyGround, now, 12);  // walking, on the pad / landed
        Sprites::draw(p, sprite, frame);

        const QString text = QStringLiteral("%1m").arg(last.y(), 0, 'f', 0);
        QRect tag(0, 0, p.fontMetrics().horizontalAdvance(text) + 12, 18);
        tag.moveCenter(sprite.center());
        tag.moveRight(sprite.left() - 4);
        if (tag.left() < sky.left() + 2)
            tag.moveLeft(sprite.right() + 4);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(Theme::Ink, 2));
        p.setBrush(Qt::white);
        p.drawRect(tag);
        p.setPen(Theme::Blue);
        p.drawText(tag, Qt::AlignCenter, text);
    }

    // Frame
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(Theme::Ink, 3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(1, 1, -2, -2));
}
