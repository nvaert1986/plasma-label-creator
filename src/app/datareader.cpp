/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "datareader.h"

#include <KLocalizedString>
#include <KZip>

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringDecoder>
#include <QUuid>
#include <QXmlStreamReader>

#include <cmath>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

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

namespace DataReader
{

namespace
{

bool fail(QString *errorString, const QString &message)
{
    if (errorString) {
        *errorString = message;
    }
    return false;
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

// Excel stores dates as days since 1899-12-30 (counting 1900 as a leap year,
// which this base absorbs for every date after February 1900).
QString excelDate(double serial, bool date, bool time)
{
    const QDate day = QDate(1899, 12, 30).addDays(qint64(std::floor(serial)));
    const QTime clock = QTime(0, 0).addMSecs(qint64(std::round((serial - std::floor(serial)) * 86400.0)) * 1000);
    const QLocale locale;
    if (date && time) {
        return locale.toString(day, QLocale::ShortFormat) + u' ' + locale.toString(clock, QLocale::ShortFormat);
    }
    return date ? locale.toString(day, QLocale::ShortFormat) : locale.toString(clock, QLocale::ShortFormat);
}

// Whether an Excel number format shows a date and/or a time.
std::pair<bool, bool> excelFormatKind(int id, const QString &code)
{
    if ((id >= 14 && id <= 17) || (id >= 27 && id <= 31) || (id >= 50 && id <= 58)) {
        return {true, false};
    }
    if (id == 22) {
        return {true, true};
    }
    if ((id >= 18 && id <= 21) || (id >= 45 && id <= 47) || (id >= 32 && id <= 36)) {
        return {false, true};
    }
    if (code.isEmpty()) {
        return {false, false};
    }
    // Leave out quoted text and [colour]/[locale] parts, then look at the letters.
    QString bare = code;
    bare.remove(QRegularExpression(u"\"[^\"]*\"|\\[[^\\]]*\\]|\\\\."_s));
    const bool date = bare.contains(u'd', Qt::CaseInsensitive) || bare.contains(u'y', Qt::CaseInsensitive);
    const bool time = bare.contains(u'h', Qt::CaseInsensitive) || bare.contains(u's', Qt::CaseInsensitive);
    return {date, time};
}

// An attribute by its local name, whatever its namespace prefix: OpenDocument
// writes table:name, office:value-type...
QStringView attribute(const QXmlStreamAttributes &attributes, QLatin1StringView name)
{
    for (const QXmlStreamAttribute &a : attributes) {
        if (a.name() == name) {
            return a.value();
        }
    }
    return {};
}

// "AB12" -> 27 (from 0); -1 if it has no letters.
int columnIndex(const QString &reference)
{
    int index = 0;
    int letters = 0;
    for (const QChar c : reference) {
        if (!c.isLetter()) {
            break;
        }
        index = index * 26 + (c.toUpper().unicode() - u'A' + 1);
        ++letters;
    }
    return letters > 0 ? index - 1 : -1;
}

QByteArray zipEntry(KZip &zip, const QString &path)
{
    const KArchiveEntry *entry = zip.directory()->entry(path);
    return entry && entry->isFile() ? static_cast<const KArchiveFile *>(entry)->data() : QByteArray();
}

// The workbook's sheets: names and their files in the archive.
QList<std::pair<QString, QString>> xlsxSheets(KZip &zip)
{
    QHash<QString, QString> targets; // relationship id -> file
    QXmlStreamReader rels(zipEntry(zip, u"xl/_rels/workbook.xml.rels"_s));
    while (!rels.atEnd()) {
        if (rels.readNext() == QXmlStreamReader::StartElement && rels.name() == "Relationship"_L1) {
            QString target = rels.attributes().value("Target"_L1).toString();
            target = target.startsWith(u'/') ? target.mid(1) : u"xl/"_s + target;
            targets.insert(rels.attributes().value("Id"_L1).toString(), target);
        }
    }
    QList<std::pair<QString, QString>> sheets;
    QXmlStreamReader workbook(zipEntry(zip, u"xl/workbook.xml"_s));
    while (!workbook.atEnd()) {
        if (workbook.readNext() == QXmlStreamReader::StartElement && workbook.name() == "sheet"_L1) {
            const QXmlStreamAttributes attributes = workbook.attributes();
            QString id;
            for (const QXmlStreamAttribute &attribute : attributes) {
                if (attribute.name() == "id"_L1) {
                    id = attribute.value().toString(); // r:id, whatever the prefix
                }
            }
            sheets.append({attributes.value("name"_L1).toString(), targets.value(id)});
        }
    }
    return sheets;
}

std::optional<QList<QStringList>> readXlsx(const QString &fileName, const QString &sheet, QString *errorString)
{
    KZip zip(fileName);
    if (!zip.open(QIODevice::ReadOnly)) {
        fail(errorString, i18n("%1 is not an Excel workbook.", fileName));
        return std::nullopt;
    }
    const auto sheets = xlsxSheets(zip);
    const auto chosen = std::find_if(sheets.cbegin(), sheets.cend(), [&sheet](const auto &s) {
        return sheet.isEmpty() || s.first == sheet;
    });
    if (chosen == sheets.cend()) {
        fail(errorString, sheet.isEmpty() ? i18n("%1 has no sheets.", fileName) : i18n("%1 has no sheet called %2.", fileName, sheet));
        return std::nullopt;
    }

    // Texts are kept once, in the shared strings.
    QStringList shared;
    QXmlStreamReader strings(zipEntry(zip, u"xl/sharedStrings.xml"_s));
    QString current;
    while (!strings.atEnd()) {
        strings.readNext();
        if (strings.isStartElement() && strings.name() == "si"_L1) {
            current.clear();
        } else if (strings.isStartElement() && strings.name() == "t"_L1) {
            current += strings.readElementText();
        } else if (strings.isEndElement() && strings.name() == "si"_L1) {
            shared << current;
        }
    }

    // Which cell styles show dates or times.
    QHash<int, QString> customFormats;
    QList<std::pair<bool, bool>> styleKinds;
    QXmlStreamReader styles(zipEntry(zip, u"xl/styles.xml"_s));
    bool inCellFormats = false;
    while (!styles.atEnd()) {
        styles.readNext();
        if (styles.isStartElement() && styles.name() == "numFmt"_L1) {
            customFormats.insert(styles.attributes().value("numFmtId"_L1).toInt(), styles.attributes().value("formatCode"_L1).toString());
        } else if (styles.isStartElement() && styles.name() == "cellXfs"_L1) {
            inCellFormats = true;
        } else if (styles.isEndElement() && styles.name() == "cellXfs"_L1) {
            inCellFormats = false;
        } else if (inCellFormats && styles.isStartElement() && styles.name() == "xf"_L1) {
            const int id = styles.attributes().value("numFmtId"_L1).toInt();
            styleKinds << excelFormatKind(id, customFormats.value(id));
        }
    }

    QList<QStringList> rows;
    QXmlStreamReader xml(zipEntry(zip, chosen->second));
    QStringList row;
    int rowNumber = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "row"_L1) {
            // Rows may skip empty ones: r says which this is.
            const int number = xml.attributes().value("r"_L1).toInt();
            while (number > 0 && rowNumber + 1 < number) {
                rows << QStringList();
                ++rowNumber;
            }
            row.clear();
        } else if (xml.isEndElement() && xml.name() == "row"_L1) {
            rows << row;
            ++rowNumber;
        } else if (xml.isStartElement() && xml.name() == "c"_L1) {
            const QXmlStreamAttributes attributes = xml.attributes();
            const int column = columnIndex(attributes.value("r"_L1).toString());
            const QString type = attributes.value("t"_L1).toString();
            const int style = attributes.value("s"_L1).toInt();
            QString value;
            while (!(xml.isEndElement() && xml.name() == "c"_L1) && !xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "v"_L1 || xml.name() == "t"_L1)) {
                    value += xml.readElementText();
                }
            }
            if (type == "s"_L1) {
                value = shared.value(value.toInt());
            } else if (type == "b"_L1) {
                value = value == "1"_L1 ? u"TRUE"_s : u"FALSE"_s;
            } else if (type.isEmpty() || type == "n"_L1) {
                const auto [date, time] = styleKinds.value(style);
                bool number = false;
                const double serial = value.toDouble(&number);
                if (number && (date || time)) {
                    value = excelDate(serial, date, time);
                }
            }
            const int at = column >= 0 ? column : int(row.size());
            while (row.size() < at) {
                row << QString();
            }
            row << value;
        }
    }
    if (xml.hasError()) {
        fail(errorString, i18n("%1 could not be read: %2", fileName, xml.errorString()));
        return std::nullopt;
    }
    return rows;
}

std::optional<QList<QStringList>> readOds(const QString &fileName, const QString &sheet, QString *errorString)
{
    KZip zip(fileName);
    if (!zip.open(QIODevice::ReadOnly)) {
        fail(errorString, i18n("%1 is not an OpenDocument spreadsheet.", fileName));
        return std::nullopt;
    }
    QXmlStreamReader xml(zipEntry(zip, u"content.xml"_s));
    QList<QStringList> rows;
    bool inSheet = false;
    bool done = false;
    // Empty rows and cells repeat up to the sheet's end; keep them only when
    // something follows.
    int pendingRows = 0;
    while (!xml.atEnd() && !done) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "table"_L1) {
            inSheet = sheet.isEmpty() || attribute(xml.attributes(), "name"_L1) == sheet;
        } else if (xml.isEndElement() && xml.name() == "table"_L1) {
            done = inSheet;
        } else if (inSheet && xml.isStartElement() && xml.name() == "table-row"_L1) {
            const int repeat = std::max(1, attribute(xml.attributes(), "number-rows-repeated"_L1).toInt());
            QStringList row;
            int pendingCells = 0;
            while (!(xml.isEndElement() && xml.name() == "table-row"_L1) && !xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "table-cell"_L1 || xml.name() == "covered-table-cell"_L1)) {
                    const QXmlStreamAttributes attributes = xml.attributes();
                    const int cells = std::max(1, attribute(attributes, "number-columns-repeated"_L1).toInt());
                    const QStringView type = attribute(attributes, "value-type"_L1);
                    QString value;
                    if (type == "date"_L1) {
                        const QString iso = attribute(attributes, "date-value"_L1).toString();
                        const QDateTime when = QDateTime::fromString(iso, Qt::ISODate);
                        value = iso.contains(u'T') ? QLocale().toString(when, QLocale::ShortFormat) : QLocale().toString(QDate::fromString(iso, Qt::ISODate), QLocale::ShortFormat);
                    } else if (type == "boolean"_L1) {
                        value = attribute(attributes, "boolean-value"_L1) == "true"_L1 ? u"TRUE"_s : u"FALSE"_s;
                    }
                    // The text as shown: paragraphs, spaces, tabs and line breaks.
                    QString text;
                    int paragraphs = 0;
                    const QString endName = xml.name().toString();
                    while (!(xml.isEndElement() && xml.name() == endName) && !xml.atEnd()) {
                        xml.readNext();
                        if (xml.isStartElement() && xml.name() == "p"_L1) {
                            if (paragraphs++ > 0) {
                                text += u'\n';
                            }
                        } else if (xml.isStartElement() && xml.name() == "s"_L1) {
                            text += QString(std::max(1, attribute(xml.attributes(), "c"_L1).toInt()), u' ');
                        } else if (xml.isStartElement() && xml.name() == "tab"_L1) {
                            text += u'\t';
                        } else if (xml.isStartElement() && xml.name() == "line-break"_L1) {
                            text += u'\n';
                        } else if (xml.isCharacters()) {
                            text += xml.text();
                        }
                    }
                    if (value.isEmpty()) {
                        value = text;
                    }
                    if (value.isEmpty()) {
                        pendingCells += cells;
                    } else {
                        for (; pendingCells > 0; --pendingCells) {
                            row << QString();
                        }
                        for (int i = 0; i < cells; ++i) {
                            row << value;
                        }
                    }
                }
            }
            if (row.isEmpty()) {
                pendingRows += repeat;
            } else {
                for (; pendingRows > 0; --pendingRows) {
                    rows << QStringList();
                }
                for (int i = 0; i < repeat; ++i) {
                    rows << row;
                }
            }
        }
    }
    if (xml.hasError()) {
        fail(errorString, i18n("%1 could not be read: %2", fileName, xml.errorString()));
        return std::nullopt;
    }
    if (!done && !sheet.isEmpty()) {
        fail(errorString, i18n("%1 has no sheet called %2.", fileName, sheet));
        return std::nullopt;
    }
    return rows;
}

std::optional<DataTable> readDatabase(const DataSource &source, const QString &password, QString *errorString)
{
    if (!QSqlDatabase::isDriverAvailable(source.driver)) {
        fail(errorString, i18n("The database driver %1 is not installed.", source.driver));
        return std::nullopt;
    }
    // Labels can come from anyone: printing one never changes a database.
    if (!isReadOnlyQuery(source.query)) {
        fail(errorString, i18n("Only a query that reads is run: one SELECT (or WITH ... SELECT) statement."));
        return std::nullopt;
    }
    const QString connection = u"labelcreator-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::optional<DataTable> result;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(source.driver, connection);
        database.setDatabaseName(source.database);
        database.setHostName(source.host);
        if (source.port > 0) {
            database.setPort(source.port);
        }
        database.setUserName(source.user);
        database.setPassword(password);
        if (source.driver == u"QSQLITE"_s) {
            database.setConnectOptions(u"QSQLITE_OPEN_READONLY"_s);
        }
        if (!database.open()) {
            fail(errorString, i18n("Could not connect to the database: %1", database.lastError().text()));
        } else {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            if (!query.exec(source.query)) {
                fail(errorString, i18n("The query failed: %1", query.lastError().text()));
            } else {
                DataTable table;
                const QSqlRecord record = query.record();
                for (int i = 0; i < record.count(); ++i) {
                    table.columns << (record.fieldName(i).isEmpty() ? columnName(i) : record.fieldName(i));
                }
                const QLocale locale;
                while (query.next()) {
                    QStringList row;
                    for (int i = 0; i < record.count(); ++i) {
                        const QVariant value = query.value(i);
                        if (value.typeId() == QMetaType::QDate) {
                            row << locale.toString(value.toDate(), QLocale::ShortFormat);
                        } else if (value.typeId() == QMetaType::QDateTime) {
                            row << locale.toString(value.toDateTime(), QLocale::ShortFormat);
                        } else {
                            row << value.toString();
                        }
                    }
                    table.rows << row;
                }
                result = table;
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return result;
}

} // namespace

QString columnName(int index)
{
    QString name;
    for (int n = index + 1; n > 0; n = (n - 1) / 26) {
        name.prepend(QChar(u'A' + (n - 1) % 26));
    }
    return name;
}

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

std::optional<DataTable> parseCsv(const QByteArray &data, QChar delimiter, bool firstRowNames, QString *errorString)
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
        fail(errorString, i18n("A quoted field is not closed."));
        return std::nullopt;
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row << field;
        rows << row;
    }
    return makeTable(rows, firstRowNames);
}

QStringList sheets(const QString &fileName, QString *errorString)
{
    KZip zip(fileName);
    if (!zip.open(QIODevice::ReadOnly)) {
        fail(errorString, i18n("%1 could not be opened.", fileName));
        return {};
    }
    QStringList names;
    if (fileName.endsWith(u".ods"_s, Qt::CaseInsensitive)) {
        QXmlStreamReader xml(zipEntry(zip, u"content.xml"_s));
        while (!xml.atEnd()) {
            if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == "table"_L1 && xml.namespaceUri().contains("table"_L1)) {
                names << attribute(xml.attributes(), "name"_L1).toString();
            }
        }
        return names;
    }
    for (const auto &sheet : xlsxSheets(zip)) {
        names << sheet.first;
    }
    return names;
}

bool isReadOnlyQuery(const QString &query)
{
    // Comments out, then one statement (a trailing ; is fine) that starts
    // with SELECT or WITH and has no writing keyword of its own.
    QString bare = query;
    bare.remove(QRegularExpression(u"--[^\\n]*|/\\*.*?\\*/"_s, QRegularExpression::DotMatchesEverythingOption));
    bare.remove(QRegularExpression(u"'(?:[^']|'')*'"_s)); // string literals may say anything
    bare = bare.trimmed();
    if (bare.endsWith(u';')) {
        bare.chop(1);
    }
    if (bare.contains(u';')) {
        return false;
    }
    static const QRegularExpression start(u"^(SELECT|WITH)\\b"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression writes(u"\\b(INSERT|UPDATE|DELETE|MERGE|DROP|ALTER|CREATE|TRUNCATE|GRANT|REVOKE|EXEC|EXECUTE|CALL|INTO|ATTACH|PRAGMA)\\b"_s,
                                           QRegularExpression::CaseInsensitiveOption);
    return start.match(bare).hasMatch() && !writes.match(bare).hasMatch();
}

std::optional<DataTable> read(const DataSource &source, const QString &password, QString *errorString)
{
    switch (source.kind) {
    case DataSource::Kind::None:
        fail(errorString, i18n("No data source has been chosen."));
        return std::nullopt;
    case DataSource::Kind::Database:
        return readDatabase(source, password, errorString);
    case DataSource::Kind::File:
        break;
    }
    const QString suffix = QFileInfo(source.fileName).suffix().toLower();
    if (suffix == u"xlsx"_s || suffix == u"xlsm"_s) {
        const auto rows = readXlsx(source.fileName, source.sheet, errorString);
        return rows ? std::optional(makeTable(*rows, source.firstRowNames)) : std::nullopt;
    }
    if (suffix == u"ods"_s) {
        const auto rows = readOds(source.fileName, source.sheet, errorString);
        return rows ? std::optional(makeTable(*rows, source.firstRowNames)) : std::nullopt;
    }
    QFile file(source.fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(errorString, i18n("Could not open %1: %2", source.fileName, file.errorString()));
        return std::nullopt;
    }
    return parseCsv(file.readAll(), source.delimiter, source.firstRowNames, errorString);
}

} // namespace DataReader
