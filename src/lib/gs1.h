/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace LabelCreator
{

/**
 * GS1 data as GS1-128, GS1 Data Matrix and GS1 DataBar Expanded carry it:
 * application identifiers (AIs) with their values, written
 * [01]09501101530003[17]261231[10]ABC123.
 */
namespace Gs1
{

/** An application identifier and the form of its value. */
struct Identifier {
    QString ai; // "01"
    QString title; // "GTIN"
    int minimumLength = 1;
    int maximumLength = 1;
    bool numeric = false; // digits only; else GS1's 82 characters
    int numericPrefix = 0; // the first n characters are digits (AI 421)
    bool checkDigit = false; // the last digit is a GS1 check digit
    bool date = false; // YYMMDD
    bool dateTime = false; // YYMMDDHHMM

    /** "N14", "X..20", "N6 (YYMMDD)": as GS1 writes the format. */
    QString format() const;
};

/** The application identifiers this program knows, the most used first. */
const QList<Identifier> &identifiers();
/** The identifier @p ai, or nullopt when it is not known. */
std::optional<Identifier> identifier(const QString &ai);

struct Element {
    QString ai;
    QString value;
    bool operator==(const Element &) const = default;
};

/**
 * The elements of @p data written with [AI] brackets; nullopt when it is not
 * written that way (an empty text has no elements).
 */
std::optional<QList<Element>> parse(const QString &data);
/** The elements written with [AI] brackets. */
QString compose(const QList<Element> &elements);

/**
 * What is wrong with @p element; nullopt when it is right. A value with a
 * field ({Batch}) is only checked when printing, so it passes here.
 */
std::optional<QString> problem(const Element &element);

/** The GS1 check digit for @p digits (a GTIN or SSCC without its last digit). */
QChar checkDigit(const QString &digits);

} // namespace Gs1

} // namespace LabelCreator
