#ifndef UI_PANELSTYLE_H
#define UI_PANELSTYLE_H

#include <QString>

inline QString panelButtonStyle(const QString &background,
                                const QString &foreground = QStringLiteral("white"),
                                const QString &pressedColor = QString())
{
    const QString pressed = pressedColor.isEmpty() ? foreground : pressedColor;
    return QStringLiteral(
               "QPushButton {"
               "  background:%1; color:%2; border:1px solid #20262c;"
               "  border-top:2px solid #ffffff;"
               "  border-left:2px solid #eef1f4;"
               "  border-right:2px solid #4d555d;"
               "  border-bottom:3px solid #333a41;"
               "  border-radius:4px; padding:4px 6px 3px 6px;"
               "  font-weight:bold;"
               "}"
               "QPushButton:hover {"
               "  border-top-color:#ffffff;"
               "  border-left-color:#ffffff;"
               "}"
               "QPushButton:pressed {"
               "  background:%3;"
               "  border-top:2px solid #333a41;"
               "  border-left:2px solid #4d555d;"
               "  border-right:2px solid #eef1f4;"
               "  border-bottom:2px solid #ffffff;"
               "  padding-top:6px; padding-bottom:1px;"
               "}"
               "QPushButton:checked {"
               "  background:#24934d; color:#ffffff;"
               "  border-top:2px solid #9ef0bb;"
               "  border-left:2px solid #58c77e;"
               "  border-right:2px solid #0e5d2c;"
               "  border-bottom:3px solid #083d1c;"
               "}")
        .arg(background, foreground, pressed);
}

#endif
