#include "pagebackground.h"
#include "sprites.h"

#include <QPainter>
#include <QStyleOption>

#include <algorithm>
#include <cmath>

namespace {

const QString kBackground = QStringLiteral("background");

// The sunset is drawn this many times smaller than the window, then
// scaled up with nearest-neighbour so every art pixel is a clean square
constexpr int kPixel = 4;

// Sky bands from the top of the window down to the horizon
const QColor kSkyBands[] = {
    QColor("#ffe7a8"), QColor("#ffd391"), QColor("#ffbc8a"), QColor("#ffa08e"),
    QColor("#f78aa2"), QColor("#dc7fb6"), QColor("#ae7bcb"), QColor("#8574d0"),
};
constexpr double kHorizon = 0.74;  // fraction of the height

// Pixel-art synthwave sunset: banded sky with dithered edges, a striped sun
// on the horizon, two mountain ranges and a neon grid on the ground
QImage drawSunset(const QSize &size)
{
    const int w = std::max(1, size.width() / kPixel);
    const int h = std::max(1, size.height() / kPixel);
    QImage image(w, h, QImage::Format_RGB32);
    const int horizon = static_cast<int>(h * kHorizon);

    // Sky: hard bands, with a checkerboard row where two bands meet
    const int bands = static_cast<int>(std::size(kSkyBands));
    for (int y = 0; y < horizon; ++y) {
        const double position = double(y) / horizon * bands;
        const int band = std::min(bands - 1, static_cast<int>(position));
        const bool edge = band + 1 < bands && position - band > 0.86;
        for (int x = 0; x < w; ++x) {
            const bool dither = edge && (x + y) % 2 == 0;
            image.setPixelColor(x, y, kSkyBands[dither ? band + 1 : band]);
        }
    }

    QPainter p(&image);

    // Clouds: stacks of flat rectangles
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#fff4dc"));
    const QPointF clouds[] = {{0.12, 0.06}, {0.62, 0.11}, {0.86, 0.04}, {0.34, 0.2}};
    for (const QPointF &cloud : clouds) {
        const int cx = static_cast<int>(cloud.x() * w), cy = static_cast<int>(cloud.y() * h);
        p.drawRect(cx, cy + 2, 16, 3);
        p.drawRect(cx + 3, cy, 9, 3);
        p.drawRect(cx + 10, cy + 1, 4, 2);
    }

    // Sun: sits on the horizon, with stripes cut out of its lower half
    const int sunRadius = std::max(6, h / 5);
    const QPoint sun(w / 2, horizon);
    for (int y = sun.y() - sunRadius; y <= sun.y(); ++y) {
        const double t = double(y - (sun.y() - sunRadius)) / sunRadius;
        const int gap = 2 + static_cast<int>(t * 3);
        if (t > 0.45 && (sun.y() - y) % (gap + 2) < 1 + static_cast<int>(t * 2))
            continue;
        const int half = static_cast<int>(std::sqrt(double(sunRadius * sunRadius - (sun.y() - y) * (sun.y() - y))));
        const QColor colour = QColor::fromRgbF(1.0, 0.92 - 0.45 * t, 0.35 + 0.15 * t);
        p.fillRect(sun.x() - half, y, half * 2 + 1, 1, colour);
    }

    // Mountains: far range then near range, as stepped columns
    auto drawRange = [&](const QColor &colour, double height, double roughness, int seed) {
        for (int x = 0; x < w; ++x) {
            const double n = std::sin(x * 0.07 + seed) * 0.5 + std::sin(x * 0.023 + seed * 2.1) * 0.8
                             + std::sin(x * 0.17 + seed * 0.7) * roughness;
            const int top = horizon - static_cast<int>(h * height * (0.55 + 0.25 * n));
            p.fillRect(x, top, 1, horizon - top + 1, colour);
        }
    };
    drawRange(QColor("#6a58b8"), 0.16, 0.25, 1);
    drawRange(QColor("#45398f"), 0.09, 0.4, 4);

    // Ground with a neon perspective grid
    p.fillRect(0, horizon, w, h - horizon, QColor("#2a2366"));
    p.setPen(QColor("#ff5fc8"));
    for (int i = 1; i < h - horizon; i = i * 2 + 1)
        p.drawLine(0, horizon + i, w, horizon + i);
    for (int i = -12; i <= 12; ++i)
        p.drawLine(QPointF(w / 2.0 + i * 4, horizon), QPointF(w / 2.0 + i * 40, h));

    p.end();
    return image.scaled(w * kPixel, h * kPixel, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

} // namespace

PageBackground::PageBackground(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
}

void PageBackground::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    // Stylesheet background first, so nothing shows through at the edges
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &p, this);

    // assets/sprites/background.png if there is one (scaled to cover the
    // window), otherwise the pixel sunset. Redrawn only on a resize.
    if (m_scaledFor != size()) {
        const QPixmap image = Sprites::get(kBackground);
        m_scaled = image.isNull()
                       ? QPixmap::fromImage(drawSunset(size()))
                       : image.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);
        m_scaledFor = size();
    }
    p.drawPixmap((width() - m_scaled.width()) / 2, (height() - m_scaled.height()) / 2, m_scaled);
}
