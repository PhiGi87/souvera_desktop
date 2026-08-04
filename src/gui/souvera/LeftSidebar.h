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

struct SidebarItem {
    QString icon;
    QString label;
};

class LeftSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit LeftSidebar(QWidget *parent = nullptr);

    void addItem(const QString &icon, const QString &label);
    void setCurrentIndex(int index);
    [[nodiscard]] int currentIndex() const { return _currentIndex; }

signals:
    void currentChanged(int index);

private:
    QVBoxLayout *_topLayout = nullptr;
    QVBoxLayout *_bottomLayout = nullptr;
    QVector<QPushButton *> _buttons;
    int _currentIndex = 0;
};

} // namespace OCC

#endif // LEFTSIDEBAR_H
