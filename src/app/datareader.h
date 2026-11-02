/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "datatable.h"
#include "labeldocument.h"

#include <QHash>
#include <QStringList>

#include <optional>

using LabelCreator::DataTable;

/**
 * Reads the rows a data source describes: CSV and text files, Excel
 * (.xlsx) and OpenDocument (.ods) spreadsheets, and database queries
 * through Qt SQL. Columns without names (no header row) are called A, B,
 * C... like a spreadsheet's.
 */
namespace DataReader
{

/**
 * Reads @p source; @p password is for a database and never kept. Nullopt
 * and @p errorString when it cannot be read.
 */
std::optional<DataTable> read(const LabelCreator::DataSource &source, const QString &password = {}, QString *errorString = nullptr);

/** CSV text; a null @p delimiter is detected from the first line. */
inline std::optional<DataTable> parseCsv(const QByteArray &data, QChar delimiter, bool firstRowNames, QString *errorString = nullptr)
{
    return LabelCreator::Csv::parse(data, delimiter, firstRowNames, errorString);
}
using LabelCreator::Csv::detectDelimiter;

/** The sheets of a spreadsheet (.xlsx, .ods), in order. */
QStringList sheets(const QString &fileName, QString *errorString = nullptr);

/** Whether @p query only reads: one SELECT or WITH statement. */
bool isReadOnlyQuery(const QString &query);

using LabelCreator::columnName;

} // namespace DataReader
