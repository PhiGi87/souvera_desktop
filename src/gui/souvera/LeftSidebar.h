/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LEFTSIDEBAR_H
#define LEFTSIDEBAR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QVector>

class QPushButton;
class QLabel;

namespace OCC {

class LeftSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit LeftSidebar(QWidget *parent = nullptr);

    void addItem(const QString &iconName, const QString &label);
    void setCurrentIndex(int index);
    [[nodiscard]] int currentIndex() const { return _currentIndex; }
    /** Shows a small unread counter on the button's icon; hidden when count is 0. */
    void setBadge(int index, int count);

signals:
    void currentChanged(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupAvatar();
    void setupThemeToggle();
    void updateThemeToggleIcon();
    void applyIconTints();
    void refreshBadgeStyles();
    void positionBadge(int index);

    struct SidebarButton {
        QPushButton *button = nullptr;
        QLabel *icon = nullptr;
        QLabel *text = nullptr;
        QLabel *badge = nullptr; //!< unread overlay, created lazily by setBadge()
        int badgeCount = 0;      //!< last count set on badge, drives theme restyle
        QString iconName;
    };

    QVBoxLayout *_topLayout = nullptr;
    QVBoxLayout *_bottomLayout = nullptr;
    QVector<SidebarButton> _buttons;
    QPushButton *_themeToggle = nullptr;
    int _currentIndex = 0;
};

} // namespace OCC

#endif // LEFTSIDEBAR_H
