/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace LabelCreator
{

struct AddressEntry {
    /** The address as it goes on the label, one line per line. */
    QString address;
    QString firstName;
    QString lastName;
    /** How the entry is sorted and listed, e.g. "Mantegna, Andrea". */
    QString fileAs;

    /** fileAs, or the first line of the address when it is empty. */
    QString displayName() const;
    /** Ways to file this entry: "Last, First", "First Last", the company line... */
    QStringList fileAsSuggestions() const;

    bool operator==(const AddressEntry &) const = default;
};

/** A DLS 8 address book (.ABX), sorted by fileAs as DLS lists it. */
class AddressBook
{
public:
    QList<AddressEntry> entries;
    bool defaultPermutation = true;

    static std::optional<AddressBook> load(const QString &fileName, QString *errorString = nullptr);
    bool save(const QString &fileName, QString *errorString = nullptr) const;

    /** Sorts the entries by display name, ignoring case and accents' order. */
    void sort();

    /**
     * Reads addresses from a CSV file. With a header row naming the columns
     * (name, company, street, postcode, city, country, in English or Dutch)
     * the address is put together in that order; otherwise every non-empty
     * field of a row becomes a line. The delimiter is detected.
     */
    static QList<AddressEntry> importCsv(const QString &fileName, QString *errorString = nullptr);
    /** As importCsv(), from text. */
    static QList<AddressEntry> parseCsv(const QString &text);

    /** Splits "Gábor Candrás" into first and last name, the way DLS files it. */
    static AddressEntry fromAddress(const QString &address);
};

} // namespace LabelCreator
