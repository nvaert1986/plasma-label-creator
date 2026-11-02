/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "datatable.h"

#include <QStringDecoder>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

QHash<QString, QString> DataTable::values(qsizetype index) const
{
    QHash<QString, QString> values;
    if (index < 0 || index >= rows.size()) {
        return values;
    }
    const QStringList &row = rows.at(index);
    for (qsizetype column = 0; column < columns.size(); ++column) {
        values.insert(columns.at(column), column < row.size() ? row.at(column) : QString());
    }
    return values;
}

QString columnName(int index)
{
    QString name;
    for (int n = index + 1; n > 0; n = (n - 1) / 26) {
        name.prepend(QChar(u'A' + (n - 1) % 26));
    }
    return name;
}

// Turns raw rows into a table: names from the first row, or A, B, C;
// rows padded to the same width; trailing empty rows dropped.
DataTable makeTable(QList<QStringList> rows, bool firstRowNames)
{
    while (!rows.isEmpty() && std::all_of(rows.constLast().cbegin(), rows.constLast().cend(), [](const QString &cell) {
        return cell.trimmed().isEmpty();
    })) {
        rows.removeLast();
    }
    qsizetype width = 0;
    for (const QStringList &row : std::as_const(rows)) {
        width = std::max(width, row.size());
    }
    DataTable table;
    if (firstRowNames && !rows.isEmpty()) {
        const QStringList names = rows.takeFirst();
        for (qsizetype i = 0; i < width; ++i) {
            QString name = i < names.size() ? names.at(i).trimmed() : QString();
            // Fields are written {Name}: names need something, and no braces.
            name.remove(u'{').remove(u'}');
            if (name.isEmpty() || table.columns.contains(name)) {
                name = columnName(int(i));
            }
            table.columns << name;
        }
    } else {
        for (qsizetype i = 0; i < width; ++i) {
            table.columns << columnName(int(i));
        }
    }
    for (QStringList &row : rows) {
        while (row.size() < width) {
            row << QString();
        }
    }
    table.rows = rows;
    return table;
}

namespace Csv
{

QChar detectDelimiter(const QString &firstLine)
{
    // Counted outside quotes; on a tie the first wins, so ; (Dutch and
    // German Excel) before , .
    const QList<QChar> candidates = {u';', u',', u'\t', u'|'};
    QHash<QChar, int> counts;
    bool quoted = false;
    for (const QChar c : firstLine) {
        if (c == u'"') {
            quoted = !quoted;
        } else if (!quoted && candidates.contains(c)) {
            ++counts[c];
        }
    }
    QChar best = u',';
    int most = 0;
    for (const QChar c : candidates) {
        if (counts.value(c) > most) {
            most = counts.value(c);
            best = c;
        }
    }
    return best;
}

std::optional<DataTable> parse(const QByteArray &data, QChar delimiter, bool firstRowNames, QString *errorString)
{
    // UTF-8 (with or without a byte order mark) when it is valid, else
    // Windows-1252, which Excel writes on Windows.
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8.decode(data);
    if (utf8.hasError()) {
        QStringDecoder windows("windows-1252");
        text = windows.isValid() ? windows.decode(data) : QString::fromLatin1(data);
    }
    if (text.startsWith(QChar(0xFEFF))) {
        text.remove(0, 1);
    }
    if (delimiter.isNull()) {
        delimiter = detectDelimiter(text.section(u'\n', 0, 0));
    }

    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c == u'"' && i + 1 < text.size() && text.at(i + 1) == u'"') {
                field += u'"';
                ++i;
            } else if (c == u'"') {
                quoted = false;
            } else {
                field += c;
            }
        } else if (c == u'"' && field.isEmpty()) {
            quoted = true;
        } else if (c == delimiter) {
            row << field;
            field.clear();
        } else if (c == u'\r') {
            // \r\n and lone \r both end the row.
            if (i + 1 < text.size() && text.at(i + 1) == u'\n') {
                ++i;
            }
            row << field;
            rows << row;
            row.clear();
            field.clear();
        } else if (c == u'\n') {
            row << field;
            rows << row;
            row.clear();
            field.clear();
        } else {
            field += c;
        }
    }
    if (quoted) {
        if (errorString) {
            *errorString = u"A quoted field is not closed."_s;
        }
        return std::nullopt;
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row << field;
        rows << row;
    }
    return makeTable(rows, firstRowNames);
}

} // namespace Csv

} // namespace LabelCreator
