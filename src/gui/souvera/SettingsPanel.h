/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QScrollArea;

namespace OCC {

class AccountState;
class Folder;
class JmapClient;

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    void setAccountState(AccountState *accountState);

private:
    void setupUi();
    void rebuildSyncFolders();
    void addFolderRow(Folder *folder, QVBoxLayout *container);
    void onTogglePause(Folder *folder);
    void onExportDiagnostics();
    void onTestMail();

    AccountState *_accountState = nullptr;
    QLabel *_accountLabel = nullptr;
    QLabel *_serverLabel = nullptr;
    QLabel *_connectionLabel = nullptr;
    QLabel *_mailTestResult = nullptr;
    QPushButton *_mailTestBtn = nullptr;
    QWidget *_syncFolderContainer = nullptr;
    JmapClient *_testClient = nullptr;
};

} // namespace OCC

#endif // SETTINGSPANEL_H
