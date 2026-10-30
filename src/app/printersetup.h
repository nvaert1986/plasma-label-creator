/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QObject;

/** Setting up CUPS queues, for which CUPS wants an administrator. */
namespace PrinterSetup
{

/**
 * A CUPS queue name for @p product ("LabelWriter 450 Turbo" becomes
 * "DYMO-LabelWriter-450-Turbo"), not one of @p taken.
 */
QString queueName(const QString &product, const QStringList &taken);

/**
 * Creates, enables and opens the queue @p name for the printer at
 * @p deviceUri with driver @p ppdName, through cups-pk-helper, which asks
 * for the administrator password itself. Asynchronous: @p done is called
 * with an empty string on success, else with the error. @p context ends the
 * call if it is destroyed first.
 */
void addQueue(QObject *context,
              const QString &name,
              const QString &deviceUri,
              const QString &ppdName,
              const QString &description,
              const std::function<void(const QString &error)> &done);

} // namespace PrinterSetup
