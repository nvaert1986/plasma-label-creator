/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QStringList>

#include <optional>

namespace LabelCreator
{

/** Rows of data to print a label for each of. */
struct DataTable {
    QStringList columns;
    QList<QStringList> rows;

    /** Row @p index as field values by column name. */
    QHash<QString, QString> values(qsizetype index) const;
};

/** Spreadsheet-like column name for @p index: A, B, ... Z, AA, AB... */
QString columnName(int index);

/**
 * Raw rows as a table: column names from the first row when
 * @p firstRowNames (else A, B, C...), rows padded to the same width,
 * trailing empty rows dropped.
 */
DataTable makeTable(QList<QStringList> rows, bool firstRowNames);

namespace Csv
{

/** The delimiter @p firstLine uses most: ; , tab or |. */
QChar detectDelimiter(const QString &firstLine);
/**
 * CSV text, UTF-8 or else Windows-1252; a null @p delimiter is detected
 * from the first line.
 */
std::optional<DataTable> parse(const QByteArray &data, QChar delimiter, bool firstRowNames, QString *errorString = nullptr);

} // namespace Csv

} // namespace LabelCreator
