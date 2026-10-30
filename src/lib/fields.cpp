/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "fields.h"

#include <QLocale>
#include <QRegularExpression>

#include <functional>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace Fields
{

namespace
{

// Walks @p text: literal parts go to @p literal, fields to @p field.
// {{ and }} are literal braces; an unclosed { is literal too.
void scan(const QString &text, const std::function<void(const QString &)> &literal, const std::function<void(const QString &)> &field)
{
    QString pending;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if ((c == u'{' || c == u'}') && i + 1 < text.size() && text.at(i + 1) == c) {
            pending += c;
            ++i;
            continue;
        }
        if (c == u'{') {
            const qsizetype close = text.indexOf(u'}', i + 1);
            const qsizetype nested = text.indexOf(u'{', i + 1);
            if (close > i + 1 && (nested < 0 || nested > close) && !text.mid(i + 1, close - i - 1).contains(u'\n')) {
                literal(pending);
                pending.clear();
                field(text.mid(i + 1, close - i - 1).trimmed());
                i = close;
                continue;
            }
        }
        pending += c;
    }
    literal(pending);
}

// A field's value, and the moment it stands for when it is a date or time.
struct Value {
    enum class Shows {
        Date,
        Time,
        DateTime,
    };
    QString text;
    std::optional<QDateTime> when;
    Shows shows = Shows::Date;
};

QString show(const QDateTime &when, Value::Shows shows)
{
    const QLocale locale;
    switch (shows) {
    case Value::Shows::Date:
        return locale.toString(when.date(), QLocale::ShortFormat);
    case Value::Shows::Time:
        return locale.toString(when.time(), QLocale::ShortFormat);
    case Value::Shows::DateTime:
        break;
    }
    return locale.toString(when, QLocale::ShortFormat);
}

// Dates and times as they come from data: the user's short format, ISO, or
// day first; a time alone is today's.
std::optional<Value> parseDate(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    const QLocale locale;
    // Two-digit years (10/30/26) are this century or the end of the last.
    const int baseYear = QDate::currentDate().year() - 50;
    if (const QDateTime when = locale.toDateTime(trimmed, locale.dateTimeFormat(QLocale::ShortFormat), baseYear); when.isValid()) {
        return Value{trimmed, when, Value::Shows::DateTime};
    }
    if (const QDate date = locale.toDate(trimmed, locale.dateFormat(QLocale::ShortFormat), baseYear); date.isValid()) {
        return Value{trimmed, QDateTime(date, QTime(0, 0)), Value::Shows::Date};
    }
    for (const QString &format : {locale.timeFormat(QLocale::ShortFormat), u"HH:mm"_s, u"H:mm"_s, u"HH:mm:ss"_s}) {
        if (const QTime time = locale.toTime(trimmed, format); time.isValid()) {
            return Value{trimmed, QDateTime(QDate::currentDate(), time), Value::Shows::Time};
        }
    }
    if (const QDateTime when = QDateTime::fromString(trimmed, Qt::ISODate); when.isValid()) {
        return Value{trimmed, when, trimmed.contains(u'T') ? Value::Shows::DateTime : Value::Shows::Date};
    }
    for (const QString &format : {u"yyyy-MM-dd"_s, u"dd-MM-yyyy"_s, u"d-M-yyyy"_s, u"dd/MM/yyyy"_s, u"d/M/yyyy"_s, u"dd.MM.yyyy"_s}) {
        if (const QDate date = QDate::fromString(trimmed, format); date.isValid()) {
            return Value{trimmed, QDateTime(date, QTime(0, 0)), Value::Shows::Date};
        }
    }
    return std::nullopt;
}

std::optional<Value> lookUp(const QString &name, const QHash<QString, QString> &values, const QDateTime &now)
{
    if (const auto it = values.constFind(name); it != values.constEnd()) {
        return Value{*it, {}, {}};
    }
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (it.key().compare(name, Qt::CaseInsensitive) == 0) {
            return Value{it.value(), {}, {}};
        }
    }
    // When printing, unless the data has a field of that name.
    for (const auto &[builtIn, shows] : {std::pair(u"Date"_s, Value::Shows::Date), std::pair(u"Time"_s, Value::Shows::Time), std::pair(u"Now"_s, Value::Shows::DateTime)}) {
        if (name.compare(builtIn, Qt::CaseInsensitive) == 0) {
            return Value{show(now, shows), now, shows};
        }
    }
    return std::nullopt;
}

// {Name+30}, {Name-1y}, {Time+2h}: moved by days, weeks, months, years or
// hours, or a number added to.
std::optional<Value> offset(Value value, QChar sign, int amount, QChar unit)
{
    const int signedAmount = sign == u'-' ? -amount : amount;
    if (unit.isNull()) {
        bool number = false;
        const qlonglong n = value.text.trimmed().toLongLong(&number);
        if (number && !value.when) {
            // Keeps leading zeros: 0041 + 1 is 0042.
            const QString digits = value.text.trimmed();
            QString result = QString::number(n + signedAmount);
            const bool zeros = digits.startsWith(u'0') && digits.size() > 1;
            if (zeros && !result.startsWith(u'-')) {
                result = result.rightJustified(digits.size(), u'0');
            }
            return Value{result, {}, {}};
        }
        unit = u'd';
    }
    if (!value.when) {
        const std::optional<Value> parsed = parseDate(value.text);
        if (!parsed) {
            return std::nullopt;
        }
        value = *parsed;
    }
    QDateTime when = *value.when;
    switch (unit.toLower().unicode()) {
    case 'd':
        when = when.addDays(signedAmount);
        break;
    case 'w':
        when = when.addDays(7 * qint64(signedAmount));
        break;
    case 'm':
        when = when.addMonths(signedAmount);
        break;
    case 'y':
        when = when.addYears(signedAmount);
        break;
    case 'h':
        when = when.addSecs(3600 * qint64(signedAmount));
        break;
    default:
        return std::nullopt;
    }
    return Value{show(when, value.shows), when, value.shows};
}

std::optional<Value> applyFilter(Value value, const QString &filter)
{
    const qsizetype colon = filter.indexOf(u':');
    const QString name = filter.left(colon).trimmed().toLower();
    const QString argument = colon < 0 ? QString() : filter.mid(colon + 1);
    const QStringList arguments = argument.split(u':');
    const auto number = [&arguments](int i, int fallback) {
        bool ok = false;
        const int n = i < arguments.size() ? arguments.at(i).trimmed().toInt(&ok) : fallback;
        return ok || i >= arguments.size() ? n : -1;
    };
    QString &text = value.text;
    if (name == u"upper"_s) {
        text = text.toUpper();
    } else if (name == u"lower"_s) {
        text = text.toLower();
    } else if (name == u"trim"_s) {
        text = text.trimmed();
    } else if (name == u"left"_s && number(0, -1) >= 0) {
        text = text.left(number(0, 0));
    } else if (name == u"right"_s && number(0, -1) >= 0) {
        text = text.right(number(0, 0));
    } else if (name == u"mid"_s && number(0, -1) >= 1) {
        text = text.mid(number(0, 1) - 1, arguments.size() > 1 ? number(1, -1) : -1);
    } else if (name == u"pad"_s && number(0, -1) >= 0) {
        const QChar fill = arguments.size() > 1 && !arguments.at(1).isEmpty() ? arguments.at(1).at(0) : u'0';
        text = text.rightJustified(number(0, 0), fill);
    } else if (name == u"date"_s && !argument.isEmpty()) {
        const std::optional<QDateTime> when = value.when ? value.when : parseDate(text).value_or(Value()).when;
        if (!when) {
            return std::nullopt;
        }
        text = QLocale().toString(*when, argument);
    } else if (name == u"replace"_s && !arguments.isEmpty() && !arguments.constFirst().isEmpty()) {
        text = text.replace(arguments.at(0), arguments.value(1));
    } else if (name == u"default"_s) {
        if (text.trimmed().isEmpty()) {
            text = argument;
        }
    } else {
        return std::nullopt; // a typing mistake shows on the label
    }
    if (name != u"date"_s) {
        value.when.reset();
    }
    return value;
}

// The name a field's expression starts from: {Serial+1|pad:6} -> Serial.
QString baseName(const QString &expression, QChar *sign = nullptr, int *amount = nullptr, QChar *unit = nullptr)
{
    static const QRegularExpression withOffset(u"^(.*?)\\s*([+-])\\s*(\\d+)\\s*([dwmyhDWMYH]?)$"_s);
    const QString head = expression.section(u'|', 0, 0).trimmed();
    const QRegularExpressionMatch match = withOffset.match(head);
    if (!match.hasMatch() || match.captured(1).isEmpty()) {
        return head;
    }
    if (sign) {
        *sign = match.captured(2).at(0);
        *amount = match.captured(3).toInt();
        *unit = match.captured(4).isEmpty() ? QChar() : match.captured(4).at(0);
    }
    return match.captured(1).trimmed();
}

std::optional<QString> evaluate(const QString &expression, const QHash<QString, QString> &values, const QDateTime &now)
{
    const QStringList parts = expression.split(u'|');
    // A name of its own first, so a column called "A-1" is still found.
    std::optional<Value> value = lookUp(parts.constFirst().trimmed(), values, now);
    if (!value) {
        QChar sign;
        int amount = 0;
        QChar unit;
        const QString name = baseName(expression, &sign, &amount, &unit);
        if (sign.isNull() || !(value = lookUp(name, values, now))) {
            return std::nullopt;
        }
        value = offset(*value, sign, amount, unit);
    }
    for (qsizetype i = 1; i < parts.size() && value; ++i) {
        value = applyFilter(*value, parts.at(i));
    }
    return value ? std::optional(value->text) : std::nullopt;
}

// Calls @p change on every text of the label that can hold fields.
void forEachText(LabelDocument &document, const std::function<void(QString &)> &change)
{
    for (LabelObject &object : document.objects) {
        std::visit(
            [&change](auto &o) {
                using T = std::decay_t<decltype(o)>;
                if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                    for (TextSpan &span : o.spans) {
                        change(span.text);
                    }
                } else if constexpr (std::is_same_v<T, BarcodeObject> || std::is_same_v<T, CircularTextObject>) {
                    change(o.text);
                } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject>) {
                    change(o.preText);
                    change(o.postText);
                } else if constexpr (std::is_same_v<T, ImageObject>) {
                    if (o.imageLocation) {
                        change(*o.imageLocation);
                    }
                }
            },
            object);
    }
}

} // namespace

QStringList used(const QString &text)
{
    QStringList names;
    scan(
        text,
        [](const QString &) {},
        [&names](const QString &expression) {
            const QString name = baseName(expression);
            if (!names.contains(name)) {
                names << name;
            }
        });
    return names;
}

QStringList used(const LabelDocument &document)
{
    QStringList names;
    LabelDocument copy = document;
    forEachText(copy, [&names](QString &text) {
        for (const QString &name : used(text)) {
            if (!names.contains(name)) {
                names << name;
            }
        }
    });
    return names;
}

QString fill(const QString &text, const QHash<QString, QString> &values, const QDateTime &now)
{
    QString result;
    scan(
        text,
        [&result](const QString &part) {
            result += part;
        },
        [&result, &values, &now](const QString &expression) {
            result += evaluate(expression, values, now).value_or(u'{' + expression + u'}');
        });
    return result;
}

QHash<QString, QString> withCalculated(const LabelDocument &document, const QHash<QString, QString> &values, const QDateTime &now)
{
    // In order: each can use the ones before it.
    QHash<QString, QString> all = values;
    for (const CalculatedField &field : document.calculated) {
        all.insert(field.name, fill(field.expression, all, now));
    }
    return all;
}

LabelDocument fill(const LabelDocument &document, const QHash<QString, QString> &values, const QDateTime &now)
{
    const QHash<QString, QString> all = withCalculated(document, values, now);
    LabelDocument filled = document;
    forEachText(filled, [&all, &now](QString &text) {
        text = fill(text, all, now);
    });
    // An address's line fonts follow its lines, which a field may add.
    for (LabelObject &object : filled.objects) {
        if (auto *address = std::get_if<AddressObject>(&object)) {
            address->fitLineFonts();
        }
    }
    return filled;
}

std::optional<QString> problem(const PrintQuestion &question, const QString &answer)
{
    const QString label = question.prompt.isEmpty() ? question.name : question.prompt;
    if (answer.isEmpty()) {
        return question.required ? std::optional(u"%1: an answer is needed."_s.arg(label)) : std::nullopt;
    }
    if (question.minimumLength > 0 && answer.size() < question.minimumLength) {
        return u"%1: at least %2 characters."_s.arg(label).arg(question.minimumLength);
    }
    if (question.maximumLength > 0 && answer.size() > question.maximumLength) {
        return u"%1: at most %2 characters."_s.arg(label).arg(question.maximumLength);
    }
    const auto all = [&answer](const std::function<bool(QChar)> &allowed) {
        return std::all_of(answer.cbegin(), answer.cend(), allowed);
    };
    switch (question.allowed) {
    case PrintQuestion::Allowed::Anything:
        break;
    case PrintQuestion::Allowed::Digits:
        if (!all([](QChar c) {
                return c.isDigit();
            })) {
            return u"%1: digits only."_s.arg(label);
        }
        break;
    case PrintQuestion::Allowed::Letters:
        if (!all([](QChar c) {
                return c.isLetter();
            })) {
            return u"%1: letters only."_s.arg(label);
        }
        break;
    case PrintQuestion::Allowed::LettersAndDigits:
        if (!all([](QChar c) {
                return c.isLetterOrNumber();
            })) {
            return u"%1: letters and digits only."_s.arg(label);
        }
        break;
    case PrintQuestion::Allowed::Pattern: {
        const QRegularExpression pattern(QRegularExpression::anchoredPattern(question.pattern));
        if (!pattern.isValid()) {
            return u"%1: the question's pattern is not valid (%2)."_s.arg(label, pattern.errorString());
        }
        if (!pattern.match(answer).hasMatch()) {
            return u"%1: not in the expected form."_s.arg(label);
        }
        break;
    }
    }
    return std::nullopt;
}

} // namespace Fields

} // namespace LabelCreator
