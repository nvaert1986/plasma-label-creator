/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printersetup.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>

using namespace Qt::Literals::StringLiterals;

namespace PrinterSetup
{

namespace
{

const QString Service = u"org.opensuse.CupsPkHelper.Mechanism"_s;
// The password prompt can take a while.
constexpr int Timeout = 5 * 60 * 1000;

// Calls a cups-pk-helper method, which answers with an error text that is
// empty on success, then continues with @p next.
void call(QObject *context, const QString &method, const QVariantList &arguments, const std::function<void(const QString &error)> &next)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, u"/"_s, Service, method);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, Timeout), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, next]() {
        watcher->deleteLater();
        const QDBusPendingReply<QString> reply = *watcher;
        if (reply.isError()) {
            next(reply.error().name() == u"org.freedesktop.DBus.Error.ServiceUnknown"_s
                     ? i18n("cups-pk-helper is not installed, so printers cannot be set up from here. Use the system's printer settings instead.")
                     : reply.error().message());
        } else {
            next(reply.value());
        }
    });
}

} // namespace

QString queueName(const QString &product, const QStringList &taken)
{
    QString base = product.startsWith(u"DYMO"_s, Qt::CaseInsensitive) ? product : u"DYMO "_s + product;
    // CUPS allows letters, digits, dashes and underscores.
    base.replace(QRegularExpression(u"[^A-Za-z0-9_-]+"_s), u"-"_s);
    QString name = base;
    for (int i = 2; taken.contains(name, Qt::CaseInsensitive); ++i) {
        name = base + u'-' + QString::number(i);
    }
    return name;
}

void addQueue(QObject *context,
              const QString &name,
              const QString &deviceUri,
              const QString &ppdName,
              const QString &description,
              const std::function<void(const QString &error)> &done)
{
    call(context, u"PrinterAdd"_s, {name, deviceUri, ppdName, description, QString()}, [context, name, done](const QString &error) {
        if (!error.isEmpty()) {
            done(error);
            return;
        }
        // New queues may start disabled and refusing jobs.
        call(context, u"PrinterSetEnabled"_s, {name, true}, [context, name, done](const QString &error) {
            if (!error.isEmpty()) {
                done(error);
                return;
            }
            call(context, u"PrinterSetAcceptJobs"_s, {name, true, QString()}, done);
        });
    });
}

} // namespace PrinterSetup
