/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STATUSHEADER_H
#define STATUSHEADER_H

#include <QWidget>

class QLabel;
class QPushButton;

namespace OCC {

class StatusHeader : public QWidget
{
    Q_OBJECT
public:
    explicit StatusHeader(QWidget *parent = nullptr);

signals:
    void settingsClicked();

private:
    void updateSyncStatus();

    QLabel *_userLabel = nullptr;
    QLabel *_syncIconLabel = nullptr;
    QLabel *_syncTextLabel = nullptr;
    QPushButton *_settingsButton = nullptr;
};

} // namespace OCC

#endif // STATUSHEADER_H
