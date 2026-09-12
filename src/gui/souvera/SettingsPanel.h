/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QScrollArea;
class QListWidget;
class QStackedWidget;

class QCheckBox;

namespace OCC {

class AccountState;
class Folder;
class JmapClient;
class NetworkSettings;

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    void setAccountState(AccountState *accountState);

signals:
    void viewSettingsChanged();

private:
    void setupUi();
    QScrollArea *createPage(int category);
    void refreshNavIcons();
    void rebuildSyncFolders();
    void addFolderRow(Folder *folder, QVBoxLayout *container);
    void onTogglePause(Folder *folder);
    void onAddFolder();
    void onRemoveFolder(Folder *folder);
    void ensureNetworkSettings(AccountState *accountState);

    NetworkSettings *_networkSettingsWidget = nullptr;
    QVBoxLayout *_networkCardLayout = nullptr;
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
    QCheckBox *_verticalLayoutCheck = nullptr;
    QCheckBox *_previewLinesCheck = nullptr;
    QListWidget *_nav = nullptr;
    QStackedWidget *_pages = nullptr;
    QStringList _navIconNames;
};

} // namespace OCC

#endif // SETTINGSPANEL_H
