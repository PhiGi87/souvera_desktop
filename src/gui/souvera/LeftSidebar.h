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

signals:
    void currentChanged(int index);

private:
    void setupAvatar();
    void setupThemeToggle();
    void updateThemeToggleIcon();
    void applyIconTints();

    struct SidebarButton {
        QPushButton *button = nullptr;
        QLabel *icon = nullptr;
        QLabel *text = nullptr;
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
