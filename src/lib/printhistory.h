/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace LabelCreator
{

/** One print: what, where, how many, and with which answers and rows. */
struct PrintRecord {
    QDateTime when;
    QString title; // the label's file name, or its label type when unsaved
    QString source; // the label file printed from; empty when unsaved
    QString labelCopy; // the label as printed, kept with the history
    QString printer;
    int labels = 0; // printed, copies included
    int job = 0; // the CUPS job
    QHash<QString, QString> answers;
    QList<QHash<QString, QString>> rows; // the data rows printed, if any

    bool operator==(const PrintRecord &) const = default;
};

/**
 * What was printed, for this user: a line per print in a file in the app's
 * data folder, and a copy of each label printed, kept once however often it
 * is printed.
 */
class PrintHistory
{
public:
    explicit PrintHistory(const QString &directory = defaultDirectory());

    /** print-history in the app's data folder. */
    static QString defaultDirectory();
    /** The history keeps this many prints by default; older ones go. */
    static constexpr int MaxRecords = 5000;
    void setMaxRecords(int count);

    /** Adds @p record, keeping @p label as printed (its labelCopy is set here). */
    bool add(PrintRecord record, const LabelDocument &label, QString *errorString = nullptr);
    /** Newest first. */
    QList<PrintRecord> records() const;
    std::optional<LabelDocument> label(const PrintRecord &record, QString *errorString = nullptr) const;
    void clear();

private:
    QString historyFile() const;

    QString m_directory;
    int m_maxRecords = MaxRecords;
};

} // namespace LabelCreator
