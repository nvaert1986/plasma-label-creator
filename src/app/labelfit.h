/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelprinter.h"

#include <optional>

/**
 * Whether a label suits a printer, and which label type a new label starts
 * with. The rules behind the warning before printing and the label type
 * following the printer.
 */
namespace LabelFit
{

enum class Problem {
    None,
    /** Not one of the printer's label types (a DYMO size on an Altec, an Altec size on a DYMO). */
    NotOffered,
    /** Not as long as the labels the printer said are loaded. */
    WrongLength,
    /** Labels across or a continuous roll, on a printer that takes only its own label sizes. */
    RollNotSupported,
};

/**
 * What is wrong with printing @p document on a printer offering @p offered,
 * that does (or not) take custom sizes, with labels @p loadedLengthMm long
 * loaded if it said so.
 */
Problem check(const LabelCreator::LabelDocument &document,
              const QList<LabelCreator::PaperSize> &offered,
              bool acceptsCustomSizes,
              std::optional<qreal> loadedLengthMm);

/**
 * The label type to start with: @p lastPaper if offered, else one that fits
 * the loaded labels, else @p fallback if offered, else the first offered,
 * else @p fallback.
 */
LabelCreator::PaperSize startingPaper(const QList<LabelCreator::PaperSize> &offered,
                                      const QString &lastPaper,
                                      std::optional<qreal> loadedLengthMm,
                                      const LabelCreator::PaperSize &fallback);

} // namespace LabelFit
