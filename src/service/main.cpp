/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "automationservice.h"
#include "labelcreator_version.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLockFile>
#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

int signalSockets[2] = {-1, -1};

void onSignal(int)
{
    const char quit = 1;
    // Only a write is safe in a signal handler; the event loop does the rest.
    [[maybe_unused]] const auto written = ::write(signalSockets[0], &quit, 1);
}

} // namespace

int main(int argc, char *argv[])
{
    // Labels are drawn without a screen; a desktop session is not needed.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QGuiApplication app(argc, argv);
    // The app's name: the print history is the app's.
    QCoreApplication::setApplicationName(u"plasma-label-creator"_s);
    QCoreApplication::setApplicationVersion(QStringLiteral(LABELCREATOR_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        u"Prints labels for jobs dropped in watched folders and, when built with Qt HTTP Server, for requests over HTTP. "
        "Set it up in Plasma Label Creator: Settings > Configure Plasma Label Creator > Automation."_s);
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption settingsOption(u"settings"_s, u"The settings file (default %1)."_s.arg(AutomationSettings::defaultFileName()), u"file"_s);
    const QCommandLineOption featuresOption(u"features"_s, u"Show what this build can do and exit."_s);
    parser.addOptions({settingsOption, featuresOption});
    parser.process(app);

    if (parser.isSet(featuresOption)) {
        fprintf(stdout, "folders: yes\nhttp: %s\n", AutomationService::hasHttp() ? "yes" : "no");
        return 0;
    }

    // Once per user: two would print every job twice.
    QLockFile lock(Automation::lockFileName());
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        lock.getLockInfo(&pid, nullptr, nullptr);
        fprintf(stderr, "The automation service is already running (process %lld).\n", qlonglong(pid));
        return 1;
    }

    // Stopped with SIGTERM (systemd, the app's Stop button) or Ctrl+C: the
    // job being printed is finished first.
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalSockets) == 0) {
        auto *notifier = new QSocketNotifier(signalSockets[1], QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, &app, [notifier]() {
            notifier->setEnabled(false);
            char quit = 0;
            [[maybe_unused]] const auto bytes = ::read(signalSockets[1], &quit, 1);
            qInfo("Stopping.");
            QCoreApplication::quit();
        });
        struct sigaction action = {};
        action.sa_handler = onSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &action, nullptr);
        sigaction(SIGINT, &action, nullptr);
    }

    AutomationService service(parser.isSet(settingsOption) ? parser.value(settingsOption) : AutomationSettings::defaultFileName());
    service.start();
    return app.exec();
}
