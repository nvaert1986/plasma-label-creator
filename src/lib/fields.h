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
 *
 * A field can also work something out:
 * - {Date}, {Time} and {Now}: when printing, unless a column or question
 *   has that name;
 * - {Date+30}, {Installed-1y}: a date moved by days (d, the default),
 *   weeks (w), months (m) or years (y); {Serial+1}: a number added to;
 * - filters after a |, one after another: upper, lower, trim, left:N,
 *   right:N, mid:start:N (from 1), pad:N (zeros in front, or pad:N:c with
 *   c), date:format (such as dd-MM-yyyy or yyMMdd), replace:old:new and
 *   default:text (when empty). {Date+30|date:dd-MM-yyyy}, {Serial|pad:6}.
 * A field that cannot be worked out stays as it is written.
 */
namespace Fields
{

/** The field names @p text uses, without offsets and filters, in order, each once. */
QStringList used(const QString &text);
/** The field names the label uses, in order, each once. */
QStringList used(const LabelDocument &document);

/**
 * @p text with every {Name} replaced by its value. Names match exactly,
 * else ignoring case; a field without a value stays as it is written, so a
 * typing mistake shows on the label.
 */
QString fill(const QString &text, const QHash<QString, QString> &values, const QDateTime &now = QDateTime::currentDateTime());
/**
 * A copy of @p document with its fields filled in, its calculated fields
 * worked out first from @p values.
 */
LabelDocument fill(const LabelDocument &document, const QHash<QString, QString> &values, const QDateTime &now = QDateTime::currentDateTime());
/** @p values with the label's calculated fields added. */
QHash<QString, QString> withCalculated(const LabelDocument &document, const QHash<QString, QString> &values, const QDateTime &now = QDateTime::currentDateTime());

/** Why @p answer does not do for @p question; nullopt if it does. */
std::optional<QString> problem(const PrintQuestion &question, const QString &answer);

} // namespace Fields

} // namespace LabelCreator
