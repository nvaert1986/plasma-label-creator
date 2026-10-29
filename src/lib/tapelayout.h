/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <optional>

namespace LabelCreator
{

/**
 * Tape labels for DYMO's LabelManager printers: the tapes they take, and
 * placing a tape label's cells and objects along the tape.
 */
namespace TapeLayout
{

/** A D1 tape as DYMO's LabelManager drivers know it. */
struct Tape {
    int millimetres; // nominal: 6, 9, 12, 19 or 24
    qreal width; // points across, as the driver's page sizes have it
    qreal printFrom; // points from the tape's edge where the print head starts
    qreal printTo; // and ends
    int mediaType; // the driver's tape code (low byte of cupsMediaType)
};

/** The tape of @p millimetres; nullopt for widths no LabelManager takes. */
std::optional<Tape> tape(int millimetres);
/** The tapes, narrowest first. */
QList<Tape> tapes();
/** The tape a DLS tape label names: Id "Tape12mm" or paper name "12mm". */
std::optional<int> millimetresOf(const QString &idOrPaperName);

/** A new, empty tape label on @p millimetres tape, as long as its content. */
LabelDocument newLabel(int millimetres);

/**
 * The band across the tape the print head reaches, in canvas twips; objects
 * are laid out within it. Empty for die-cut labels.
 */
QRectF printableBand(const LabelDocument &document);

/**
 * Places the cells and their objects: along the tape, cells as long as
 * their content (Auto), their length (Fixed) or a share of what a
 * fixed-length label has left (Star); across, within the printable band.
 * The label's length follows its content unless fixed. Nothing happens to
 * die-cut labels.
 */
void layout(LabelDocument &document);

} // namespace TapeLayout

} // namespace LabelCreator
