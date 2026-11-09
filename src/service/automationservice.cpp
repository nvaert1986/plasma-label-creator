/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "automationservice.h"

#include "datatable.h"

#include "labelcreator_config.h"
#if LABELCREATOR_HTTP_SERVER
#include "httpapi.h"
#endif

#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTimer>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// Jobs in a watched folder: data rows for the folder's label, or a job
// as a request would send it.
const QStringList jobPatterns = {u"*.csv"_s, u"*.txt"_s, u"*.tsv"_s, u"*.json"_s};

// A job file larger than this is not a label job.
constexpr qint64 MaxJobSize = 16 * 1024 * 1024;

} // namespace

AutomationService::AutomationService(const QString &settingsFile, QObject *parent)
    : QObject(parent)
    , m_settingsFile(settingsFile)
    , m_watcher(new QFileSystemWatcher(this))
    , m_reloadTimer(new QTimer(this))
    , m_scanTimer(new QTimer(this))
    , m_jobLog(Automation::jobLogFileName())
{
    // Editors and the app replace the file: its folder is watched too, and
    // a burst of changes is read once.
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(500);
    connect(m_reloadTimer, &QTimer::timeout, this, &AutomationService::reload);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, m_reloadTimer, qOverload<>(&QTimer::start));
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_reloadTimer, qOverload<>(&QTimer::start));
    // Folders on a network share send no notice of new files: looked in
    // every two seconds.
    m_scanTimer->setInterval(2000);
    connect(m_scanTimer, &QTimer::timeout, this, &AutomationService::scanFolders);
}

AutomationService::~AutomationService() = default;

bool AutomationService::hasHttp()
{
    return LABELCREATOR_HTTP_SERVER;
}

void AutomationService::setJobLog(const QString &fileName)
{
    m_jobLog = fileName;
}

quint16 AutomationService::httpPort() const
{
#if LABELCREATOR_HTTP_SERVER
    return m_http ? m_http->port() : 0;
#else
    return 0;
#endif
}

const AutomationSettings &AutomationService::settings() const
{
    return m_settings;
}

void AutomationService::start()
{
    QDir().mkpath(QFileInfo(m_settingsFile).absolutePath());
    m_watcher->addPath(QFileInfo(m_settingsFile).absolutePath());
    reload();
    m_scanTimer->start();
}

void AutomationService::reload()
{
    if (QFileInfo::exists(m_settingsFile) && !m_watcher->files().contains(m_settingsFile)) {
        m_watcher->addPath(m_settingsFile);
    }
    QString error;
    const AutomationSettings settings = AutomationSettings::load(m_settingsFile, &error);
    if (!error.isEmpty()) {
        qWarning("%s", qPrintable(error));
        return;
    }
    const bool httpChanged = settings.http != m_settings.http || settings.httpNetwork != m_settings.httpNetwork || settings.httpPort != m_settings.httpPort
        || (m_http == nullptr && settings.http);
    m_settings = settings;
    for (const AutomationSettings::Folder &folder : std::as_const(m_settings.folders)) {
        if (!QFileInfo(folder.folder).isDir()) {
            qWarning("Watched folder %s does not exist (yet).", qPrintable(folder.folder));
        }
    }
    qInfo("Watching %lld folder(s).", qlonglong(m_settings.folders.size()));

#if LABELCREATOR_HTTP_SERVER
    if (httpChanged) {
        delete m_http;
        m_http = nullptr;
        if (m_settings.http) {
            m_http = new HttpApi(this, this);
            if (const QString problem = m_http->listen(m_settings); !problem.isEmpty()) {
                qWarning("Requests over HTTP are off: %s", qPrintable(problem));
                delete m_http;
                m_http = nullptr;
            }
        }
    }
#else
    if (settings.http && httpChanged) {
        qWarning("Requests over HTTP are on in the settings, but this service was built without Qt HTTP Server.");
    }
#endif
}

void AutomationService::scanFolders()
{
    QSet<QString> present;
    QSet<QString> looked; // a folder set up twice is looked in once
    for (const AutomationSettings::Folder &folder : std::as_const(m_settings.folders)) {
        const QDir dir(folder.folder);
        if (!dir.exists() || looked.contains(dir.canonicalPath())) {
            continue;
        }
        looked.insert(dir.canonicalPath());
        const QFileInfoList files = dir.entryInfoList(jobPatterns, QDir::Files | QDir::Readable, QDir::Time | QDir::Reversed);
        for (const QFileInfo &file : files) {
            const QString path = file.absoluteFilePath();
            present.insert(path);
            if (const auto stuck = m_stuck.constFind(path); stuck != m_stuck.cend() && *stuck == file.lastModified()) {
                continue;
            }
            m_stuck.remove(path);
            Seen &seen = m_seen[path];
            // Still being written: wait for the next look.
            if (seen.size != file.size() || seen.modified != file.lastModified()) {
                seen = {file.size(), file.lastModified()};
                continue;
            }
            m_seen.remove(path);
            processFile(folder, path);
        }
    }
    // Forget files gone meanwhile.
    for (auto it = m_seen.begin(); it != m_seen.end();) {
        it = present.contains(it.key()) ? std::next(it) : m_seen.erase(it);
    }
    for (auto it = m_stuck.begin(); it != m_stuck.end();) {
        it = present.contains(it.key()) ? std::next(it) : m_stuck.erase(it);
    }
}

PrintOutcome AutomationService::print(const PrintRequest &request, const QString &source, const QString &defaultLabel, const QString &defaultPrinter)
{
    PrintOutcome outcome;
    QString labelFile = defaultLabel;
    PrintRequest job = request;
    if (job.printer.isEmpty()) {
        job.printer = defaultPrinter.isEmpty() ? m_settings.printer : defaultPrinter;
    }
    if (!request.label.isEmpty()) {
        const auto found = Automation::labelInFolder(m_settings.labelsFolder, request.label);
        if (!found) {
            outcome.status = PrintOutcome::Status::NotFound;
            outcome.message = m_settings.labelsFolder.isEmpty() ? i18n("No labels folder is set in the automation settings.")
                                                                : i18n("There is no label %1 in the labels folder.", request.label);
        }
        labelFile = found.value_or(QString());
    } else if (labelFile.isEmpty()) {
        outcome.status = PrintOutcome::Status::BadRequest;
        outcome.message = i18n("No label: name one.");
    }
    if (!labelFile.isEmpty()) {
        // The app's printer settings as they are now: a print offset or a
        // printer's model may have changed since the service started.
        PrintEnvironment environment = PrintEnvironment::fromAppConfig();
        environment.saveCounters = m_settings.saveCounters;
        outcome = Automation::print(labelFile, job, environment);
    }

    const QString name = labelFile.isEmpty() ? request.label : QFileInfo(labelFile).fileName();
    if (outcome.status == PrintOutcome::Status::Printed) {
        qInfo("%s: %s", qPrintable(name), qPrintable(outcome.message));
    } else {
        qWarning("%s: %s", qPrintable(name), qPrintable(outcome.message));
    }
    for (const QString &warning : std::as_const(outcome.warnings)) {
        qWarning("  %s", qPrintable(warning));
    }
    AutomationJob logged;
    logged.when = QDateTime::currentDateTime();
    logged.source = source;
    logged.label = name;
    logged.printer = job.printer;
    logged.printed = outcome.status == PrintOutcome::Status::Printed;
    logged.labels = outcome.labels;
    logged.job = outcome.job;
    logged.message = (QStringList{outcome.message} + outcome.warnings).join(u'\n');
    logJob(logged);
    return outcome;
}

void AutomationService::logJob(const AutomationJob &job)
{
    if (!Automation::logJob(job, m_jobLog)) {
        qWarning("Could not write the list of recent jobs, %s.", qPrintable(m_jobLog));
    }
}

void AutomationService::processFile(const AutomationSettings::Folder &folder, const QString &fileName)
{
    QFile file(fileName);
    const QString source = i18n("Folder: %1", fileName);
    const auto refuse = [&](const QString &reason) {
        AutomationJob logged;
        logged.when = QDateTime::currentDateTime();
        logged.source = source;
        logged.label = QFileInfo(folder.label).fileName();
        logged.printer = folder.printer.isEmpty() ? m_settings.printer : folder.printer;
        logged.message = reason;
        logJob(logged);
        qWarning("%s: %s", qPrintable(fileName), qPrintable(reason));
        putAway(fileName, u"failed"_s, reason);
    };
    if (file.size() > MaxJobSize || !file.open(QIODevice::ReadOnly)) {
        refuse(file.size() > MaxJobSize ? i18n("The file is too large for a print job.") : file.errorString());
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    QString error;
    std::optional<PrintRequest> request;
    if (fileName.endsWith(u".json"_s, Qt::CaseInsensitive)) {
        request = PrintRequest::fromJson(data, &error);
        // Without copies of its own, the folder's.
        if (request && !QJsonDocument::fromJson(data).object().contains("copies"_L1)) {
            request->copies = folder.copies;
        }
    } else {
        // Rows of data, with their column names on the first line.
        if (const auto table = Csv::parse(data, QChar(), true, &error)) {
            request = PrintRequest();
            request->copies = folder.copies;
            for (qsizetype row = 0; row < table->rows.size(); ++row) {
                request->rows << table->values(row);
            }
            if (request->rows.isEmpty()) {
                request.reset();
                error = i18n("The file has no rows below its column names.");
            }
        }
    }
    if (!request) {
        refuse(error);
        return;
    }
    const PrintOutcome outcome = print(*request, source, folder.label, folder.printer);
    QString message = outcome.message;
    for (const QString &warning : outcome.warnings) {
        message += u'\n' + warning;
    }
    putAway(fileName, outcome.status == PrintOutcome::Status::Printed ? u"printed"_s : u"failed"_s, outcome.status == PrintOutcome::Status::Printed ? QString() : message);
}

void AutomationService::putAway(const QString &fileName, const QString &subfolder, const QString &message)
{
    const QFileInfo info(fileName);
    QDir dir = info.absoluteDir();
    // A job that cannot be put away is left alone until it changes, so it
    // is not printed again.
    if (!dir.mkpath(subfolder)) {
        qWarning("Could not create %s in %s; %s is left as it is.", qPrintable(subfolder), qPrintable(dir.path()), qPrintable(info.fileName()));
        m_stuck.insert(fileName, info.lastModified());
        return;
    }
    dir.cd(subfolder);
    // Never over an earlier job of the same name.
    QString target = dir.filePath(info.fileName());
    for (int n = 2; QFileInfo::exists(target); ++n) {
        target = dir.filePath(u"%1-%2.%3"_s.arg(info.completeBaseName()).arg(n).arg(info.suffix()));
    }
    if (!QFile::rename(fileName, target)) {
        qWarning("Could not move %s to %s; it is left as it is.", qPrintable(fileName), qPrintable(target));
        m_stuck.insert(fileName, info.lastModified());
        return;
    }
    if (!message.isEmpty()) {
        QSaveFile reason(target + u".txt"_s);
        if (reason.open(QIODevice::WriteOnly)) {
            reason.write(message.toUtf8() + '\n');
            reason.commit();
        }
    }
}
