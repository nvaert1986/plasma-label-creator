/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "fields.h"

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
        [&names](const QString &name) {
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

QString fill(const QString &text, const QHash<QString, QString> &values)
{
    QString result;
    scan(
        text,
        [&result](const QString &part) {
            result += part;
        },
        [&result, &values](const QString &name) {
            if (const auto it = values.constFind(name); it != values.constEnd()) {
                result += *it;
                return;
            }
            for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
                if (it.key().compare(name, Qt::CaseInsensitive) == 0) {
                    result += it.value();
                    return;
                }
            }
            result += u'{' + name + u'}';
        });
    return result;
}

LabelDocument fill(const LabelDocument &document, const QHash<QString, QString> &values)
{
    LabelDocument filled = document;
    forEachText(filled, [&values](QString &text) {
        text = fill(text, values);
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
