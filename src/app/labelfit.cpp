/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelfit.h"

#include "alteclabels.h"
#include "units.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace LabelFit
{

Problem check(const LabelDocument &document, const QList<PaperSize> &offered, bool acceptsCustomSizes, std::optional<qreal> loadedLengthMm)
{
    // Rows of labels and continuous rolls print at sizes of their own.
    if ((document.roll.across > 1 || document.roll.continuous) && !acceptsCustomSizes) {
        return Problem::RollNotSupported;
    }
    // A custom size is not expected in the list of a printer that takes
    // them; its length still counts. A DYMO takes none.
    const bool custom = document.id.startsWith(u"Custom."_s) && acceptsCustomSizes;
    if (!custom && !offered.isEmpty() && !LabelPrinter::matchPaper(document, offered)) {
        return Problem::NotOffered;
    }
    // The length the printer measured between gaps means nothing on a
    // continuous roll.
    if (loadedLengthMm && !document.roll.continuous && !AltecLabels::fitsLoaded(Units::twipsToMm(document.paperSize().height()), *loadedLengthMm)) {
        return Problem::WrongLength;
    }
    return Problem::None;
}

PaperSize startingPaper(const QList<PaperSize> &offered, const QString &lastPaper, std::optional<qreal> loadedLengthMm, const PaperSize &fallback)
{
    for (const PaperSize &candidate : offered) {
        if (candidate.name == lastPaper) {
            return candidate;
        }
    }
    if (loadedLengthMm) {
        for (const PaperSize &candidate : offered) {
            if (AltecLabels::fitsLoaded(candidate.size.height() * Units::MmPerInch / 72.0, *loadedLengthMm)) {
                return candidate;
            }
        }
    }
    for (const PaperSize &candidate : offered) {
        if (candidate.name == fallback.name) {
            return candidate;
        }
    }
    // Tape: 12 mm, the width a LabelManager comes with and most use.
    for (const PaperSize &candidate : offered) {
        if (candidate.tape == 12) {
            return candidate;
        }
    }
    return offered.isEmpty() ? fallback : offered.constFirst();
}

} // namespace LabelFit
