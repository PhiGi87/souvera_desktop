/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BOTTOMBAR_H
#define BOTTOMBAR_H

#include <QWidget>
#include <QVector>

class QPushButton;

namespace OCC {

class BottomBar : public QWidget
{
    Q_OBJECT
public:
    explicit BottomBar(QWidget *parent = nullptr);

    void setCurrentIndex(int index);

signals:
    void currentChanged(int index);

private:
    QVector<QPushButton *> _buttons;
};

} // namespace OCC

#endif // BOTTOMBAR_H
