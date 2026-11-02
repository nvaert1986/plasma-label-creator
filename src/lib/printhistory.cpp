/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "printhistory.h"

#include "labelformat.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

QJsonObject toJson(const QHash<QString, QString> &values)
{
    QJsonObject object;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        object[it.key()] = it.value();
    }
    return object;
}

QHash<QString, QString> fromJson(const QJsonObject &object)
{
    QHash<QString, QString> values;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        values.insert(it.key(), it.value().toString());
    }
    return values;
}

QByteArray toLine(const PrintRecord &record)
{
    QJsonObject line;
    line[u"when"_s] = record.when.toString(Qt::ISODateWithMs);
    line[u"title"_s] = record.title;
    line[u"source"_s] = record.source;
    line[u"label"_s] = record.labelCopy;
    line[u"printer"_s] = record.printer;
    line[u"labels"_s] = record.labels;
    line[u"job"_s] = record.job;
    line[u"answers"_s] = toJson(record.answers);
    QJsonArray rows;
    for (const auto &row : record.rows) {
        rows.append(toJson(row));
    }
    line[u"rows"_s] = rows;
    return QJsonDocument(line).toJson(QJsonDocument::Compact) + '\n';
}

PrintRecord fromLine(const QJsonObject &line)
{
    PrintRecord record;
    record.when = QDateTime::fromString(line[u"when"_s].toString(), Qt::ISODateWithMs);
    record.title = line[u"title"_s].toString();
    record.source = line[u"source"_s].toString();
    record.labelCopy = line[u"label"_s].toString();
    record.printer = line[u"printer"_s].toString();
    record.labels = line[u"labels"_s].toInt();
    record.job = line[u"job"_s].toInt();
    record.answers = fromJson(line[u"answers"_s].toObject());
    for (const auto &row : line[u"rows"_s].toArray()) {
        record.rows << fromJson(row.toObject());
    }
    return record;
}

} // namespace

PrintHistory::PrintHistory(const QString &directory)
    : m_directory(directory)
{
}

void PrintHistory::setMaxRecords(int count)
{
    m_maxRecords = count;
}

QString PrintHistory::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/print-history"_s;
}

QString PrintHistory::historyFile() const
{
    return m_directory + u"/history.jsonl"_s;
}

bool PrintHistory::add(PrintRecord record, const LabelDocument &label, QString *errorString)
{
    const auto fail = [errorString](const QString &message) {
        if (errorString) {
            *errorString = message;
        }
        return false;
    };
    if (!QDir().mkpath(m_directory + u"/labels"_s)) {
        return fail(u"Could not make %1"_s.arg(m_directory));
    }
    // The label as printed, once per content: a label printed a hundred
    // times is kept once.
    const QByteArray xml = LabelWriter::toByteArray(label);
    const QString copy = u"labels/"_s + QString::fromLatin1(QCryptographicHash::hash(xml, QCryptographicHash::Sha1).toHex()) + u".label"_s;
    if (!QFile::exists(m_directory + u'/' + copy)) {
        QSaveFile file(m_directory + u'/' + copy);
        if (!file.open(QIODevice::WriteOnly) || file.write(xml) != xml.size() || !file.commit()) {
            return fail(u"Could not keep the printed label: %1"_s.arg(file.errorString()));
        }
    }
    record.labelCopy = copy;

    QFile history(historyFile());
    if (!history.open(QIODevice::Append)) {
        return fail(u"Could not write the print history: %1"_s.arg(history.errorString()));
    }
    history.write(toLine(record));
    history.close();

    // Now and then the oldest prints go, and labels nothing refers to.
    QList<PrintRecord> all = records();
    const int keep = std::max(1, m_maxRecords);
    if (all.size() > keep + keep / 10) {
        all = all.mid(0, keep);
        QSaveFile trimmed(historyFile());
        if (trimmed.open(QIODevice::WriteOnly)) {
            QStringList used;
            for (auto it = all.crbegin(); it != all.crend(); ++it) {
                trimmed.write(toLine(*it));
                used << it->labelCopy;
            }
            trimmed.commit();
            const QDir labels(m_directory + u"/labels"_s);
            for (const QString &name : labels.entryList({u"*.label"_s}, QDir::Files)) {
                if (!used.contains(u"labels/"_s + name)) {
                    QFile::remove(labels.filePath(name));
                }
            }
        }
    }
    return true;
}

QList<PrintRecord> PrintHistory::records() const
{
    QList<PrintRecord> list;
    QFile history(historyFile());
    if (!history.open(QIODevice::ReadOnly)) {
        return list;
    }
    while (!history.atEnd()) {
        const QJsonDocument line = QJsonDocument::fromJson(history.readLine());
        if (line.isObject()) {
            list.prepend(fromLine(line.object()));
        }
    }
    return list;
}

std::optional<LabelDocument> PrintHistory::label(const PrintRecord &record, QString *errorString) const
{
    LabelReader reader;
    const auto document = reader.readFile(m_directory + u'/' + record.labelCopy);
    if (!document && errorString) {
        *errorString = reader.errorString();
    }
    return document;
}

void PrintHistory::clear()
{
    QDir(m_directory).removeRecursively();
}

} // namespace LabelCreator
