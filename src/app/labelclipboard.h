/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

class QMimeData;

/**
 * Cut, copy and paste of label objects. Objects travel as a small DLS label
 * holding them, so pasting between two windows keeps everything; text and
 * images from other programs paste as new text and image objects.
 */
namespace LabelClipboard
{

/** Clipboard data for @p objects, taken from @p document. */
QMimeData *mimeData(const QList<LabelCreator::LabelObject> &objects, const LabelCreator::LabelDocument &document);

/** Whether @p data holds something that can be pasted onto a label. */
bool canPaste(const QMimeData *data);

/**
 * The objects to add to @p document for @p data: with names and groups of
 * their own, and moved a little together when one would land exactly on an
 * object already there.
 */
QList<LabelCreator::LabelObject> objectsToPaste(const QMimeData *data, const LabelCreator::LabelDocument &document);

} // namespace LabelClipboard
