/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LEFTSIDEBAR_H
#define LEFTSIDEBAR_H

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>

namespace OCC {

/**
 * @brief Vertical left sidebar navigation replacing the bottom tab bar.
 *
 * Shows icon + text for each panel. Emits currentChanged(int) when clicked.
 */
class LeftSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit LeftSidebar(QWidget *parent = nullptr);

    void addItem(const QString &icon, const QString &label);
    void setCurrentIndex(int index);

signals:
    void currentChanged(int index);

private:
    QListWidget *_list;
};

} // namespace OCC

#endif // LEFTSIDEBAR_H
