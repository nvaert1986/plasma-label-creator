/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "addressbook.h"

#include <QCollator>
#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

QString firstLine(const QString &text)
{
    return text.section(u'\n', 0, 0).trimmed();
}

// Column names recognised in a CSV header, English and Dutch.
enum class Column {
    Ignored,
    FullName,
    FirstName,
    LastName,
    Company,
    Street,
    PostalCode,
    City,
    Country,
};

Column columnFor(const QString &header)
{
    QString h = header.trimmed().toLower();
    h.remove(u' ').remove(u'_').remove(u'-');
    static const QList<std::pair<Column, QStringList>> names = {
        {Column::FullName, {u"name"_s, u"naam"_s, u"fullname"_s, u"volledigenaam"_s, u"contact"_s}},
        {Column::FirstName, {u"firstname"_s, u"givenname"_s, u"voornaam"_s}},
        {Column::LastName, {u"lastname"_s, u"surname"_s, u"familyname"_s, u"achternaam"_s}},
        {Column::Company, {u"company"_s, u"organization"_s, u"organisation"_s, u"bedrijf"_s, u"organisatie"_s, u"firma"_s}},
        {Column::Street, {u"street"_s, u"address"_s, u"address1"_s, u"straat"_s, u"adres"_s, u"straatnaam"_s}},
        {Column::PostalCode, {u"postcode"_s, u"postalcode"_s, u"zip"_s, u"zipcode"_s}},
        {Column::City, {u"city"_s, u"town"_s, u"plaats"_s, u"woonplaats"_s, u"stad"_s}},
        {Column::Country, {u"country"_s, u"land"_s}},
    };
    for (const auto &[column, aliases] : names) {
        if (aliases.contains(h)) {
            return column;
        }
    }
    return Column::Ignored;
}

QList<QStringList> splitCsv(const QString &text)
{
    // The delimiter that splits the first line into the most fields.
    const QString head = text.section(u'\n', 0, 0);
    QChar delimiter = u',';
    int best = 0;
    for (const QChar candidate : {u',', u';', u'\t'}) {
        const int count = int(head.count(candidate));
        if (count > best) {
            best = count;
            delimiter = candidate;
        }
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
            } else if (c != u'\r') {
                field += c;
            }
        } else if (c == u'"') {
            quoted = true;
        } else if (c == delimiter) {
            row << field;
            field.clear();
        } else if (c == u'\n') {
            row << field;
            field.clear();
            rows << row;
            row.clear();
        } else if (c != u'\r') {
            field += c;
        }
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row << field;
        rows << row;
    }
    return rows;
}

} // namespace

QString AddressEntry::displayName() const
{
    return fileAs.isEmpty() ? firstLine(address) : fileAs;
}

QStringList AddressEntry::fileAsSuggestions() const
{
    QStringList suggestions;
    const auto add = [&suggestions](const QString &suggestion) {
        if (!suggestion.trimmed().isEmpty() && !suggestions.contains(suggestion)) {
            suggestions << suggestion;
        }
    };
    if (!lastName.isEmpty() && !firstName.isEmpty()) {
        add(lastName + u", "_s + firstName);
        add(firstName + u' ' + lastName);
    } else {
        add(lastName);
        add(firstName);
    }
    // Further lines, e.g. the company.
    const QStringList lines = address.split(u'\n');
    for (qsizetype i = 0; i < std::min<qsizetype>(lines.size(), 2); ++i) {
        add(lines.at(i).trimmed());
    }
    return suggestions;
}

AddressEntry AddressBook::fromAddress(const QString &address)
{
    AddressEntry entry;
    entry.address = address;
    const QStringList words = firstLine(address).split(u' ', Qt::SkipEmptyParts);
    if (words.size() >= 2) {
        entry.lastName = words.last();
        entry.firstName = words.mid(0, words.size() - 1).join(u' ');
        entry.fileAs = entry.lastName + u", "_s + entry.firstName;
    } else if (!words.isEmpty()) {
        entry.lastName = words.first();
        entry.fileAs = entry.lastName;
    }
    return entry;
}

std::optional<AddressBook> AddressBook::load(const QString &fileName, QString *errorString)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return std::nullopt;
    }

    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != "AddressBook"_L1) {
        if (errorString) {
            *errorString = xml.hasError() ? xml.errorString() : u"Not a DYMO address book"_s;
        }
        return std::nullopt;
    }

    AddressBook book;
    book.defaultPermutation = xml.attributes().value("DefaultPermutation"_L1).compare("false"_L1, Qt::CaseInsensitive) != 0;
    while (xml.readNextStartElement()) {
        if (xml.name() != "AddressEntry"_L1) {
            xml.skipCurrentElement();
            continue;
        }
        AddressEntry entry;
        while (xml.readNextStartElement()) {
            if (xml.name() == "AddressData"_L1) {
                entry.address = xml.readElementText();
            } else if (xml.name() == "Name"_L1) {
                while (xml.readNextStartElement()) {
                    if (xml.name() == "FirstName"_L1) {
                        entry.firstName = xml.readElementText();
                    } else if (xml.name() == "LastName"_L1) {
                        entry.lastName = xml.readElementText();
                    } else if (xml.name() == "FileAs"_L1) {
                        entry.fileAs = xml.readElementText();
                    } else {
                        xml.skipCurrentElement();
                    }
                }
            } else {
                xml.skipCurrentElement();
            }
        }
        book.entries << entry;
    }
    if (xml.hasError()) {
        if (errorString) {
            *errorString = xml.errorString();
        }
        return std::nullopt;
    }
    book.sort();
    return book;
}

bool AddressBook::save(const QString &fileName, QString *errorString) const
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    // DLS writes its address books with a byte order mark.
    file.write("\xEF\xBB\xBF");
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement("AddressBook"_L1);
    xml.writeAttribute("Version"_L1, "8.0"_L1);
    xml.writeAttribute("DefaultPermutation"_L1, defaultPermutation ? "true"_L1 : "false"_L1);
    xml.writeAttribute("Format"_L1, "Native"_L1);
    for (const AddressEntry &entry : entries) {
        xml.writeStartElement("AddressEntry"_L1);
        xml.writeTextElement("AddressData"_L1, entry.address);
        xml.writeStartElement("Name"_L1);
        xml.writeTextElement("FirstName"_L1, entry.firstName);
        xml.writeTextElement("LastName"_L1, entry.lastName);
        xml.writeTextElement("FileAs"_L1, entry.fileAs);
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError() || !file.commit()) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    return true;
}

void AddressBook::sort()
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(entries.begin(), entries.end(), [&collator](const AddressEntry &a, const AddressEntry &b) {
        return collator.compare(a.displayName(), b.displayName()) < 0;
    });
}

QList<AddressEntry> AddressBook::parseCsv(const QString &text)
{
    QList<QStringList> rows = splitCsv(text);
    if (rows.isEmpty()) {
        return {};
    }

    QList<Column> columns;
    for (const QString &header : rows.constFirst()) {
        columns << columnFor(header);
    }
    const bool hasHeader = std::any_of(columns.cbegin(), columns.cend(), [](Column c) {
        return c != Column::Ignored;
    });
    if (hasHeader) {
        rows.removeFirst();
    }

    QList<AddressEntry> entries;
    for (const QStringList &row : std::as_const(rows)) {
        QStringList lines;
        AddressEntry entry;
        if (hasHeader) {
            QString full, company, street, postalCode, city, country;
            for (qsizetype i = 0; i < row.size() && i < columns.size(); ++i) {
                const QString value = row.at(i).trimmed();
                switch (columns.at(i)) {
                case Column::FullName:
                    full = value;
                    break;
                case Column::FirstName:
                    entry.firstName = value;
                    break;
                case Column::LastName:
                    entry.lastName = value;
                    break;
                case Column::Company:
                    company = value;
                    break;
                case Column::Street:
                    street = value;
                    break;
                case Column::PostalCode:
                    postalCode = value;
                    break;
                case Column::City:
                    city = value;
                    break;
                case Column::Country:
                    country = value;
                    break;
                case Column::Ignored:
                    break;
                }
            }
            if (full.isEmpty()) {
                full = QStringList{entry.firstName, entry.lastName}.join(u' ').trimmed();
            }
            // Postcode before the city, as in most of Europe.
            lines = {full, company, street, QStringList{postalCode, city}.join(u"  "_s).trimmed(), country};
        } else {
            lines = row;
        }
        for (QString &line : lines) {
            line = line.trimmed();
        }
        lines.removeAll(QString());
        if (lines.isEmpty()) {
            continue;
        }
        const QString address = lines.join(u'\n');
        if (entry.firstName.isEmpty() && entry.lastName.isEmpty()) {
            entry = fromAddress(address);
        } else {
            entry.address = address;
            entry.fileAs = entry.lastName.isEmpty() ? entry.firstName : (entry.firstName.isEmpty() ? entry.lastName : entry.lastName + u", "_s + entry.firstName);
        }
        entries << entry;
    }
    return entries;
}

QList<AddressEntry> AddressBook::importCsv(const QString &fileName, QString *errorString)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return {};
    }
    const QByteArray data = file.readAll();
    // UTF-8 (with or without BOM), else the Windows code page spreadsheet
    // programs use.
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8.decode(data);
    if (utf8.hasError()) {
        QStringDecoder latin(QStringDecoder::Latin1);
        text = latin.decode(data);
    }
    return parseCsv(text);
}

} // namespace LabelCreator
