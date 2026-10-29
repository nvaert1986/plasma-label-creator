/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QStringList>

#include <optional>

class QIODevice;

namespace LabelCreator
{

/**
 * Reads DYMO Label Software 8 `.label` files (DieCutLabel, version 8.0).
 * Continuous (tape) labels are recognised but not supported yet.
 */
class LabelReader
{
public:
    std::optional<LabelDocument> read(QIODevice *device);
    std::optional<LabelDocument> readFile(const QString &fileName);

    /** Why the last read failed. */
    QString errorString() const;
    /** Things the last read skipped or did not understand, in file order. */
    QStringList warnings() const;

private:
    QString m_errorString;
    QStringList m_warnings;
};

/** Writes the `.label` format in the element order DYMO Label Software uses. */
class LabelWriter
{
public:
    static bool write(const LabelDocument &document, QIODevice *device);
    static bool writeFile(const LabelDocument &document, const QString &fileName, QString *errorString = nullptr);
    static QByteArray toByteArray(const LabelDocument &document);
};

} // namespace LabelCreator
