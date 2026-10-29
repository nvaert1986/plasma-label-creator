/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace LabelCreator
{

/** A label kept in the recently used layouts. */
struct RecentLayout {
    QString title;
    QString fileName; // the copy, a .label file
    QString thumbnail; // a .png next to it
    QString source; // the file it was printed from; empty for unsaved labels
    QDateTime used;
};

/**
 * DLS's "Recently used layouts": a copy of every label printed, newest
 * first, with a thumbnail to choose it by. A label printed again moves to
 * the front instead of being kept twice: the same file, or the same label.
 */
class RecentLayouts
{
public:
    explicit RecentLayouts(const QString &directory = defaultDirectory());

    /** Where the app keeps them: recent-layouts in its data directory. */
    static QString defaultDirectory();
    static constexpr int MaxLayouts = 20;
    static constexpr int ThumbnailSize = 160; // pixels, the longer side

    /** Remembers @p document, printed from @p sourceFile (empty if unsaved). */
    bool add(const LabelDocument &document, const QString &sourceFile, QString *errorString = nullptr);
    /** Newest first. */
    QList<RecentLayout> layouts() const;
    std::optional<LabelDocument> load(const RecentLayout &layout, QString *errorString = nullptr) const;
    void remove(const RecentLayout &layout);
    void clear();

private:
    QList<RecentLayout> readIndex() const;
    bool writeIndex(const QList<RecentLayout> &layouts, QString *errorString) const;

    QString m_directory;
};

} // namespace LabelCreator
