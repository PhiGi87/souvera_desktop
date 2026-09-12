/*
 * SPDX-FileCopyrightText: 2026 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SOUVERAMETRICS_H
#define SOUVERAMETRICS_H

namespace OCC::Sou::Metrics {

/**
 * @brief Shared layout constants for Souvera panels.
 *
 * Centralizes the spacing scale so panels stay visually consistent without
 * scattering magic numbers through widget code. The scale mirrors the 8dp
 * grid documented in souvera.qss.
 */
constexpr int SpacingXS = 4;
constexpr int SpacingS = 8;
constexpr int SpacingM = 12;
constexpr int SpacingL = 16;
constexpr int SpacingXL = 24;
constexpr int SpacingXXL = 32;

constexpr int CardMargin = SpacingL; //!< outer margin of a content page
constexpr int CardPaddingH = SpacingL; //!< horizontal padding inside a card
constexpr int CardPaddingV = 14; //!< vertical padding inside a card
constexpr int CardRadius = 10; //!< card corner radius, matches QSS #FolderRow
constexpr int CardSpacing = 10; //!< vertical spacing between rows in a card

constexpr int ToolbarHeight = 44; //!< panel toolbar min height, matches QSS
constexpr int NavWidth = 220; //!< settings category navigation width

} // namespace OCC::Sou::Metrics

#endif // SOUVERAMETRICS_H
