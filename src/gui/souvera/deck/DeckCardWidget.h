/*
 * SPDX-FileCopyrightText: 2025 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DECKCARDWIDGET_H
#define DECKCARDWIDGET_H

#include <QFrame>

class QLabel;

namespace OCC {

class DeckCardWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DeckCardWidget(const QString &title, const QString &description, QWidget *parent = nullptr);
    ~DeckCardWidget() override = default;

    void setTitle(const QString &title);
    void setDescription(const QString &description);
    void addLabel(const QString &color, const QString &title);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *_titleLabel = nullptr;
    QLabel *_descriptionLabel = nullptr;
    QWidget *_labelsContainer = nullptr;
};

} // namespace OCC

#endif // DECKCARDWIDGET_H
