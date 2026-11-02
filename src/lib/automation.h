/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <optional>

namespace LabelCreator
{

/**
 * What the automation service watches and how it takes requests. Written
 * by the app's settings, read by the service, as JSON in the user's
 * configuration folder (readable by the user only: it holds the token).
 */
struct AutomationSettings {
    /** A folder whose files are print jobs for one label. */
    struct Folder {
        QString folder;
        QString label; // the .label file each job prints
        QString printer; // empty: the default printer below
        int copies = 1;
        bool operator==(const Folder &) const = default;
    };

    QList<Folder> folders;
    /** Labels that jobs and requests may name, relative to this folder. */
    QString labelsFolder;
    /** The printer when a job names none. */
    QString printer;
    /** Counters on a label count on in the label file after printing. */
    bool saveCounters = true;

    bool http = false;
    /** Listening on every network interface; else this computer only. */
    bool httpNetwork = false;
    int httpPort = 8631;
    /** Every request carries it: Authorization: Bearer <token>. */
    QString httpToken;

    bool operator==(const AutomationSettings &) const = default;

    static QString defaultFileName();
    /** The settings in @p fileName; the defaults when there is no file. */
    static AutomationSettings load(const QString &fileName = defaultFileName(), QString *errorString = nullptr);
    bool save(const QString &fileName = defaultFileName(), QString *errorString = nullptr) const;
    /** A new random token for HTTP requests. */
    static QString newToken();
};

/** A print job: a request over HTTP, or a file dropped in a watched folder. */
struct PrintRequest {
    QString label; // as named: relative to the labels folder
    QString printer; // empty: the folder's or the default
    int copies = 1;
    /** Answers to the label's questions, and other {fields}: on every label. */
    QHash<QString, QString> values;
    /** A label per row, each with its own fields. */
    QList<QHash<QString, QString>> rows;
    /** Print even when the queue reports a problem, such as being stopped. */
    bool printAnyway = false;

    /**
     * From JSON: {"label": "...", "printer": "...", "copies": 1,
     * "values": {...}, "rows": [{...}], "printAnyway": false}.
     */
    static std::optional<PrintRequest> fromJson(const QByteArray &json, QString *errorString = nullptr);
};

/** What became of a print job. */
struct PrintOutcome {
    enum class Status {
        Printed,
        BadRequest, // the job or its answers do not do
        NotFound, // no such label or printer
        NotReady, // the printer cannot take the label, or reports a problem
        Failed, // printing itself failed
    };
    Status status = Status::Failed;
    QString message; // what went wrong, or what was sent
    int job = 0;
    int labels = 0;
    QStringList warnings;
};

/**
 * The app's settings that printing follows: which LabelWriter a queue
 * without DYMO's driver prints to, print offsets, how long to wait for a
 * shared counter, and how many prints the history keeps.
 */
struct PrintEnvironment {
    QHash<QString, QString> drivers; // queue → LabelWriter driver, such as lw450t.ppd
    QHash<QString, QPointF> offsetsMm; // queue → across the head, along the feed
    int counterWaitSeconds = 10;
    int historyKept = 5000; // 0: no history
    QString historyDirectory; // empty: the app's
    bool saveCounters = true;

    /**
     * From the app's settings file (plasma-label-creatorrc), the system's
     * first; values an administrator fixed ([$i]) stay.
     */
    static PrintEnvironment fromAppConfig();
};

namespace Automation
{

/** The service's lock file: held while it runs, so it runs once per user. */
QString lockFileName();
/** The running service's process id; 0 when it does not run. */
qint64 servicePid();

/** The labels in @p labelsFolder and its subfolders, as requests name them (Rack/patch.label). */
QStringList labelNames(const QString &labelsFolder);

/**
 * The file of the label @p name in @p labelsFolder: a .label file inside
 * that folder (subfolders too), never outside it; nullopt otherwise.
 */
std::optional<QString> labelInFolder(const QString &labelsFolder, const QString &name);

/**
 * The answers to @p document's questions from @p values, as the form
 * would give them: defaults for those not given (today for a date), check
 * boxes as their texts, numbers in the question's decimals. Nullopt and
 * @p errorString when one does not do.
 */
std::optional<QHash<QString, QString>> answers(const LabelDocument &document, const QHash<QString, QString> &values, QString *errorString = nullptr);

/**
 * The labels to print for @p request: one per row, else one, with the
 * answers and values filled in, and pictures with relative paths found
 * next to @p labelFile.
 */
std::optional<QList<LabelDocument>> prepare(const LabelDocument &document, const QString &labelFile, const PrintRequest &request, QString *errorString = nullptr);

/**
 * Prints @p request with the label in @p labelFile, as the app's Print
 * button would: shared counters locked, the printer checked, counters
 * moved on and the print recorded in the history.
 */
PrintOutcome print(const QString &labelFile, const PrintRequest &request, const PrintEnvironment &environment);

} // namespace Automation

} // namespace LabelCreator
