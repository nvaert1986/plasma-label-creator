/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

class QWidget;

/**
 * Shows the properties of @p object in a dialog, as DLS does on a
 * double-click. Returns true and updates @p object if the user accepted.
 * @p objectNames: the label's other objects, to anchor to; @p fields: the
 * fields a print condition can test.
 */
bool editObjectProperties(QWidget *parent,
                          LabelCreator::LabelObject &object,
                          LabelCreator::TapeCell *cell = nullptr,
                          const QStringList &objectNames = {},
                          const QStringList &fields = {});

/**
 * Asks for an image file: an SVG drawing as it is, any other picture as
 * PNG data; nothing if cancelled.
 */
QByteArray chooseImage(QWidget *parent);
/** Whether @p data, from chooseImage(), is an SVG drawing. */
bool isSvg(const QByteArray &data);
