/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelprinter.h"

class QWidget;

/**
 * Label sizes the user made, for labels no list has, such as Altec's
 * self-laminating wrap labels. Kept in the app's settings and offered,
 * under "My label sizes", by printers that print sizes they do not list.
 */
namespace CustomLabels
{

QList<LabelCreator::PaperSize> all();
void add(const LabelCreator::PaperSize &paper);
void remove(const QString &name);

/**
 * Asks for a name, width, length and, for labels only partly printable,
 * the printable area; nullopt if cancelled. The size is saved. With
 * @p existing, that size is edited (and renamed, if the name changes).
 */
std::optional<LabelCreator::PaperSize> create(QWidget *parent, const std::optional<LabelCreator::PaperSize> &existing = std::nullopt);

/** The user's size called @p name, if there is one. */
std::optional<LabelCreator::PaperSize> find(const QString &name);

} // namespace CustomLabels
