/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QtGlobal>

/**
 * Unit conversions. DYMO label files measure everything in twips
 * (1/1440 inch), so the label model stores twips and converts at the edges.
 */
namespace LabelCreator::Units
{

inline constexpr qreal TwipsPerInch = 1440.0;
inline constexpr qreal TwipsPerPoint = 20.0;
inline constexpr qreal MmPerInch = 25.4;

constexpr qreal twipsToMm(qreal twips)
{
    return twips / TwipsPerInch * MmPerInch;
}

constexpr qreal mmToTwips(qreal mm)
{
    return mm / MmPerInch * TwipsPerInch;
}

constexpr qreal twipsToPoints(qreal twips)
{
    return twips / TwipsPerPoint;
}

constexpr qreal pointsToTwips(qreal points)
{
    return points * TwipsPerPoint;
}

constexpr qreal twipsToDots(qreal twips, qreal dpi)
{
    return twips / TwipsPerInch * dpi;
}

} // namespace LabelCreator::Units
