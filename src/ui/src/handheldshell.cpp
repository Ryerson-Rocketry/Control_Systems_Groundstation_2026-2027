#include "handheldshell.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>

namespace {

constexpr int kRim = 4;           // white edge round the faceplate
constexpr int kSide = 42;         // faceplate either side of the bezel
constexpr int kBezelTop = 20;     // bezel band holding the red title
constexpr int kBezelBottom = 16;  // bezel band holding the Nintendo logo and caption
constexpr int kBezelSide = 6;
constexpr int kLcdBorder = 3;
constexpr int kLightMs = 150;     // how long the light stays on per flash

void drawCaption(QPainter &p, const QPoint &centre, const QString &text)
{
    p.setPen(Qt::white);
    p.setFont(Theme::pixelFont(9));
    p.drawText(QRect(centre.x() - 30, centre.y() - 6, 60, 12), Qt::AlignCenter, text);
}

// Red round button sitting in a white ring
void drawRedButton(QPainter &p, const QPoint &centre, int radius)
{
    p.setPen(QPen(QColor("#9a9a9a"), 1));
    p.setBrush(Theme::ShellWhite);
    p.drawEllipse(centre, radius + 3, radius + 3);
    p.setPen(QPen(QColor("#7a1c22"), 1));
    p.setBrush(Theme::ButtonRed);
    p.drawEllipse(centre, radius, radius);
}

} // namespace

HandheldShell::HandheldShell(const QString &title, const QString &caption, QWidget *parent)
    : QWidget(parent), m_title(title), m_caption(caption)
{
    m_screen = new QWidget;

    // Keeps the LCD contents inside the LCD (see lcdRect)
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(kRim + kSide + kBezelSide + kLcdBorder + 1,
                               kRim + kBezelTop + kLcdBorder + 1,
                               kRim + kSide + kBezelSide + kLcdBorder + 1,
                               kRim + kBezelBottom + kLcdBorder + 1);
    layout->addWidget(m_screen);

    m_lightTimer = new QTimer(this);
    m_lightTimer->setSingleShot(true);
    m_lightTimer->setInterval(kLightMs);
    connect(m_lightTimer, &QTimer::timeout, this, [this] {
        m_light = false;
        update();
    });
}

void HandheldShell::flashLight()
{
    m_light = true;
    m_lightTimer->start();
    update();
}

QRect HandheldShell::bezelRect() const
{
    return rect().adjusted(kRim + kSide, kRim, -(kRim + kSide), -kRim);
}

QRect HandheldShell::lcdRect() const
{
    return bezelRect().adjusted(kBezelSide, kBezelTop, -kBezelSide, -kBezelBottom);
}

void HandheldShell::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Body: white rim, blue faceplate
    p.setPen(QPen(Theme::Ink, 3));
    p.setBrush(Theme::ShellWhite);
    p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 8, 8);
    p.setPen(QPen(QColor("#b8b8b0"), 1));
    p.setBrush(Theme::ShellBlue);
    p.drawRoundedRect(rect().adjusted(kRim, kRim, -kRim, -kRim), 5, 5);

    // Bezel with the title above the screen and the caption below it
    const QRect bezel = bezelRect();
    p.setPen(QPen(QColor("#a8a8a0"), 2));
    p.setBrush(Theme::ShellWhite);
    p.drawRoundedRect(bezel, 5, 5);

    p.setFont(Theme::pixelFont(13));
    const QRect titleBand(bezel.left(), bezel.top(), bezel.width(), kBezelTop);
    p.setPen(Theme::Ink);
    p.drawText(titleBand.translated(1, 1), Qt::AlignCenter, m_title);
    p.setPen(Theme::ButtonRed);
    p.drawText(titleBand, Qt::AlignCenter, m_title);

    // Under the screen: red Nintendo logo (word in an outlined oval) on the
    // left, the caption on the right, like the real Game & Watch
    const QRect bottomBand(bezel.left() + 8, bezel.bottom() - kBezelBottom, bezel.width() - 16, kBezelBottom);
    QFont logoFont(QStringLiteral("DejaVu Sans"));
    logoFont.setPixelSize(10);
    logoFont.setBold(true);
    p.setFont(logoFont);
    const int logoWidth = p.fontMetrics().horizontalAdvance(QStringLiteral("Nintendo")) + 12;
    const QRect logo(bottomBand.left(), bottomBand.center().y() - 6, logoWidth, 13);
    p.setPen(QPen(Theme::ButtonRed, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(logo, 6.5, 6.5);
    p.setPen(Theme::ButtonRed);
    p.drawText(logo, Qt::AlignCenter, QStringLiteral("Nintendo"));

    p.setPen(QColor("#9a9a9a"));
    p.setFont(Theme::pixelFont(11));
    p.drawText(bottomBand, Qt::AlignRight | Qt::AlignVCenter, m_caption);

    // LCD glass
    p.setPen(QPen(QColor("#3a3a3a"), kLcdBorder));
    p.setBrush(Theme::Lcd);
    p.drawRect(lcdRect());

    // Left: D-pad
    const int leftX = kRim + kSide / 2;
    const QPoint pad(leftX, height() / 2 + 10);
    p.setPen(QPen(QColor("#9a9a9a"), 1));
    p.setBrush(Theme::ShellWhite);
    p.drawRoundedRect(QRect(pad.x() - 8, pad.y() - 26, 16, 52), 6, 6);
    p.drawRoundedRect(QRect(pad.x() - 26, pad.y() - 8, 52, 16), 6, 6);
    p.setPen(QPen(QColor("#7a1c22"), 1));
    p.setBrush(Theme::ButtonRed);
    for (const QPoint &offset : {QPoint(0, -17), QPoint(0, 17), QPoint(-17, 0), QPoint(17, 0)})
        p.drawEllipse(pad + offset, 6, 6);
    drawCaption(p, QPoint(leftX, height() - kRim - 14), QStringLiteral("MOVE"));

    // Right: packet light, then B and A buttons
    const int rightX = width() - kRim - kSide / 2;
    p.setPen(QPen(QColor("#555"), 1));
    p.setBrush(m_light ? QColor("#ff3030") : QColor("#6a6a6a"));
    p.drawEllipse(QPoint(rightX, kRim + 26), 4, 4);
    drawCaption(p, QPoint(rightX, kRim + 40), QStringLiteral("RX"));

    drawRedButton(p, QPoint(rightX, height() / 2 - 4), 11);
    drawCaption(p, QPoint(rightX, height() / 2 + 18), QStringLiteral("B"));
    drawRedButton(p, QPoint(rightX, height() - kRim - 44), 11);
    drawCaption(p, QPoint(rightX, height() - kRim - 22), QStringLiteral("A"));
}
