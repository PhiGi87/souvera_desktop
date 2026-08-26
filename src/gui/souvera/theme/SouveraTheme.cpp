/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SouveraTheme.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <QPainter>
#include <QPixmapCache>
#include <QSettings>
#include <QSvgRenderer>

Q_LOGGING_CATEGORY(lcSouveraTheme, "souvera.theme")

namespace OCC {

namespace {

struct Palette {
    QColor background;
    QColor contentBackground;
    QColor sidebarBackground;
    QColor surface;
    QColor surfaceHover;
    QColor border;
    QColor borderStrong;
    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor textDisabled;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor onAccent;
    QColor danger;
    QColor success;
    QColor warning;
};

const Palette &darkPalette()
{
    static const Palette p{
        QStringLiteral("#0f1729"),
        QStringLiteral("#161a2e"),
        QStringLiteral("#0b1120"),
        QStringLiteral("#1e293b"),
        QStringLiteral("#1a2540"),
        QStringLiteral("#1e293b"),
        QStringLiteral("#334155"),
        QStringLiteral("#e2e8f0"),
        QStringLiteral("#94a3b8"),
        QStringLiteral("#64748b"),
        QStringLiteral("#475569"),
        QStringLiteral("#4bbfea"),
        QStringLiteral("#38bdf8"),
        QStringLiteral("#0ea5e9"),
        QStringLiteral("#0f1729"),
        QStringLiteral("#ef4444"),
        QStringLiteral("#22c55e"),
        QStringLiteral("#eab308"),
    };
    return p;
}

const Palette &lightPalette()
{
    static const Palette p{
        QStringLiteral("#eef1f6"),
        QStringLiteral("#ffffff"),
        QStringLiteral("#e7ebf2"),
        QStringLiteral("#ffffff"),
        QStringLiteral("#f1f5f9"),
        QStringLiteral("#e2e8f0"),
        QStringLiteral("#cbd5e1"),
        QStringLiteral("#0f172a"),
        QStringLiteral("#475569"),
        QStringLiteral("#64748b"),
        QStringLiteral("#94a3b8"),
        QStringLiteral("#0284c7"),
        QStringLiteral("#0369a1"),
        QStringLiteral("#075985"),
        QStringLiteral("#ffffff"),
        QStringLiteral("#dc2626"),
        QStringLiteral("#16a34a"),
        QStringLiteral("#d97706"),
    };
    return p;
}

const Palette &paletteFor(SouveraTheme::Theme theme)
{
    return theme == SouveraTheme::Theme::Dark ? darkPalette() : lightPalette();
}

QString rgba(const QColor &color, int alphaPercent)
{
    const auto alpha = alphaPercent / 100.0;
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(alpha, 'g', 2));
}

QPixmap tintedSvgPixmap(const QString &resourcePath, const QColor &color, int size)
{
    const auto cacheKey = resourcePath + QLatin1Char(',') + color.name() + QLatin1Char(',') + QString::number(size);
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) {
        return cached;
    }

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcSouveraTheme) << "Could not open SVG resource" << resourcePath;
        return {};
    }
    const auto svgData = file.readAll();

    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        qCWarning(lcSouveraTheme) << "Invalid SVG resource" << resourcePath;
        return {};
    }

    const auto renderSize = QSize(size * 2, size * 2);

    QImage mask(renderSize, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    {
        QPainter maskPainter(&mask);
        renderer.render(&maskPainter);
    }

    QImage tinted(renderSize, QImage::Format_ARGB32);
    tinted.fill(color);
    {
        QPainter tintPainter(&tinted);
        tintPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        tintPainter.drawImage(0, 0, mask);
    }

    auto result = QPixmap::fromImage(tinted);
    result.setDevicePixelRatio(2.0);
    if (!result.isNull()) {
        QPixmapCache::insert(cacheKey, result);
    }
    return result;
}

} // namespace

SouveraTheme *SouveraTheme::instance()
{
    static auto *theme = new SouveraTheme(qApp);
    return theme;
}

SouveraTheme::SouveraTheme(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    const auto saved = settings.value(QStringLiteral("souvera/theme"), QStringLiteral("dark")).toString();
    _theme = saved.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0 ? Theme::Light : Theme::Dark;
}

void SouveraTheme::setTheme(Theme theme)
{
    if (_theme == theme) {
        return;
    }
    _theme = theme;

    QSettings settings;
    settings.setValue(QStringLiteral("souvera/theme"),
                      _theme == Theme::Light ? QStringLiteral("light") : QStringLiteral("dark"));

    applyStyleSheet();
    emit themeChanged();
}

void SouveraTheme::toggleTheme()
{
    setTheme(_theme == Theme::Dark ? Theme::Light : Theme::Dark);
}

QColor SouveraTheme::color(Color role) const
{
    const auto &p = paletteFor(_theme);
    switch (role) {
    case Color::Background:
        return p.background;
    case Color::ContentBackground:
        return p.contentBackground;
    case Color::SidebarBackground:
        return p.sidebarBackground;
    case Color::Surface:
        return p.surface;
    case Color::SurfaceHover:
        return p.surfaceHover;
    case Color::Border:
        return p.border;
    case Color::BorderStrong:
        return p.borderStrong;
    case Color::TextPrimary:
        return p.textPrimary;
    case Color::TextSecondary:
        return p.textSecondary;
    case Color::TextMuted:
        return p.textMuted;
    case Color::TextDisabled:
        return p.textDisabled;
    case Color::Accent:
        return p.accent;
    case Color::AccentHover:
        return p.accentHover;
    case Color::AccentPressed:
        return p.accentPressed;
    case Color::OnAccent:
        return p.onAccent;
    case Color::Danger:
        return p.danger;
    case Color::Success:
        return p.success;
    case Color::Warning:
        return p.warning;
    }
    return {};
}

QPixmap SouveraTheme::pixmap(const QString &iconName, Color tint, int size) const
{
    const auto path = QStringLiteral(":/souvera/icons/") + iconName + QStringLiteral(".svg");
    return tintedSvgPixmap(path, color(tint), size);
}

QString SouveraTheme::styleSheet() const
{
    QFile file(QStringLiteral(":/souvera/souvera.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcSouveraTheme) << "Could not load souvera.qss";
        return {};
    }

    auto qss = QString::fromUtf8(file.readAll());

    const auto &p = paletteFor(_theme);
    const auto overlay = _theme == Theme::Dark ? QColor(255, 255, 255) : QColor(0, 0, 0);

    QHash<QString, QString> tokens;
    tokens.insert(QStringLiteral("background"), p.background.name());
    tokens.insert(QStringLiteral("contentBackground"), p.contentBackground.name());
    tokens.insert(QStringLiteral("sidebarBackground"), p.sidebarBackground.name());
    tokens.insert(QStringLiteral("surface"), p.surface.name());
    tokens.insert(QStringLiteral("surfaceHover"), p.surfaceHover.name());
    tokens.insert(QStringLiteral("border"), p.border.name());
    tokens.insert(QStringLiteral("borderStrong"), p.borderStrong.name());
    tokens.insert(QStringLiteral("textPrimary"), p.textPrimary.name());
    tokens.insert(QStringLiteral("textSecondary"), p.textSecondary.name());
    tokens.insert(QStringLiteral("textMuted"), p.textMuted.name());
    tokens.insert(QStringLiteral("textDisabled"), p.textDisabled.name());
    tokens.insert(QStringLiteral("accent"), p.accent.name());
    tokens.insert(QStringLiteral("accentHover"), p.accentHover.name());
    tokens.insert(QStringLiteral("accentPressed"), p.accentPressed.name());
    tokens.insert(QStringLiteral("onAccent"), p.onAccent.name());
    tokens.insert(QStringLiteral("danger"), p.danger.name());
    tokens.insert(QStringLiteral("success"), p.success.name());
    tokens.insert(QStringLiteral("warning"), p.warning.name());
    tokens.insert(QStringLiteral("overlayHover"), rgba(overlay, 4));
    tokens.insert(QStringLiteral("overlayHoverStrong"), rgba(overlay, 6));
    tokens.insert(QStringLiteral("accentOverlay"), rgba(p.accent, 8));
    tokens.insert(QStringLiteral("accentOverlayHover"), rgba(p.accent, 12));

    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it) {
        qss.replace(QStringLiteral("@") + it.key() + QStringLiteral("@"), it.value());
    }

    return qss;
}

void SouveraTheme::applyStyleSheet() const
{
    if (qApp) {
        qApp->setStyleSheet(styleSheet());
    }
}

} // namespace OCC
