/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelversions.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

const QString TimeFormat = u"yyyyMMdd-HHmmsszzz"_s;

QByteArray contents(const QString &fileName)
{
    QFile file(fileName);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

LabelVersions::LabelVersions(const QString &directory)
    : m_directory(directory)
{
}

LabelVersions LabelVersions::nextToLabels()
{
    return LabelVersions(QString());
}

QString LabelVersions::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/versions"_s;
}

QString LabelVersions::folderFor(const QString &labelFile) const
{
    if (m_directory.isEmpty()) {
        const QFileInfo label(labelFile);
        return label.absolutePath() + u"/.versions/"_s + label.fileName();
    }
    // The label's path, hashed: any path makes a valid folder name.
    const QByteArray path = QFileInfo(labelFile).absoluteFilePath().toUtf8();
    return m_directory + u'/' + QString::fromLatin1(QCryptographicHash::hash(path, QCryptographicHash::Sha1).toHex());
}

bool LabelVersions::keep(const QString &labelFile, int keep, QString *errorString)
{
    const auto fail = [errorString](const QString &message) {
        if (errorString) {
            *errorString = message;
        }
        return false;
    };
    if (!QFileInfo::exists(labelFile)) {
        return true; // a new file: nothing to keep
    }
    const QByteArray now = contents(labelFile);
    const QList<Version> earlier = versions(labelFile);
    if (!earlier.isEmpty() && contents(earlier.constFirst().fileName) == now) {
        return true; // saved without changes
    }
    const QString folder = folderFor(labelFile);
    if (!QDir().mkpath(folder)) {
        return fail(u"Could not make %1"_s.arg(folder));
    }
    // Which label the folder is for, for whoever looks in it.
    QFile source(folder + u"/source.txt"_s);
    if (source.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        source.write(QFileInfo(labelFile).absoluteFilePath().toUtf8() + '\n');
    }
    QFile copy(folder + u'/' + QDateTime::currentDateTime().toString(TimeFormat) + u".label"_s);
    if (!copy.open(QIODevice::WriteOnly) || copy.write(now) != now.size()) {
        return fail(u"Could not keep a version of %1: %2"_s.arg(labelFile, copy.errorString()));
    }
    copy.close();
    // The oldest go.
    const QList<Version> all = versions(labelFile);
    for (qsizetype i = std::max(1, keep); i < all.size(); ++i) {
        QFile::remove(all.at(i).fileName);
    }
    return true;
}

QList<LabelVersions::Version> LabelVersions::versions(const QString &labelFile) const
{
    QList<Version> list;
    const QDir folder(folderFor(labelFile));
    // Names are times: sorted by name, newest first.
    for (const QFileInfo &file : folder.entryInfoList({u"*.label"_s}, QDir::Files, QDir::Name | QDir::Reversed)) {
        const QDateTime saved = QDateTime::fromString(file.completeBaseName(), TimeFormat);
        list.append({file.absoluteFilePath(), saved.isValid() ? saved : file.lastModified()});
    }
    return list;
}

} // namespace LabelCreator
