/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace LabelCreator
{

/**
 * Earlier versions of saved labels: before a label file is saved over, the
 * file as it was is kept here, per label, newest first. Kept in the app's
 * data folder, for this user, so label folders stay as they are.
 */
class LabelVersions
{
public:
    struct Version {
        QString fileName; // the copy, a .label file
        QDateTime saved; // when it was kept, so when it was saved over
    };

    explicit LabelVersions(const QString &directory = defaultDirectory());

    /** versions in the app's data folder. */
    static QString defaultDirectory();
    static constexpr int DefaultKeep = 20;

    /**
     * Keeps @p labelFile as it is on disk now, when it exists and differs
     * from the newest version; at most @p keep versions stay.
     */
    bool keep(const QString &labelFile, int keep = DefaultKeep, QString *errorString = nullptr);
    /** The versions of @p labelFile, newest first. */
    QList<Version> versions(const QString &labelFile) const;

private:
    /** The folder for @p labelFile's versions: one per label, by its path. */
    QString folderFor(const QString &labelFile) const;

    QString m_directory;
};

} // namespace LabelCreator
