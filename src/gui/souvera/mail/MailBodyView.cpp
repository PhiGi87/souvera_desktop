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
constexpr const char *kRemoteAttributePattern =
    R"((src|background)\s*=\s*["'](https?://[^"']+)["'])";
constexpr const char *kBlockedPixel =
    "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";

// Restrict the network fetches to a sane size.
constexpr qint64 MaxImageBytes = 8 * 1024 * 1024;

} // namespace

MailBodyView::MailBodyView(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
    setOpenLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &MailBodyView::onAnchorClicked);
}

void MailBodyView::setNetworkAccessManager(QNetworkAccessManager *nam)
{
    _nam = nam;
}

void MailBodyView::setMailBody(const QString &html, bool allowRemote)
{
    _rawHtml = html;
    _remoteLoaded = allowRemote;
    render(html, allowRemote);
}

bool MailBodyView::prefersDarkPaper(const QString &html)
{
    // Classify every background color declaration; a mail that mostly uses
    // dark backgrounds gets a dark paper, everything else a light one.
    static const QRegularExpression bgRe(
        QStringLiteral("(?:background(?:-color)?\\s*[:=]\\s*[\"']?\\s*#([0-9a-fA-F]{6}))"),
        QRegularExpression::CaseInsensitiveOption);

    auto dark = 0;
    auto light = 0;
    auto matches = bgRe.globalMatch(html);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const auto hex = match.captured(1);
        const auto value = hex.toInt(nullptr, 16);
        const auto r = (value >> 16) & 0xff;
        const auto g = (value >> 8) & 0xff;
        const auto b = value & 0xff;
        // Perceived luminance (ITU-R BT.601)
        const auto lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
        if (lum < 0.4) {
            ++dark;
        } else if (lum > 0.6) {
            ++light;
        }
        if (dark + light > 40) break; // enough samples
    }
    return dark > light;
}

QString MailBodyView::blockRemoteResources(const QString &html, int *blockedCount)
{
    static const QRegularExpression remoteRe(
        QString::fromLatin1(kRemoteAttributePattern),
        QRegularExpression::CaseInsensitiveOption);

    auto blocked = 0;
    auto result = html;
    auto matches = remoteRe.globalMatch(html);
    QList<QPair<int, int>> spans; // start, length of the attribute value region
    QList<QString> replacements;
    while (matches.hasNext()) {
        const auto match = matches.next();
        spans.append({match.capturedStart(2), match.capturedLength(2)});
        replacements.append(QString::fromLatin1(kBlockedPixel));
        ++blocked;
    }
    // Replace from the back to keep offsets stable.
    for (int i = spans.size() - 1; i >= 0; --i) {
        result.replace(spans.at(i).first, spans.at(i).second, replacements.at(i));
    }
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
    const auto paper = darkPaper ? QStringLiteral("#1b1f27") : QStringLiteral("#ffffff");
    const auto ink = darkPaper ? QStringLiteral("#e8ecf2") : QStringLiteral("#1a2230");
    const auto accent = SouveraTheme::instance()->color(SouveraTheme::Color::Accent).name();

    QString banner;
    if (!allowRemote && blocked > 0) {
        banner = QStringLiteral(
            "<div style='background:%1; color:%2; border:1px solid %3;"
            " border-radius:8px; padding:8px 12px; margin-bottom:10px; font-size:12px;'>"
            "\U0001F512 <b>%4 externe(s) Element</b> blockiert \u2014 "
            "<a href='souvera-loadremote:1' style='color:%5;'>Inhalte laden</a></div>")
            .arg(darkPaper ? QStringLiteral("#2a3140") : QStringLiteral("#eef4fb"),
                 ink, darkPaper ? QStringLiteral("#3a4256") : QStringLiteral("#d7e4f3"))
            .arg(blocked)
            .arg(accent);
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
    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
        QDesktopServices::openUrl(url);
    }
}

void MailBodyView::loadRemoteImages(const QString &html)
{
    if (!_nam) {
        // No network access at hand — render at least without the gate.
        setMailBody(html, true);
        return;
    }

    static const QRegularExpression remoteRe(
        QString::fromLatin1(kRemoteAttributePattern),
        QRegularExpression::CaseInsensitiveOption);

    QSet<QString> urls;
    auto matches = remoteRe.globalMatch(html);
    while (matches.hasNext()) {
        urls.insert(matches.next().captured(2));
    }

    if (urls.isEmpty()) {
        setMailBody(html, true);
        return;
    }

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
        connect(reply, &QNetworkReply::finished, this, [this, reply, imageUrl, html]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError
                && reply->size() <= MaxImageBytes) {
                const auto data = reply->readAll();
                const auto mime = reply->header(QNetworkRequest::ContentTypeHeader)
                                      .toString()
                                      .section(QLatin1Char(';'), 0, 0);
                if (!mime.startsWith(QLatin1String("image/"))) {
                    _imageCache.insert(imageUrl, QString::fromUtf8(kBlockedPixel));
                } else {
                    _imageCache.insert(imageUrl, QStringLiteral("data:%1;base64,%2")
                        .arg(mime, QString::fromLatin1(data.toBase64())));
                }
            } else {
                _imageCache.insert(imageUrl, QString::fromUtf8(kBlockedPixel));
            }
            if (--_pendingImages <= 0) {
                auto inlined = html;
                for (auto it = _imageCache.cbegin(); it != _imageCache.cend(); ++it) {
                    inlined.replace(it.key(), it.value());
                }
                emit remoteContentLoaded(_imageCache.size());
                setMailBody(inlined, true);
            }
        });
    }
}

} // namespace OCC
