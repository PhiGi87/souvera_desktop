/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MailBodyView.h"
#include "theme/SouveraTheme.h"

#include <QDesktopServices>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace OCC {

namespace {

// Remote references in tag attributes: quoted, single-quoted and unquoted forms.
// Group 1 is the attribute part, the value sits in exactly one of groups 2-4.
const QRegularExpression kAttrRe(
    QStringLiteral("((?:src|background|poster)\\s*=\\s*)(?:\"([^\"]*)\"|'([^']*)'|([^\\s\"'>][^>\\s]*))"),
    QRegularExpression::CaseInsensitiveOption);

// Remote references inside CSS (style attributes and <style> blocks).
const QRegularExpression kCssUrlRe(
    QStringLiteral("url\\(\\s*(?:\"([^\"]*)\"|'([^']*)'|([^)'\"]+))\\s*\\)"),
    QRegularExpression::CaseInsensitiveOption);

// srcset candidate lists (attribute forms with quotes only).
const QRegularExpression kSrcsetRe(
    QStringLiteral("(srcset\\s*=\\s*)(?:\"([^\"]*)\"|'([^']*)')"),
    QRegularExpression::CaseInsensitiveOption);

// Paper selectors for the luminance heuristic: CSS and HTML-attribute forms.
const QRegularExpression kPaperHexRe(
    QStringLiteral("(?:background(?:-color)?|bgcolor)\\s*[:=]\\s*[\"']?\\s*#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})"),
    QRegularExpression::CaseInsensitiveOption);
const QRegularExpression kPaperRgbRe(
    QStringLiteral("(?:background(?:-color)?|bgcolor)\\s*[:=]\\s*[\"']?\\s*rgba?\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)"),
    QRegularExpression::CaseInsensitiveOption);

// Placeholder for blocked remote resources (1x1 transparent GIF).
constexpr QLatin1String kBlockedPixel(
    "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7");

// Limits for a single remote fetch and for the total inline cache.
constexpr qint64 kMaxImageBytes = 8 * 1024 * 1024;
constexpr qint64 kMaxCacheBytes = 32 * 1024 * 1024;

// Paper palette: dark mails keep a dark paper, everything else a light one,
// so rendered text never ends up dark-on-dark or light-on-light.
constexpr QLatin1String kDarkPaperBg("#1b1f27");
constexpr QLatin1String kDarkPaperInk("#e8ecf2");
constexpr QLatin1String kLightPaperBg("#ffffff");
constexpr QLatin1String kLightPaperInk("#1a2230");
constexpr QLatin1String kDarkBannerBg("#2a3140");
constexpr QLatin1String kDarkBannerBorder("#3a4256");
constexpr QLatin1String kLightBannerBg("#eef4fb");
constexpr QLatin1String kLightBannerBorder("#d7e4f3");

[[nodiscard]] bool isRemoteUrl(const QString &value)
{
    const auto trimmed = value.trimmed();
    return trimmed.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QLatin1String("https://"), Qt::CaseInsensitive);
}

[[nodiscard]] QString decodeHtmlAmp(const QString &url)
{
    return QString(url).replace(QLatin1String("&amp;"), QLatin1String("&"));
}

} // namespace

MailBodyView::MailBodyView(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(false);
    setOpenLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &MailBodyView::onAnchorClicked);
}

void MailBodyView::setNetworkAccessManager(QNetworkAccessManager *nam)
{
    _nam = nam;
}

void MailBodyView::setMailBody(const QString &html, bool allowRemote)
{
    ++_generation;
    for (auto *reply : std::as_const(_activeReplies)) {
        if (reply) {
            reply->abort();
        }
    }
    _activeReplies.clear();
    _pendingImages = 0;

    _rawHtml = html;
    _remoteLoaded = allowRemote;
    render(html, allowRemote);
}

bool MailBodyView::prefersDarkPaper(const QString &html)
{
    auto dark = 0;
    auto light = 0;
    const auto classify = [&dark, &light](double lum) {
        if (lum < 0.4) {
            ++dark;
        } else if (lum > 0.6) {
            ++light;
        }
    };

    auto hexMatches = kPaperHexRe.globalMatch(html);
    while (hexMatches.hasNext() && dark + light <= 40) {
        auto hex = hexMatches.next().captured(1);
        if (hex.size() == 3) {
            hex = QString() + hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
        }
        const auto value = hex.toInt(nullptr, 16);
        const auto r = (value >> 16) & 0xff;
        const auto g = (value >> 8) & 0xff;
        const auto b = value & 0xff;
        // Perceived luminance (ITU-R BT.601)
        classify((0.299 * r + 0.587 * g + 0.114 * b) / 255.0);
    }

    auto rgbMatches = kPaperRgbRe.globalMatch(html);
    while (rgbMatches.hasNext() && dark + light <= 40) {
        const auto match = rgbMatches.next();
        classify((0.299 * match.captured(1).toInt() + 0.587 * match.captured(2).toInt()
                  + 0.114 * match.captured(3).toInt()) / 255.0);
    }

    return dark > light;
}

QSet<QString> MailBodyView::remoteUrls(const QString &html)
{
    QSet<QString> urls;

    auto attrMatches = kAttrRe.globalMatch(html);
    while (attrMatches.hasNext()) {
        const auto match = attrMatches.next();
        const auto value = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
        const auto unquoted = match.captured(4);
        const auto candidate = value.isEmpty() ? unquoted : value;
        if (isRemoteUrl(candidate)) {
            urls.insert(decodeHtmlAmp(candidate.trimmed()));
        }
    }

    auto srcsetMatches = kSrcsetRe.globalMatch(html);
    while (srcsetMatches.hasNext()) {
        const auto match = srcsetMatches.next();
        const auto value = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
        const auto candidates = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto &candidate : candidates) {
            const auto url = candidate.section(QLatin1Char(' '), 0, 0).trimmed();
            if (isRemoteUrl(url)) {
                urls.insert(decodeHtmlAmp(url));
            }
        }
    }

    auto cssMatches = kCssUrlRe.globalMatch(html);
    while (cssMatches.hasNext()) {
        const auto match = cssMatches.next();
        auto value = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        if (value.isEmpty()) {
            value = match.captured(3);
        }
        if (isRemoteUrl(value)) {
            urls.insert(decodeHtmlAmp(value.trimmed()));
        }
    }

    return urls;
}

QString MailBodyView::blockRemoteResources(const QString &html, int *blockedCount)
{
    auto result = html;
    auto blocked = 0;
    QList<QPair<int, int>> spans;
    QList<QString> replacements;
    const auto apply = [&result, &spans, &replacements](int start, int length, const QString &text) {
        spans.append({start, length});
        replacements.append(text);
    };

    // Replace from the back to keep offsets stable.
    const auto flush = [&result, &spans, &replacements]() {
        for (int i = spans.size() - 1; i >= 0; --i) {
            result.replace(spans.at(i).first, spans.at(i).second, replacements.at(i));
        }
        spans.clear();
        replacements.clear();
    };

    auto attrMatches = kAttrRe.globalMatch(result);
    while (attrMatches.hasNext()) {
        const auto match = attrMatches.next();
        const auto value = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
        const auto unquoted = match.captured(4);
        if (isRemoteUrl(value)) {
            apply(match.capturedStart(2), match.capturedLength(2), QString(kBlockedPixel));
            ++blocked;
        } else if (isRemoteUrl(unquoted)) {
            apply(match.capturedStart(4), match.capturedLength(4), QString(kBlockedPixel));
            ++blocked;
        }
    }
    flush();

    auto srcsetMatches = kSrcsetRe.globalMatch(result);
    while (srcsetMatches.hasNext()) {
        const auto match = srcsetMatches.next();
        const auto value = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
        if (!value.contains(QLatin1String("http://"), Qt::CaseInsensitive)
            && !value.contains(QLatin1String("https://"), Qt::CaseInsensitive)) {
            continue;
        }
        const auto candidates = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto &candidate : candidates) {
            if (isRemoteUrl(candidate.section(QLatin1Char(' '), 0, 0))) {
                ++blocked;
            }
        }
        const auto group = match.captured(2).isEmpty() ? 3 : 2;
        apply(match.capturedStart(group), match.capturedLength(group), QString());
    }
    flush();

    auto cssMatches = kCssUrlRe.globalMatch(result);
    while (cssMatches.hasNext()) {
        const auto match = cssMatches.next();
        auto value = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        if (value.isEmpty()) {
            value = match.captured(3);
        }
        if (isRemoteUrl(value)) {
            apply(match.capturedStart(0), match.capturedLength(0),
                  QStringLiteral("url(%1)").arg(QString(kBlockedPixel)));
            ++blocked;
        }
    }
    flush();

    if (blockedCount) {
        *blockedCount = blocked;
    }
    return result;
}

void MailBodyView::render(const QString &html, bool allowRemote)
{
    auto blocked = 0;
    auto body = allowRemote ? html : blockRemoteResources(html, &blocked);

    const auto darkPaper = prefersDarkPaper(html);
    const auto paper = darkPaper ? QString(kDarkPaperBg) : QString(kLightPaperBg);
    const auto ink = darkPaper ? QString(kDarkPaperInk) : QString(kLightPaperInk);
    const auto accent = SouveraTheme::instance()->color(SouveraTheme::Color::Accent).name();

    QString banner;
    if (!allowRemote && blocked > 0) {
        const auto bannerBg = darkPaper ? QString(kDarkBannerBg) : QString(kLightBannerBg);
        const auto bannerBorder = darkPaper ? QString(kDarkBannerBorder) : QString(kLightBannerBorder);
        const auto count = (blocked == 1)
            ? QStringLiteral("<b>1 externes Element</b> blockiert")
            : QStringLiteral("<b>%1 externe Elemente</b> blockiert").arg(blocked);
        banner = QStringLiteral(
            "<div style='background:%1; color:%2; border:1px solid %3;"
            " border-radius:8px; padding:8px 12px; margin-bottom:10px; font-size:12px;'>"
            "\U0001F512 %4 \u2014 <a href='souvera-loadremote:1' style='color:%5;'>Inhalte laden</a></div>")
            .arg(bannerBg, ink, bannerBorder, count, accent);
    }

    const auto wrapped = QStringLiteral(
        "<div style='background:%1; color:%2; padding:14px; font-size:14px;'>%3%4</div>")
        .arg(paper, ink, banner, body);
    setHtml(wrapped);
}

void MailBodyView::onAnchorClicked(const QUrl &url)
{
    if (url.scheme() == QLatin1String("souvera-loadremote")) {
        loadRemoteImages(_rawHtml);
        return;
    }
    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")
        || url.scheme() == QLatin1String("mailto")) {
        QDesktopServices::openUrl(url);
    }
}

void MailBodyView::loadRemoteImages(const QString &html)
{
    if (_remoteLoaded) {
        return;
    }
    if (!_nam) {
        // No network access at hand — render at least without the gate.
        setMailBody(html, true);
        return;
    }

    const auto urls = remoteUrls(html);
    if (urls.isEmpty()) {
        setMailBody(html, true);
        return;
    }

    const auto generation = _generation;
    _pendingImages = urls.size();
    for (const auto &imageUrl : urls) {
        if (_imageCache.contains(imageUrl)) {
            if (--_pendingImages == 0) {
                auto inlined = html;
                for (auto it = _imageCache.cbegin(); it != _imageCache.cend(); ++it) {
                    inlined.replace(it.key(), it.value());
                }
                setMailBody(inlined, true);
            }
            continue;
        }

        QNetworkRequest request{QUrl(imageUrl)};
        request.setTransferTimeout(15000);
        auto *reply = _nam->get(request);
        _activeReplies.append(reply);
        connect(reply, &QNetworkReply::finished, this, [this, reply, imageUrl, html, generation]() {
            _activeReplies.removeOne(reply);
            reply->deleteLater();
            if (generation != _generation) {
                return;
            }
            if (reply->error() == QNetworkReply::NoError
                && reply->size() <= kMaxImageBytes) {
                const auto data = reply->readAll();
                auto mime = reply->header(QNetworkRequest::ContentTypeHeader)
                                .toString()
                                .section(QLatin1Char(';'), 0, 0)
                                .trimmed();
                if (!mime.startsWith(QLatin1String("image/"))) {
                    insertCachedImage(imageUrl, QString(kBlockedPixel));
                } else {
                    insertCachedImage(imageUrl, QStringLiteral("data:%1;base64,%2")
                        .arg(mime, QString::fromLatin1(data.toBase64())));
                }
            } else {
                insertCachedImage(imageUrl, QString(kBlockedPixel));
            }
            if (--_pendingImages <= 0) {
                auto inlined = html;
                for (auto it = _imageCache.cbegin(); it != _imageCache.cend(); ++it) {
                    inlined.replace(it.key(), it.value());
                }
                setMailBody(inlined, true);
            }
        });
    }
}

void MailBodyView::insertCachedImage(const QString &url, const QString &dataUri)
{
    if (_cacheBytes + dataUri.size() > kMaxCacheBytes) {
        _imageCache.clear();
        _cacheBytes = 0;
    }
    _imageCache.insert(url, dataUri);
    _cacheBytes += dataUri.size();
}

} // namespace OCC
