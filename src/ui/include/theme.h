#ifndef THEME_H
#define THEME_H

/*
 * theme.h
 * Purpose:
 *   One place for the retro look (req 17): the colour palette, the
 *   pixel font and the app-wide stylesheet. Change a colour here and it
 *   changes everywhere.
 */

#include <QColor>
#include <QFont>
#include <QString>

namespace Theme {

inline const QColor Ink("#1b1b1b");
inline const QColor Cream("#fdf6e3");
inline const QColor Paper("#fffdf7");
inline const QColor Beige("#e8dcc0");
inline const QColor Sky("#5c94fc");
inline const QColor SkyAxis("#e6ecff");
inline const QColor Blue("#2563eb");
inline const QColor Red("#e03c28");
inline const QColor Orange("#f5a623");
inline const QColor Yellow("#f8d84a");
inline const QColor Green("#43b047");
inline const QColor Lime("#c6f04a");
inline const QColor LightBlue("#a8e4ff");
inline const QColor Brick("#c84c0c");
inline const QColor Grey("#9a9a9a");

// Game & Watch handheld round the status screen (handheldshell.cpp)
inline const QColor ShellWhite("#f2f0ea");
inline const QColor ShellBlue("#6cb6e4");
inline const QColor Lcd("#c6ccb8");
inline const QColor ButtonRed("#c8323c");
inline const QColor ButtonGrey("#8f8f8a");

// Loads any font in assets/fonts. Call once, after QApplication exists.
void loadFonts();

// The pixel font from assets/fonts if there is one, else a bold monospace
QFont pixelFont(int pixelSize, bool bold = true);

QString styleSheet();

} // namespace Theme

#endif // THEME_H
