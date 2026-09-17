/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INCOMINGCALLDIALOG_H
#define INCOMINGCALLDIALOG_H

#include <QDialog>

class QLabel;
class QTimer;
class QAudioSink;
class QIODevice;

namespace OCC {

/**
 * @brief Native incoming call dialog with ring tone, accept and decline.
 *
 * Shown when a Talk call notification arrives from the server. Plays a
 * repeating ring tone until the user accepts or declines. On accept the
 * accepted(roomToken) signal is emitted — the caller is responsible for
 * switching to the Talk panel and joining the call.
 */
class IncomingCallDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IncomingCallDialog(const QString &roomToken, const QString &callerName,
                                QWidget *parent = nullptr);
    ~IncomingCallDialog() override;

signals:
    void accepted(const QString &roomToken);

private:
    void startRing();
    void stopRing();
    void closeEvent(QCloseEvent *event) override;

    QString _roomToken;
    QLabel *_callerLabel = nullptr;
    QPushButton *_acceptButton = nullptr;
    QPushButton *_declineButton = nullptr;
    QAudioSink *_ringSink = nullptr;
    qint64 _ringPhase = 0;
    QIODevice *_ringIo = nullptr;
    QTimer *_ringTimer = nullptr;
};

} // namespace OCC

#endif // INCOMINGCALLDIALOG_H
