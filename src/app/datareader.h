/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QHash>
#include <QStringList>

#include <optional>

/** Rows of data to print a label for each of. */
struct DataTable {
    QStringList columns;
    QList<QStringList> rows;

    /** Row @p index as field values by column name. */
    QHash<QString, QString> values(qsizetype index) const;
};

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
std::optional<DataTable> parseCsv(const QByteArray &data, QChar delimiter, bool firstRowNames, QString *errorString = nullptr);
/** The delimiter @p data's first line uses most: ; , tab or |. */
QChar detectDelimiter(const QString &firstLine);

/** The sheets of a spreadsheet (.xlsx, .ods), in order. */
QStringList sheets(const QString &fileName, QString *errorString = nullptr);

/** Whether @p query only reads: one SELECT or WITH statement. */
bool isReadOnlyQuery(const QString &query);

/** Spreadsheet-like column name for @p index: A, B, ... Z, AA, AB... */
QString columnName(int index);

} // namespace DataReader
