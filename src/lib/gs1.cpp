/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "gs1.h"

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
        text(u"10"_s, u"Batch or lot number"_s, 20),
        date(u"17"_s, u"Expiration date"_s),
        date(u"15"_s, u"Best before date"_s),
        text(u"21"_s, u"Serial number"_s, 20),
        fixedDigits(u"00"_s, u"SSCC"_s, 18, true),
        fixedDigits(u"02"_s, u"GTIN of contained trade items"_s, 14, true),
        date(u"11"_s, u"Production date"_s),
        date(u"12"_s, u"Due date"_s),
        date(u"13"_s, u"Packaging date"_s),
        date(u"16"_s, u"Sell by date"_s),
        fixedDigits(u"20"_s, u"Internal product variant"_s, 2),
        text(u"22"_s, u"Consumer product variant"_s, 20),
        text(u"240"_s, u"Additional product identification"_s, 30),
        text(u"241"_s, u"Customer part number"_s, 30),
        text(u"250"_s, u"Secondary serial number"_s, 30),
        digitsUpTo(u"30"_s, u"Variable count of items"_s, 8),
        digitsUpTo(u"37"_s, u"Count of trade items"_s, 8),
        text(u"400"_s, u"Customer's purchase order number"_s, 30),
        text(u"401"_s, u"Global Identification Number for Consignment"_s, 30),
        fixedDigits(u"402"_s, u"Global Shipment Identification Number"_s, 17, true),
        fixedDigits(u"410"_s, u"Ship to / deliver to GLN"_s, 13, true),
        fixedDigits(u"411"_s, u"Bill to / invoice to GLN"_s, 13, true),
        fixedDigits(u"412"_s, u"Purchased from GLN"_s, 13, true),
        fixedDigits(u"413"_s, u"Ship for / deliver for / forward to GLN"_s, 13, true),
        fixedDigits(u"414"_s, u"Identification of a physical location (GLN)"_s, 13, true),
        text(u"420"_s, u"Ship to / deliver to postal code"_s, 20),
        fixedDigits(u"422"_s, u"Country of origin (ISO 3166 code)"_s, 3),
        text(u"8004"_s, u"Global Individual Asset Identifier"_s, 30),
        text(u"90"_s, u"Information agreed between trading partners"_s, 30),
    };
    Identifier postal = text(u"421"_s, u"Ship to postal code with ISO country code"_s, 12);
    postal.minimumLength = 4;
    postal.numericPrefix = 3;
    list << postal;
    Identifier expiryTime = fixedDigits(u"7003"_s, u"Expiration date and time"_s, 10);
    expiryTime.dateTime = true;
    list << expiryTime;
    // Measures with the decimal point's place in the AI's last digit.
    const QList<std::pair<QString, QString>> measures = {{u"310"_s, u"Net weight, kilograms"_s},
                                                         {u"330"_s, u"Gross weight, kilograms"_s},
                                                         {u"311"_s, u"Length, metres"_s},
                                                         {u"312"_s, u"Width, metres"_s},
                                                         {u"313"_s, u"Height, metres"_s},
                                                         {u"315"_s, u"Net volume, litres"_s}};
    for (const auto &[base, title] : measures) {
        for (int decimals = 0; decimals <= 5; ++decimals) {
            list << fixedDigits(base + QString::number(decimals), u"%1 (%2 decimals)"_s.arg(title).arg(decimals), 6);
        }
    }
    for (int ai = 91; ai <= 99; ++ai) {
        list << text(QString::number(ai), u"Company internal information"_s, 90);
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
        result += u", with check digit"_s;
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
            return u"(%1) is not an application identifier."_s.arg(element.ai);
        }
        return isGs1Text(element.value) && !element.value.isEmpty() ? std::nullopt
                                                                      : std::optional(u"(%1) needs a value of GS1 characters."_s.arg(element.ai));
    }
    const Identifier &id = *known;
    const QString &value = element.value;
    if (value.size() < id.minimumLength || value.size() > id.maximumLength) {
        return id.minimumLength == id.maximumLength ? u"(%1) %2 needs %3 characters."_s.arg(id.ai, id.title).arg(id.maximumLength)
                                                    : u"(%1) %2 needs %3 to %4 characters."_s.arg(id.ai, id.title).arg(id.minimumLength).arg(id.maximumLength);
    }
    if (id.numeric && !isDigits(value)) {
        return u"(%1) %2 takes digits only."_s.arg(id.ai, id.title);
    }
    if (id.numericPrefix > 0 && !isDigits(QStringView(value).left(id.numericPrefix))) {
        return u"(%1) %2 starts with %3 digits."_s.arg(id.ai, id.title).arg(id.numericPrefix);
    }
    if (!id.numeric && !isGs1Text(value)) {
        return u"(%1) %2 has characters GS1 does not allow (letters, digits and !\"%&'()*+,-./:;<=>?_ only)."_s.arg(id.ai, id.title);
    }
    if (id.date && !isDate(value)) {
        return u"(%1) %2 is not a date written YYMMDD."_s.arg(id.ai, id.title);
    }
    if (id.dateTime && (!isDate(QStringView(value).left(6)) || !QTime(value.mid(6, 2).toInt(), value.mid(8, 2).toInt()).isValid())) {
        return u"(%1) %2 is not written YYMMDDHHMM."_s.arg(id.ai, id.title);
    }
    if (id.checkDigit && checkDigit(value.left(value.size() - 1)) != value.back()) {
        return u"(%1) %2: the check digit should be %3."_s.arg(id.ai, id.title, QString(checkDigit(value.left(value.size() - 1))));
    }
    return std::nullopt;
}

} // namespace Gs1

} // namespace LabelCreator
