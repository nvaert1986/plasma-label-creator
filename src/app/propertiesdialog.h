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
 */
bool editObjectProperties(QWidget *parent, LabelCreator::LabelObject &object, LabelCreator::TapeCell *cell = nullptr);

/** Asks for an image file and returns it as PNG data, or nothing if cancelled. */
QByteArray chooseImage(QWidget *parent);
