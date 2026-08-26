/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SOUVERATHEME_H
#define SOUVERATHEME_H

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QString>

namespace OCC {

/**
 * @brief Central design system for the Souvera workspace.
 *
 * Owns the color palette (dark/light), the tokenized stylesheet and the
 * tinted SVG icon set. All widgets should derive their colors and icons from
 * here instead of hardcoding values.
 */
class SouveraTheme : public QObject
{
    Q_OBJECT

public:
    enum class Theme { Dark, Light };

    enum class Color {
        Background,
        ContentBackground,
        SidebarBackground,
        Surface,
        SurfaceHover,
        Border,
        BorderStrong,
        TextPrimary,
        TextSecondary,
        TextMuted,
        TextDisabled,
        Accent,
        AccentHover,
        AccentPressed,
        OnAccent,
        Danger,
        Success,
        Warning,
    };

    static SouveraTheme *instance();

    [[nodiscard]] Theme theme() const { return _theme; }
    void setTheme(Theme theme);
    void toggleTheme();

    [[nodiscard]] QColor color(Color role) const;
    [[nodiscard]] QPixmap pixmap(const QString &iconName, Color tint = Color::TextSecondary, int size = 24) const;
    [[nodiscard]] QIcon icon(const QString &iconName, Color tint = Color::TextSecondary) const
    {
        return QIcon(pixmap(iconName, tint));
    }

    [[nodiscard]] QString styleSheet() const;
    void applyStyleSheet() const;

signals:
    void themeChanged();

private:
    explicit SouveraTheme(QObject *parent = nullptr);

    Theme _theme = Theme::Dark;
};

} // namespace OCC

#endif // SOUVERATHEME_H
