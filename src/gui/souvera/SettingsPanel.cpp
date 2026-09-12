/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SettingsPanel.h"
#include "folderwizard.h"
#include "notifications/NotificationService.h"
#include "generalsettings.h"
#include "networksettings.h"

#include "mail/JmapClient.h"
#include "mail/MailLoginFlow.h"
#include "account.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"
#include "folderman.h"
#include "folder.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"
#include "common/vfs.h"
#include "common/utility.h"
#include <QMessageBox>
#include <QUuid>
#include "theme.h"
#include "theme/SouveraMetrics.h"
#include "theme/SouveraTheme.h"

#include <QDateTime>
#include <QSysInfo>

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLoggingCategory>
#include <QCheckBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcSettingsPanel, "souvera.settings.panel")

namespace OCC {

namespace Metrics = Sou::Metrics;

namespace {

// Settings categories, in display order. The index doubles as the page index
// in the QStackedWidget and the row index in the navigation list.
enum Category {
    CatAccount,
    CatSync,
    CatAppearance,
    CatMail,
    CatNotifications,
    CatNetwork,
    CatAdvanced,
    CatDiagnostics,
    CatCount
};

QFrame *makeCard(const QString &title, QWidget *parent, QVBoxLayout **outLayout)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("FolderRow"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(Metrics::CardPaddingH, Metrics::CardPaddingV,
                               Metrics::CardPaddingH, Metrics::CardPaddingV);
    layout->setSpacing(Metrics::CardSpacing);

    auto *label = new QLabel(title, card);
    label->setObjectName(QStringLiteral("SettingsCardTitle"));
    layout->addWidget(label);

    if (outLayout) {
        *outLayout = layout;
    }
    return card;
}

QWidget *cardRow(QWidget *parent, QHBoxLayout **outLayout)
{
    auto *row = new QWidget(parent);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(Metrics::SpacingS);
    if (outLayout) {
        *outLayout = rowLayout;
    }
    return row;
}

} // namespace

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FilesPanel"));
    setupUi();

    connect(FolderMan::instance(), &FolderMan::folderListChanged,
            this, [this](const Folder::Map &) { rebuildSyncFolders(); });
    connect(FolderMan::instance(), &FolderMan::folderSyncStateChange,
            this, [this](Folder *) { rebuildSyncFolders(); });
}

void SettingsPanel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) return;
    if (_accountState) {
        disconnect(_accountState, &AccountState::stateChanged, this, nullptr);
    }
    _accountState = accountState;
    ensureNetworkSettings(accountState);

    if (!accountState) {
        _accountLabel->setText(QStringLiteral("Kein Konto verbunden"));
        _serverLabel->clear();
        _connectionLabel->clear();
        return;
    }

    const auto acc = accountState->account();
    const auto creds = acc ? acc->credentials() : nullptr;
    const auto user = creds ? creds->user() : QString();
    const auto host = acc ? acc->url().host() : QString();
    // Email logins already carry their domain — never append the host twice.
    _accountLabel->setText(user.isEmpty()
        ? host
        : (user.contains(QLatin1Char('@')) ? user : QStringLiteral("%1@%2").arg(user, host)));
    _serverLabel->setText(acc ? acc->url().toString() : QString());

    // Re-entrancy guard: drop any previous connection for this sender first.
    disconnect(accountState, &AccountState::stateChanged, this, nullptr);
    connect(accountState, &AccountState::stateChanged, this, [this](AccountState::State state) {
        switch (state) {
        case AccountState::State::Connected:
            _connectionLabel->setText(QStringLiteral("Verbunden"));
            break;
        case AccountState::State::Disconnected:
            _connectionLabel->setText(QStringLiteral("Getrennt"));
            break;
        case AccountState::State::SignedOut:
            _connectionLabel->setText(QStringLiteral("Abgemeldet"));
            break;
        default:
            _connectionLabel->setText(QStringLiteral("Verbinde\u2026"));
            break;
        }
    });
}

void SettingsPanel::setupUi()
{
    setObjectName(QStringLiteral("SettingsPanel"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PanelToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(Metrics::CardMargin, 8, Metrics::CardMargin, 8);
    auto *title = new QLabel(QStringLiteral("Einstellungen"), toolbar);
    title->setObjectName(QStringLiteral("PanelTitle"));
    toolbarLayout->addWidget(title);
    toolbarLayout->addStretch();
    layout->addWidget(toolbar);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    _nav = new QListWidget(body);
    _nav->setObjectName(QStringLiteral("SettingsNav"));
    _nav->setFixedWidth(Metrics::NavWidth);
    _nav->setUniformItemSizes(true);
    _nav->setIconSize(QSize(20, 20));
    _nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    _pages = new QStackedWidget(body);
    _pages->setObjectName(QStringLiteral("SettingsPages"));

    bodyLayout->addWidget(_nav);
    bodyLayout->addWidget(_pages, 1);
    layout->addWidget(body, 1);

    static const struct {
        const char *label;
        const char *icon;
    } categories[CatCount] = {
        {"Konto", "user"},
        {"Synchronisation", "folder"},
        {"Erscheinungsbild", "palette"},
        {"Mail", "mail"},
        {"Benachrichtigungen", "bell"},
        {"Netzwerk", "globe"},
        {"Erweitert", "settings"},
        {"Diagnose", "activity"},
    };
    for (int i = 0; i < CatCount; ++i) {
        _nav->addItem(new QListWidgetItem(
            QIcon(), QLatin1String(categories[i].label)));
        _navIconNames.append(QLatin1String(categories[i].icon));
        _pages->addWidget(createPage(i));
    }
    refreshNavIcons();

    connect(_nav, &QListWidget::currentRowChanged, _pages, &QStackedWidget::setCurrentIndex);
    connect(_nav, &QListWidget::currentRowChanged, this, [this](int) { refreshNavIcons(); });
    connect(SouveraTheme::instance(), &SouveraTheme::themeChanged, this, &SettingsPanel::refreshNavIcons);
    _nav->setCurrentRow(0);
    rebuildSyncFolders();
}

QScrollArea *SettingsPanel::createPage(int category)
{
    auto *scroll = new QScrollArea(_pages);
    scroll->setObjectName(QStringLiteral("PanelScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("ContentArea"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(Metrics::CardMargin, Metrics::CardMargin,
                                      Metrics::CardMargin, Metrics::CardMargin);
    contentLayout->setSpacing(Metrics::SpacingM);

    switch (category) {
    case CatAccount: {
        QVBoxLayout *accountLayout = nullptr;
        auto *accountCard = makeCard(QStringLiteral("Konto"), content, &accountLayout);
        _accountLabel = new QLabel(QStringLiteral("Kein Konto verbunden"), accountCard);
        _accountLabel->setObjectName(QStringLiteral("FolderAliasLabel"));
        accountLayout->addWidget(_accountLabel);
        _serverLabel = new QLabel(accountCard);
        _serverLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
        accountLayout->addWidget(_serverLabel);
        _connectionLabel = new QLabel(accountCard);
        _connectionLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
        accountLayout->addWidget(_connectionLabel);
        contentLayout->addWidget(accountCard);
        contentLayout->addStretch();
        break;
    }

    case CatSync: {
        QVBoxLayout *syncLayout = nullptr;
        auto *syncCard = makeCard(QStringLiteral("Datei-Synchronisation"), content, &syncLayout);
        auto *syncHint = new QLabel(
            QStringLiteral("Synchronisierte Ordner laufen automatisch im Hintergrund weiter."),
            syncCard);
        syncHint->setObjectName(QStringLiteral("FolderStatusLabel"));
        syncHint->setWordWrap(true);
        syncLayout->addWidget(syncHint);

        _syncFolderContainer = new QWidget(syncCard);
        auto *folderLayout = new QVBoxLayout(_syncFolderContainer);
        folderLayout->setContentsMargins(0, 0, 0, 0);
        folderLayout->setSpacing(Metrics::SpacingS);
        syncLayout->addWidget(_syncFolderContainer);

        QHBoxLayout *addFolderLayout = nullptr;
        auto *addFolderRow = cardRow(syncCard, &addFolderLayout);
        addFolderLayout->addStretch();
        auto *addFolderBtn = new QPushButton(QStringLiteral("Ordner hinzufügen..."), addFolderRow);
        addFolderBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
        connect(addFolderBtn, &QPushButton::clicked, this, &SettingsPanel::onAddFolder);
        addFolderLayout->addWidget(addFolderBtn);
        syncLayout->addWidget(addFolderRow);
        contentLayout->addWidget(syncCard);
        contentLayout->addStretch();
        break;
    }

    case CatAppearance: {
        QVBoxLayout *themeLayout = nullptr;
        auto *themeCard = makeCard(QStringLiteral("Erscheinungsbild"), content, &themeLayout);
        QHBoxLayout *themeRowLayout = nullptr;
        auto *themeRow = cardRow(themeCard, &themeRowLayout);
        auto *themeLabel = new QLabel(QStringLiteral("Dunkles Design verwenden"), themeRow);
        themeLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
        themeRowLayout->addWidget(themeLabel);
        themeRowLayout->addStretch();
        auto *themeBtn = new QPushButton(QStringLiteral("Umschalten"), themeRow);
        themeBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
        connect(themeBtn, &QPushButton::clicked, this, []() {
            SouveraTheme::instance()->toggleTheme();
        });
        themeRowLayout->addWidget(themeBtn);
        themeLayout->addWidget(themeRow);
        contentLayout->addWidget(themeCard);
        contentLayout->addStretch();
        break;
    }

    case CatMail: {
        QVBoxLayout *mailViewLayout = nullptr;
        auto *mailViewCard = makeCard(QStringLiteral("Nachrichtenliste"), content, &mailViewLayout);

        _verticalLayoutCheck = new QCheckBox(QStringLiteral("Nachrichten untereinander anzeigen (vertikale Ansicht)"), mailViewCard);
        _previewLinesCheck = new QCheckBox(QStringLiteral("Vorschauzeilen in der Nachrichtenliste zeigen"), mailViewCard);
        mailViewLayout->addWidget(_verticalLayoutCheck);
        mailViewLayout->addWidget(_previewLinesCheck);

        {
            QSettings settings;
            settings.beginGroup(QStringLiteral("souvera/mailview"));
            _verticalLayoutCheck->setChecked(settings.value(QStringLiteral("verticalLayout"), false).toBool());
            _previewLinesCheck->setChecked(settings.value(QStringLiteral("previewLines"), true).toBool());
            settings.endGroup();
        }

        connect(_verticalLayoutCheck, &QCheckBox::toggled, this, [this](bool checked) {
            QSettings settings;
            settings.beginGroup(QStringLiteral("souvera/mailview"));
            settings.setValue(QStringLiteral("verticalLayout"), checked);
            settings.endGroup();
            emit viewSettingsChanged();
        });
        connect(_previewLinesCheck, &QCheckBox::toggled, this, [this](bool checked) {
            QSettings settings;
            settings.beginGroup(QStringLiteral("souvera/mailview"));
            settings.setValue(QStringLiteral("previewLines"), checked);
            settings.endGroup();
            emit viewSettingsChanged();
        });
        contentLayout->addWidget(mailViewCard);

        QVBoxLayout *mailTestLayout = nullptr;
        auto *mailTestCard = makeCard(QStringLiteral("Mail-Verbindung"), content, &mailTestLayout);
        QHBoxLayout *mailRowLayout = nullptr;
        auto *mailRow = cardRow(mailTestCard, &mailRowLayout);
        _mailTestBtn = new QPushButton(QStringLiteral("Verbindung testen"), mailRow);
        _mailTestBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
        connect(_mailTestBtn, &QPushButton::clicked, this, &SettingsPanel::onTestMail);
        mailRowLayout->addWidget(_mailTestBtn);
        mailRowLayout->addStretch();
        _mailTestResult = new QLabel(mailRow);
        _mailTestResult->setObjectName(QStringLiteral("FolderStatusLabel"));
        _mailTestResult->setWordWrap(true);
        mailRowLayout->addWidget(_mailTestResult, 1);
        mailTestLayout->addWidget(mailRow);
        contentLayout->addWidget(mailTestCard);
        contentLayout->addStretch();
        break;
    }

    case CatNotifications: {
        QVBoxLayout *notifLayout = nullptr;
        auto *notifCard = makeCard(QStringLiteral("Benachrichtigungen"), content, &notifLayout);

        auto *masterCheck = new QCheckBox(
            QStringLiteral("Desktop-Benachrichtigungen aktivieren"), notifCard);
        masterCheck->setChecked(NotificationService::notificationsEnabled());
        notifLayout->addWidget(masterCheck);

        auto *mailCheck = new QCheckBox(
            QStringLiteral("Bei neuen E-Mails benachrichtigen"), notifCard);
        mailCheck->setChecked(NotificationService::mailNotificationsEnabled());
        notifLayout->addWidget(mailCheck);

        auto *notifHint = new QLabel(
            QStringLiteral("Benachrichtigungen werden regelm\u00E4\u00DFig vom Server abgerufen "
                           "(Nextcloud-Benachrichtigungen und Posteingang)."), notifCard);
        notifHint->setObjectName(QStringLiteral("FolderStatusLabel"));
        notifHint->setWordWrap(true);
        notifLayout->addWidget(notifHint);

        connect(masterCheck, &QCheckBox::toggled, this, [](bool checked) {
            NotificationService::setNotificationsEnabled(checked);
        });
        connect(mailCheck, &QCheckBox::toggled, this, [](bool checked) {
            NotificationService::setMailNotificationsEnabled(checked);
        });
        contentLayout->addWidget(notifCard);
        contentLayout->addStretch();
        break;
    }

    case CatNetwork: {
        // The widget needs a valid Account; it is created in setAccountState().
        QVBoxLayout *networkLayout = nullptr;
        auto *networkCard = makeCard(QStringLiteral("Netzwerk"), content, &networkLayout);
        _networkCardLayout = networkLayout;
        contentLayout->addWidget(networkCard);
        contentLayout->addStretch();
        break;
    }

    case CatAdvanced: {
        QVBoxLayout *generalLayout = nullptr;
        auto *generalCard = makeCard(QStringLiteral("Allgemein"), content, &generalLayout);
        auto *generalSettings = new GeneralSettings(generalCard);
        generalSettings->setContentsMargins(0, 0, 0, 0);
        generalLayout->addWidget(generalSettings);
        contentLayout->addWidget(generalCard);
        contentLayout->addStretch();
        break;
    }

    case CatDiagnostics: {
        QVBoxLayout *diagLayout = nullptr;
        auto *diagCard = makeCard(QStringLiteral("Diagnose"), content, &diagLayout);

        auto *versionLabel = new QLabel(
            QStringLiteral("Souvera Workspace %1").arg(Theme::instance()->version()), diagCard);
        versionLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
        diagLayout->addWidget(versionLabel);

#ifdef Q_OS_WIN
        const auto logHint = QStringLiteral("Debug-Log: %LOCALAPPDATA%\\Souvera\\startup.log");
#else
        const auto logHint = QStringLiteral("Debug-Log: ~/.local/share/Souvera/startup.log");
#endif
        auto *logPathLabel = new QLabel(logHint, diagCard);
        logPathLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
        logPathLabel->setWordWrap(true);
        diagLayout->addWidget(logPathLabel);

        auto *logBtn = new QPushButton(QStringLiteral("Debug-Log \u00F6ffnen"), diagCard);
        logBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
        connect(logBtn, &QPushButton::clicked, this, []() {
#ifdef Q_OS_WIN
            const auto base = qEnvironmentVariable("LOCALAPPDATA");
            const auto logPath = base + QStringLiteral("\\Souvera\\startup.log");
#else
            const auto logPath = QDir::homePath() + QStringLiteral("/.local/share/Souvera/startup.log");
#endif
            QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
        });
        diagLayout->addWidget(logBtn);

        auto *exportBtn = new QPushButton(QStringLiteral("Diagnose-Bericht exportieren"), diagCard);
        exportBtn->setObjectName(QStringLiteral("PanelPrimaryBtn"));
        connect(exportBtn, &QPushButton::clicked, this, &SettingsPanel::onExportDiagnostics);
        diagLayout->addWidget(exportBtn);
        contentLayout->addWidget(diagCard);
        contentLayout->addStretch();
        break;
    }

    default:
        break;
    }

    scroll->setWidget(content);
    return scroll;
}

void SettingsPanel::refreshNavIcons()
{
    if (!_nav) return;
    const auto *theme = SouveraTheme::instance();
    for (int i = 0; i < _nav->count() && i < _navIconNames.size(); ++i) {
        const auto tint = i == _nav->currentRow()
            ? SouveraTheme::Color::Accent
            : SouveraTheme::Color::TextSecondary;
        _nav->item(i)->setIcon(theme->icon(_navIconNames.at(i), tint));
    }
}

void SettingsPanel::ensureNetworkSettings(AccountState *accountState)
{
    if (_networkSettingsWidget || !_networkCardLayout) return;
    if (!accountState || !accountState->account()) return;

    _networkSettingsWidget = new NetworkSettings(accountState->account(), this);
    _networkSettingsWidget->setContentsMargins(0, 0, 0, 0);
    _networkCardLayout->addWidget(_networkSettingsWidget);
}

void SettingsPanel::rebuildSyncFolders()
{
    if (!_syncFolderContainer || !FolderMan::instance()) return;
    auto *containerLayout = qobject_cast<QVBoxLayout *>(_syncFolderContainer->layout());
    if (!containerLayout) return;

    while (auto *item = containerLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const auto folders = FolderMan::instance()->map();
    if (folders.isEmpty()) {
        auto *label = new QLabel(QStringLiteral("Keine Synchronisationsordner eingerichtet."), _syncFolderContainer);
        label->setObjectName(QStringLiteral("EmptyLabel"));
        containerLayout->addWidget(label);
        return;
    }

    for (auto *folder : folders) {
        addFolderRow(folder, containerLayout);
    }
}

void SettingsPanel::addFolderRow(Folder *folder, QVBoxLayout *container)
{
    auto *row = new QWidget(_syncFolderContainer);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto *nameLabel = new QLabel(folder->shortGuiLocalPath(), row);
    nameLabel->setObjectName(QStringLiteral("FolderAliasLabel"));
    rowLayout->addWidget(nameLabel);

    auto *pathLabel = new QLabel(folder->path(), row);
    pathLabel->setObjectName(QStringLiteral("FolderStatusLabel"));
    rowLayout->addWidget(pathLabel, 1);

    auto *pauseBtn = new QPushButton(folder->syncPaused()
                                         ? QStringLiteral("Fortsetzen")
                                         : QStringLiteral("Pausieren"), row);
    pauseBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(pauseBtn, &QPushButton::clicked, this, [this, folder]() {
        onTogglePause(folder);
    });
    rowLayout->addWidget(pauseBtn);

    auto *openBtn = new QPushButton(QStringLiteral("\u00D6ffnen"), row);
    openBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(openBtn, &QPushButton::clicked, this, [folder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder->path()));
    });
    rowLayout->addWidget(openBtn);

    auto *removeBtn = new QPushButton(QStringLiteral("Entfernen"), row);
    removeBtn->setObjectName(QStringLiteral("PanelSecondaryBtn"));
    connect(removeBtn, &QPushButton::clicked, this, [this, folder]() {
        onRemoveFolder(folder);
    });
    rowLayout->addWidget(removeBtn);

    container->addWidget(row);
}

void SettingsPanel::onTogglePause(Folder *folder)
{
    if (!folder) return;
    folder->setSyncPaused(!folder->syncPaused());
    rebuildSyncFolders();
}

void SettingsPanel::onAddFolder()
{
    if (!_accountState || !_accountState->account()) return;
    auto *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false); // do not start more syncs while the wizard runs.

    auto *folderWizard = new FolderWizard(_accountState->account(), this);
    folderWizard->setAttribute(Qt::WA_DeleteOnClose);

    connect(folderWizard, &QDialog::accepted, this, [this, folderWizard]() {
        auto *folderMan = FolderMan::instance();
        FolderDefinition definition;
        definition.localPath = FolderDefinition::prepareLocalPath(
            folderWizard->field(QLatin1String("sourceFolder")).toString());
        definition.targetPath = FolderDefinition::prepareTargetPath(
            folderWizard->property("targetPath").toString());

        if (folderWizard->property("useVirtualFiles").toBool()) {
            definition.virtualFilesMode = bestAvailableVfsMode();
        }

        QDir dir(definition.localPath);
        if (!dir.exists()) {
            if (!dir.mkpath(QStringLiteral("."))) {
                QMessageBox::warning(this, QStringLiteral("Ordner konnte nicht erstellt werden"),
                    QStringLiteral("<p>Der lokale Ordner <i>%1</i> konnte nicht erstellt werden.</p>")
                        .arg(QDir::toNativeSeparators(definition.localPath)));
                folderMan->setSyncEnabled(true);
                return;
            }
        }
        FileSystem::setFolderMinimumPermissions(definition.localPath);
        Utility::setupFavLink(definition.localPath);

        definition.ignoreHiddenFiles = folderMan->ignoreHiddenFiles();

        folderMan->setSyncEnabled(true);
        const auto folder = folderMan->addFolder(_accountState, definition);
        if (folder) {
            if (definition.virtualFilesMode != Vfs::Off && folderWizard->property("useVirtualFiles").toBool()) {
                folder->setRootPinState(PinState::OnlineOnly);
            }
            const auto selectiveSyncBlackList =
                folderWizard->property("selectiveSyncBlackList").toStringList();
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                selectiveSyncBlackList);
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
                QStringList() << QLatin1String("/"));
            folderMan->scheduleAllFolders();
        }
        rebuildSyncFolders();
    });

    connect(folderWizard, &QDialog::rejected, folderMan, [folderMan]() {
        folderMan->setSyncEnabled(true);
    });
    folderWizard->open();
}

void SettingsPanel::onRemoveFolder(Folder *folder)
{
    if (!folder) return;
    const auto ret = QMessageBox::question(
        this, QStringLiteral("Ordner entfernen"),
        QStringLiteral("Soll der Synchronisationsordner \u201E%1\u201C entfernt werden? "
                       "Die lokalen Dateien bleiben erhalten.")
            .arg(folder->shortGuiLocalPath()));
    if (ret != QMessageBox::Yes) return;
    FolderMan::instance()->removeFolder(folder);
    rebuildSyncFolders();
}

void SettingsPanel::onTestMail()
{
    if (!_accountState) {
        _mailTestResult->setText(QStringLiteral("Kein Konto verbunden."));
        return;
    }

    const auto acc = _accountState->account();
    const auto creds = acc ? acc->credentials() : nullptr;
    if (!acc || !creds) {
        _mailTestResult->setText(QStringLiteral("Konto nicht bereit."));
        return;
    }

    _mailTestResult->setText(QStringLiteral("Teste\u2026"));
    _mailTestBtn->setEnabled(false);

    // The JMAP server only accepts the combined Nextcloud+Stalwart password
    // minted via souvera_mail - the plain app password X returns 401.
    const auto accountGuard = QPointer<AccountState>(_accountState);
    MailLoginFlow::ensureCombinedPassword(_accountState,
        [this, accountGuard](const CombinedAppPassword &result) {
            if (!accountGuard || accountGuard != _accountState) return;
            const auto user = accountGuard->account()
                && accountGuard->account()->credentials()
                    ? accountGuard->account()->credentials()->user() : QString();

            if (_testClient) {
                _testClient->deleteLater();
                _testClient = nullptr;
            }
            _testClient = new JmapClient(accountGuard, this);
            _testClient->setCredentials(user, result.appPassword);

            connect(_testClient, &JmapClient::sessionResolved, this, [this](const QString &accountId, const QString &) {
                _mailTestResult->setText(QStringLiteral("OK \u2014 Postfach %1 erreichbar.").arg(accountId));
                _mailTestBtn->setEnabled(true);
            });
            connect(_testClient, &JmapClient::sessionError, this, [this](const QString &error) {
                _mailTestResult->setText(error);
                _mailTestBtn->setEnabled(true);
            });

            _testClient->resolveSession();
        },
        [this, accountGuard](const QString &error) {
            if (!accountGuard) return;
            _mailTestResult->setText(error);
            _mailTestBtn->setEnabled(true);
        });
}

void SettingsPanel::onExportDiagnostics()
{
    QString davUser;
    QString credUser;
    QString credLen;
    if (_accountState && _accountState->account()) {
        const auto acc = _accountState->account();
        davUser = acc->davUser();
        if (acc->credentials()) {
            credUser = acc->credentials()->user();
            credLen = QString::number(acc->credentials()->password().size());
        }
    }

    const auto reports = QStringList{
        QStringLiteral("Souvera Workspace Diagnose-Bericht"),
        QStringLiteral("=============================="),
        QString(),
        QStringLiteral("Client-Version: %1").arg(Theme::instance()->version()),
        QStringLiteral("Qt-Version: %1").arg(QString::fromLatin1(qVersion())),
        QStringLiteral("Betriebssystem: %1").arg(QSysInfo::prettyProductName()),
        QString(),
        QStringLiteral("Konto: %1").arg(_accountLabel->text()),
        QStringLiteral("Server: %1").arg(_serverLabel->text()),
        QStringLiteral("Verbindung: %1").arg(_connectionLabel->text()),
        QStringLiteral("DAV-User: %1").arg(davUser),
        QStringLiteral("Credential-User: %1").arg(credUser),
        QStringLiteral("Passwort-L\u00E4nge: %1").arg(credLen),
        QString(),
        QStringLiteral("Mail-Test: %1").arg(_mailTestResult->text()),
        QString(),
        QStringLiteral("Synchronisationsordner:"),
    };

    QStringList folderLines;
    const auto folders = FolderMan::instance()->map();
    for (const auto *folder : folders) {
        folderLines << QStringLiteral("  - %1 (%2) %3")
            .arg(folder->shortGuiLocalPath(), folder->path(),
                 folder->syncPaused() ? QStringLiteral("pausiert")
                                      : QStringLiteral("aktiv"));
    }
    if (folderLines.isEmpty()) {
        folderLines << QStringLiteral("  (keine)");
    }

    const auto defaultName = QStringLiteral("souvera-diagnose-%1.txt")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm")));

    const auto target = QFileDialog::getSaveFileName(this, QStringLiteral("Diagnose-Bericht speichern"),
                                                     defaultName);
    if (target.isEmpty()) return;

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    file.write((reports + folderLines).join(QLatin1Char('\n')).toUtf8());
    file.commit();
}

} // namespace OCC
