#include "theme.h"

#include <QDir>
#include <QFontDatabase>

namespace Theme {

namespace {
QString s_family = QStringLiteral("DejaVu Sans Mono");
bool s_customFont = false;
} // namespace

void loadFonts()
{
    // CMake compiles every .ttf/.otf in assets/fonts into :/fonts
    const QStringList files = QDir(QStringLiteral(":/fonts")).entryList(QDir::Files);
    for (const QString &file : files) {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/") + file);
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!s_customFont && !families.isEmpty()) {
            s_family = families.first();
            s_customFont = true;
        }
    }
}

QFont pixelFont(int pixelSize, bool bold)
{
    QFont font(s_family);
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    // Pixel fonts look blurry when smoothed
    if (s_customFont)
        font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

QString styleSheet()
{
    return QStringLiteral(R"(
        QWidget#page {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #ffd79a, stop:0.30 #f6a978, stop:0.48 #8a7fd0,
                stop:0.58 #6fb7f0, stop:0.60 #6fd36a, stop:1 #3fa84a);
        }
        QLabel { color: %1; background: transparent; }
        QFrame#retroPanel { background: %2; border: 3px solid %1; }
        QFrame#tile { background: %3; border: 3px solid %1; }
        QFrame#modeBox { background: white; border: 3px solid %1; }
    )")
        .arg(Ink.name(), Cream.name(), Paper.name());
}

} // namespace Theme
