/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QHash>

#include <optional>

namespace LabelCreator
{

/**
 * Fields on a label: {Name} in a text, an address, a barcode's data, a
 * counter's or date's text around it, or an image's file path, filled in
 * when printing from a data row and from the answers to print-time
 * questions. {{ and }} stand for a brace.
 */
namespace Fields
{

/** The field names @p text uses, in order, each once. */
QStringList used(const QString &text);
/** The field names the label uses, in order, each once. */
QStringList used(const LabelDocument &document);

/**
 * @p text with every {Name} replaced by its value. Names match exactly,
 * else ignoring case; a field without a value stays as it is written, so a
 * typing mistake shows on the label.
 */
QString fill(const QString &text, const QHash<QString, QString> &values);
/** A copy of @p document with its fields filled in. */
LabelDocument fill(const LabelDocument &document, const QHash<QString, QString> &values);

/** Why @p answer does not do for @p question; nullopt if it does. */
std::optional<QString> problem(const PrintQuestion &question, const QString &answer);

} // namespace Fields

} // namespace LabelCreator
