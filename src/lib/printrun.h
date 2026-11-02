/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace LabelCreator
{

/**
 * What one print run prints, worked out the same way for the Print button,
 * the print preview, print-only mode and the automation service: a label
 * per data row (as many as the row's copies column says, with the label its
 * label column names), or one; each filled in, objects whose condition does
 * not hold left out; and a header and trailer label around them.
 */
namespace PrintRun
{

struct Run {
    QList<LabelDocument> labels;
    /** Copies of each label, for the printer; 1 when they were laid out here (header, trailer). */
    int copies = 1;
    /** The first and last labels are the header and trailer: not counted, counters do not move on for them. */
    int headerLabels = 0;
    int trailerLabels = 0;
};

struct Request {
    LabelDocument document;
    /** Where the label is: other labels and pictures named without a folder are next to it. */
    QString labelFile;
    /** Where pictures named without a folder are; empty: next to the label. */
    QString pictureFolder;
    /** On every label, below a row's own values: values given for the whole run. */
    QHash<QString, QString> values;
    /** On every label, above a row's values: the answers to the questions. */
    QHash<QString, QString> answers;
    /** A label per row; none: one label. */
    QList<QHash<QString, QString>> rows;
    /** Labels already made (one per address, say) instead of rows. */
    QList<LabelDocument> prepared;
    int copies = 1;
};

/** The labels @p request prints; nullopt and @p errorString when it cannot be. */
std::optional<Run> make(const Request &request, QString *errorString = nullptr);

/** @p document with @p values filled in, and pictures named without a folder found in @p pictureFolder. */
LabelDocument filled(const LabelDocument &document, const QHash<QString, QString> &values, const QString &pictureFolder);

/** The file another label is named as, next to @p labelFile unless a full path; empty when it cannot be found. */
QString labelFileFor(const QString &name, const QString &labelFile);

/** Whether @p a and @p b print on the same labels: one run takes one label type. */
bool sameLabelType(const LabelDocument &a, const LabelDocument &b);

/** The data columns @p document uses beyond its fields: copies and label per row. */
QStringList runColumns(const LabelDocument &document);

} // namespace PrintRun

} // namespace LabelCreator
