/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EMOJIPICKER_H
#define EMOJIPICKER_H

#include <QFrame>
#include <QStringList>

class QGridLayout;
class QLineEdit;

namespace OCC {

/**
 * @brief Lightweight emoji picker popup for the Link chat composer.
 *
 * Shows a fixed set of commonly used emojis in categories; clicking one
 * emits emojiClicked() and closes the popup.
 */
class EmojiPicker : public QFrame
{
    Q_OBJECT
public:
    explicit EmojiPicker(QWidget *parent = nullptr);

signals:
    void emojiClicked(const QString &emoji);

protected:
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

private:
    void addCategory(const QString &title, const QStringList &emojis, int &row);

    QGridLayout *_grid = nullptr;
    int _row = 0;
};

} // namespace OCC

#endif // EMOJIPICKER_H
