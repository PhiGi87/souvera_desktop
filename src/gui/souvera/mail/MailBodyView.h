/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAILBODYVIEW_H
#define MAILBODYVIEW_H

#include <QTextBrowser>

class QNetworkAccessManager;

namespace OCC {

/**
 * @brief Mail body renderer with per-mail background adaptation and a
 *        privacy gate for remote content.
 *
 * - Remote resources (http/https images) are blocked by default and a banner
 *   offers to load them; loading fetches the images and inlines them as data
 *   URIs.
 * - The "paper" background/foreground follows the mail's own design: dark
 *   mails keep a dark paper, everything else renders on a light paper — so
 *   text is never dark-on-dark.
 */
class MailBodyView : public QTextBrowser
{
    Q_OBJECT
public:
    explicit MailBodyView(QWidget *parent = nullptr);

    void setNetworkAccessManager(QNetworkAccessManager *nam);

    /** Renders a mail body. Remote content stays blocked unless allowRemote. */
    void setMailBody(const QString &html, bool allowRemote = false);

signals:
    void remoteContentLoaded(int count);

private slots:
    void onAnchorClicked(const QUrl &url);

private:
    void render(const QString &html, bool allowRemote);
    void loadRemoteImages(const QString &html);
    [[nodiscard]] static bool prefersDarkPaper(const QString &html);
    [[nodiscard]] static QString blockRemoteResources(const QString &html, int *blockedCount);

    QNetworkAccessManager *_nam = nullptr;
    QString _rawHtml;
    bool _remoteLoaded = false;
    int _pendingImages = 0;
    QHash<QString, QString> _imageCache;
};

} // namespace OCC

#endif // MAILBODYVIEW_H
