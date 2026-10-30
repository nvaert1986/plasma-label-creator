/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QLockFile>
#include <QString>

#include <optional>

namespace LabelCreator
{

/**
 * A counter kept in a file, on a network share say, so that several
 * computers printing the same label count on from each other. The file
 * holds the next value as text; a lock file next to it (name.lock) keeps
 * two computers from printing the same numbers.
 */
class SharedCounter
{
public:
    explicit SharedCounter(const QString &fileName);

    /** Waits up to @p timeoutMs for another computer to finish printing. */
    bool lock(int timeoutMs = 10000);
    void unlock();

    /** The next value; @p fallback when there is no file yet, nullopt when the file cannot be read. */
    std::optional<int> read(int fallback);
    bool write(int value);

    QString errorString() const;

private:
    QString m_fileName;
    QLockFile m_lock;
    QString m_errorString;
};

} // namespace LabelCreator
