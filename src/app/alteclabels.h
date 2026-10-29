/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelprinter.h"

/**
 * The labels Altec sells for its ATP printers, as listed on altec.nl for
 * the ATP-600 Pro (2026-10-29): die-cut and round labels, grouped by
 * product. Continuous rolls are left out until the editor can design
 * labels of a chosen length.
 */
namespace AltecLabels
{

/** Every label, as custom sizes grouped by product. */
QList<LabelCreator::PaperSize> all();

/** Whether @p text (a model, queue name or description) names an Altec ATP printer. */
bool isAltec(const QString &text);

/**
 * Whether a label @p labelLengthMm long along the feed fits the length the
 * printer measured, @p loadedLengthMm, which may include up to 3 mm of gap.
 */
bool fitsLoaded(qreal labelLengthMm, qreal loadedLengthMm);

/**
 * The labels from @p labels that fit the measured @p loadedLengthMm. Not by
 * width: the printer's print width is whatever the last job set.
 */
QList<LabelCreator::PaperSize> fitting(const QList<LabelCreator::PaperSize> &labels, qreal loadedLengthMm);

} // namespace AltecLabels
