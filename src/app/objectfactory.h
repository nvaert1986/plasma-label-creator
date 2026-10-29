/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"
#include "labelprinter.h"

#include <QString>

#include <optional>

/** Creates new label objects with sensible defaults, the way DLS does. */
namespace ObjectFactory
{

enum class Kind {
    Text,
    Address,
    Barcode,
    Image,
    HorizontalLine,
    VerticalLine,
    Rectangle,
    Ellipse,
    Counter,
    DateTime,
    CurvedText,
};

/**
 * A new object of @p kind, placed on @p document. Images need their file
 * data in @p imageData.
 */
LabelCreator::LabelObject create(Kind kind, const LabelCreator::LabelDocument &document, const QByteArray &imageData = {});

/** A name not used by any object yet: TEXT, TEXT_1, TEXT_2... */
QString uniqueName(const LabelCreator::LabelDocument &document, const QString &base);
/** The name a copy of an object called @p name gets: TEXT_1 for TEXT, MY_TEXT_1 for MY_TEXT_3. */
QString copyName(const LabelCreator::LabelDocument &document, const QString &name);

/**
 * Centres @p object on @p centre (twips), as where it was dropped, but keeps
 * it on the label where it fits.
 */
void centreOn(LabelCreator::LabelObject &object, const LabelCreator::LabelDocument &document, const QPointF &centre);

/**
 * Moves @p object against an edge or to the middle of the label's usable
 * area, the part 1/10 inch in from the edges where new objects go too.
 * Horizontal and vertical flags of @p alignment are applied independently.
 */
void alignOnLabel(LabelCreator::LabelObject &object, const LabelCreator::LabelDocument &document, Qt::Alignment alignment);

/**
 * A new label of @p paper: a tape label for a LabelManager's tape types,
 * else a die-cut label of its size, oriented as DLS orients it.
 */
LabelCreator::LabelDocument newLabel(const LabelCreator::PaperSize &paper);

/** Whether @p object has text that can be printed white on black. */
bool canInvert(const LabelCreator::LabelObject &object);
/** Whether @p object's text is white on a black box. */
bool isInverted(const LabelCreator::LabelObject &object);
/**
 * Prints @p object's text white on a black box, or back to black on nothing.
 * Uses the object's colours, so DLS shows it the same way.
 */
void setInverted(LabelCreator::LabelObject &object, bool inverted);

/** A label of @p paperSize (in points), oriented as DLS orients it. */
LabelCreator::LabelDocument newLabel(const QString &paperName, const QString &id, const QSizeF &paperSize);

} // namespace ObjectFactory
