/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "gs1.h"

#include <KLocalizedString>
#include <QDate>
#include <QRegularExpression>
#include <QTime>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace Gs1
{

namespace
{

Identifier fixedDigits(const QString &ai, const QString &title, int length, bool check = false)
{
    Identifier id{ai, title, length, length, true};
    id.checkDigit = check;
    return id;
}

Identifier digitsUpTo(const QString &ai, const QString &title, int maximum)
{
    return Identifier{ai, title, 1, maximum, true};
}

Identifier text(const QString &ai, const QString &title, int maximum)
{
    return Identifier{ai, title, 1, maximum, false};
}

Identifier date(const QString &ai, const QString &title)
{
    Identifier id = fixedDigits(ai, title, 6);
    id.date = true;
    return id;
}

QList<Identifier> makeIdentifiers()
{
    // From the GS1 General Specifications: the ones labels use most.
    QList<Identifier> list = {
        fixedDigits(u"01"_s, u"GTIN"_s, 14, true),
        text(u"10"_s, i18n("Batch or lot number"), 20),
        date(u"17"_s, i18n("Expiration date")),
        date(u"15"_s, i18n("Best before date")),
        text(u"21"_s, i18n("Serial number"), 20),
        fixedDigits(u"00"_s, u"SSCC"_s, 18, true),
        fixedDigits(u"02"_s, i18n("GTIN of contained trade items"), 14, true),
        date(u"11"_s, i18n("Production date")),
        date(u"12"_s, i18n("Due date")),
        date(u"13"_s, i18n("Packaging date")),
        date(u"16"_s, i18n("Sell by date")),
        fixedDigits(u"20"_s, i18n("Internal product variant"), 2),
        text(u"22"_s, i18n("Consumer product variant"), 20),
        text(u"240"_s, i18n("Additional product identification"), 30),
        text(u"241"_s, i18n("Customer part number"), 30),
        text(u"250"_s, i18n("Secondary serial number"), 30),
        digitsUpTo(u"30"_s, i18n("Variable count of items"), 8),
        digitsUpTo(u"37"_s, i18n("Count of trade items"), 8),
        text(u"400"_s, i18n("Customer's purchase order number"), 30),
        text(u"401"_s, i18n("Global Identification Number for Consignment"), 30),
        fixedDigits(u"402"_s, i18n("Global Shipment Identification Number"), 17, true),
        fixedDigits(u"410"_s, i18n("Ship to / deliver to GLN"), 13, true),
        fixedDigits(u"411"_s, i18n("Bill to / invoice to GLN"), 13, true),
        fixedDigits(u"412"_s, i18n("Purchased from GLN"), 13, true),
        fixedDigits(u"413"_s, i18n("Ship for / deliver for / forward to GLN"), 13, true),
        fixedDigits(u"414"_s, i18n("Identification of a physical location (GLN)"), 13, true),
        text(u"420"_s, i18n("Ship to / deliver to postal code"), 20),
        fixedDigits(u"422"_s, i18n("Country of origin (ISO 3166 code)"), 3),
        text(u"8004"_s, i18n("Global Individual Asset Identifier"), 30),
        text(u"90"_s, i18n("Information agreed between trading partners"), 30),
    };
    Identifier postal = text(u"421"_s, i18n("Ship to postal code with ISO country code"), 12);
    postal.minimumLength = 4;
    postal.numericPrefix = 3;
    list << postal;
    Identifier expiryTime = fixedDigits(u"7003"_s, i18n("Expiration date and time"), 10);
    expiryTime.dateTime = true;
    list << expiryTime;
    // Measures with the decimal point's place in the AI's last digit.
    const QList<std::pair<QString, QString>> measures = {{u"310"_s, i18n("Net weight, kilograms")},
                                                         {u"330"_s, i18n("Gross weight, kilograms")},
                                                         {u"311"_s, i18n("Length, metres")},
                                                         {u"312"_s, i18n("Width, metres")},
                                                         {u"313"_s, i18n("Height, metres")},
                                                         {u"315"_s, i18n("Net volume, litres")}};
    for (const auto &[base, title] : measures) {
        for (int decimals = 0; decimals <= 5; ++decimals) {
            list << fixedDigits(base + QString::number(decimals), i18ncp("@item GS1 measure, such as Net weight, kilograms", "%2 (%1 decimal)", "%2 (%1 decimals)", decimals, title), 6);
        }
    }
    for (int ai = 91; ai <= 99; ++ai) {
        list << text(QString::number(ai), i18n("Company internal information"), 90);
    }
    return list;
}

// GS1's 82 characters for alphanumeric values (character set 82).
bool isGs1Text(const QString &value)
{
    static const QRegularExpression allowed(u"^[A-Za-z0-9!\"%&'()*+,\\-./:;<=>?_]*$"_s);
    return allowed.match(value).hasMatch();
}

bool isDigits(QStringView value)
{
    return !value.isEmpty() && std::all_of(value.cbegin(), value.cend(), [](QChar c) {
        return c >= u'0' && c <= u'9';
    });
}

// YYMMDD; a day of 00 means the end of the month, as GS1 allows.
bool isDate(QStringView value)
{
    if (value.size() != 6 || !isDigits(value)) {
        return false;
    }
    const int month = value.mid(2, 2).toInt();
    const int day = value.mid(4, 2).toInt();
    return month >= 1 && month <= 12 && (day == 0 || QDate(2000 + value.left(2).toInt(), month, day).isValid());
}

} // namespace

QString Identifier::format() const
{
    QString result = numeric ? u"N"_s : u"X"_s;
    if (numericPrefix > 0) {
        result = u"N%1+X..%2"_s.arg(numericPrefix).arg(maximumLength - numericPrefix);
    } else if (minimumLength == maximumLength) {
        result += QString::number(maximumLength);
    } else {
        result += u".."_s + QString::number(maximumLength);
    }
    if (date) {
        result += u" (YYMMDD)"_s;
    } else if (dateTime) {
        result += u" (YYMMDDHHMM)"_s;
    } else if (checkDigit) {
        result += i18nc("@info appended to a GS1 value's form, such as N13", ", with check digit");
    }
    return result;
}

const QList<Identifier> &identifiers()
{
    static const QList<Identifier> list = makeIdentifiers();
    return list;
}

std::optional<Identifier> identifier(const QString &ai)
{
    for (const Identifier &id : identifiers()) {
        if (id.ai == ai) {
            return id;
        }
    }
    return std::nullopt;
}

std::optional<QList<Element>> parse(const QString &data)
{
    QList<Element> elements;
    const QString trimmed = data.trimmed();
    qsizetype at = 0;
    while (at < trimmed.size()) {
        if (trimmed.at(at) != u'[') {
            return std::nullopt;
        }
        const qsizetype close = trimmed.indexOf(u']', at);
        if (close < 0 || !isDigits(QStringView(trimmed).mid(at + 1, close - at - 1))) {
            return std::nullopt;
        }
        // The value runs to the next [ that starts an AI, not one inside a field.
        qsizetype next = close + 1;
        int depth = 0;
        while (next < trimmed.size() && !(depth == 0 && trimmed.at(next) == u'[')) {
            depth += trimmed.at(next) == u'{' ? 1 : (trimmed.at(next) == u'}' ? -1 : 0);
            ++next;
        }
        elements.append({trimmed.mid(at + 1, close - at - 1), trimmed.mid(close + 1, next - close - 1)});
        at = next;
    }
    return elements;
}

QString compose(const QList<Element> &elements)
{
    QString data;
    for (const Element &element : elements) {
        data += u'[' + element.ai + u']' + element.value;
    }
    return data;
}

QChar checkDigit(const QString &digits)
{
    // From the right: weights 3, 1, 3, 1...
    int sum = 0;
    for (qsizetype i = 0; i < digits.size(); ++i) {
        const int digit = digits.at(digits.size() - 1 - i).digitValue();
        sum += digit * (i % 2 == 0 ? 3 : 1);
    }
    return QChar(u'0' + (10 - sum % 10) % 10);
}

std::optional<QString> problem(const Element &element)
{
    if (element.value.contains(u'{')) {
        return std::nullopt; // filled in when printing
    }
    const std::optional<Identifier> known = identifier(element.ai);
    if (!known) {
        if (element.ai.size() < 2 || element.ai.size() > 4) {
            return i18n("(%1) is not an application identifier.", element.ai);
        }
        return isGs1Text(element.value) && !element.value.isEmpty() ? std::nullopt
                                                                      : std::optional(i18n("(%1) needs a value of GS1 characters.", element.ai));
    }
    const Identifier &id = *known;
    const QString &value = element.value;
    if (value.size() < id.minimumLength || value.size() > id.maximumLength) {
        return id.minimumLength == id.maximumLength ? u"(%1) %2 needs %3 characters."_s.arg(id.ai, id.title).arg(id.maximumLength)
                                                    : u"(%1) %2 needs %3 to %4 characters."_s.arg(id.ai, id.title).arg(id.minimumLength).arg(id.maximumLength);
    }
    if (id.numeric && !isDigits(value)) {
        return i18n("(%1) %2 takes digits only.", id.ai, id.title);
    }
    if (id.numericPrefix > 0 && !isDigits(QStringView(value).left(id.numericPrefix))) {
        return i18n("(%1) %2 starts with %3 digits.", id.ai, id.title, id.numericPrefix);
    }
    if (!id.numeric && !isGs1Text(value)) {
        return i18n("(%1) %2 has characters GS1 does not allow (letters, digits and !\"%&'()*+,-./:;<=>?_ only).", id.ai, id.title);
    }
    if (id.date && !isDate(value)) {
        return i18n("(%1) %2 is not a date written YYMMDD.", id.ai, id.title);
    }
    if (id.dateTime && (!isDate(QStringView(value).left(6)) || !QTime(value.mid(6, 2).toInt(), value.mid(8, 2).toInt()).isValid())) {
        return i18n("(%1) %2 is not written YYMMDDHHMM.", id.ai, id.title);
    }
    if (id.checkDigit && checkDigit(value.left(value.size() - 1)) != value.back()) {
        return i18n("(%1) %2: the check digit should be %3.", id.ai, id.title, QString(checkDigit(value.left(value.size() - 1))));
    }
    return std::nullopt;
}

} // namespace Gs1

} // namespace LabelCreator
