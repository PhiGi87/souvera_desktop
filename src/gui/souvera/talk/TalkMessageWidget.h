/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TALKMESSAGEWIDGET_H
#define TALKMESSAGEWIDGET_H

#include <QWidget>
#include <QJsonObject>
#include <QLabel>

namespace OCC {

class TalkMessageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TalkMessageWidget(bool isOwn, QWidget *parent = nullptr);

    void setMessage(const QJsonObject &msg);

    [[nodiscard]] qint64 messageId() const { return _messageId; }

private:
    void applyBubbleStyle();

    bool _isOwn = false;
    qint64 _messageId = 0;

    QLabel *_avatarLabel = nullptr;
    QLabel *_nameLabel = nullptr;
    QLabel *_timestampLabel = nullptr;
    QLabel *_textLabel = nullptr;
    QWidget *_bubble = nullptr;
};

} // namespace OCC

#endif // TALKMESSAGEWIDGET_H
