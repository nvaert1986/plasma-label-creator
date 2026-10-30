/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "sharedcounter.h"

#include <QFile>
#include <QSaveFile>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

SharedCounter::SharedCounter(const QString &fileName)
    : m_fileName(fileName)
    , m_lock(fileName + u".lock"_s)
{
    // A computer that crashed while printing leaves its lock behind; on a
    // share its process cannot be checked, so a lock this old is taken over.
    m_lock.setStaleLockTime(120 * 1000);
}

bool SharedCounter::lock(int timeoutMs)
{
    if (m_lock.tryLock(timeoutMs)) {
        return true;
    }
    switch (m_lock.error()) {
    case QLockFile::LockFailedError:
        m_errorString = u"%1 is in use by another computer printing this label."_s.arg(m_fileName);
        break;
    case QLockFile::PermissionError:
        m_errorString = u"No permission to lock %1."_s.arg(m_fileName);
        break;
    default:
        m_errorString = u"Could not lock %1."_s.arg(m_fileName);
        break;
    }
    return false;
}

void SharedCounter::unlock()
{
    m_lock.unlock();
}

std::optional<int> SharedCounter::read(int fallback)
{
    QFile file(m_fileName);
    if (!file.exists()) {
        return fallback;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_errorString = u"Could not read %1: %2"_s.arg(m_fileName, file.errorString());
        return std::nullopt;
    }
    bool ok = false;
    const int value = QString::fromUtf8(file.readAll()).trimmed().toInt(&ok);
    if (!ok) {
        m_errorString = u"%1 does not hold a number."_s.arg(m_fileName);
        return std::nullopt;
    }
    return value;
}

bool SharedCounter::write(int value)
{
    // Whole or not at all: a half-written file would stop every computer.
    QSaveFile file(m_fileName);
    if (!file.open(QIODevice::WriteOnly) || file.write(QByteArray::number(value) + '\n') < 0 || !file.commit()) {
        m_errorString = u"Could not write %1: %2"_s.arg(m_fileName, file.errorString());
        return false;
    }
    return true;
}

QString SharedCounter::errorString() const
{
    return m_errorString;
}

} // namespace LabelCreator
