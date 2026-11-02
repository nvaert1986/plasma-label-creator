/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "automation.h"

#include <QDateTime>
#include <QHash>
#include <QObject>

class HttpApi;
class QFileSystemWatcher;
class QTimer;

/**
 * The automation service: prints the jobs dropped in the watched folders,
 * and, when built with Qt HTTP Server, the requests sent over HTTP.
 * Follows its settings file as it changes.
 */
class AutomationService : public QObject
{
    Q_OBJECT

public:
    explicit AutomationService(const QString &settingsFile, QObject *parent = nullptr);
    ~AutomationService() override;

    /** Reads the settings and starts watching and listening. */
    void start();
    /** Looks in every watched folder now: files that stopped growing are printed. */
    void scanFolders();

    const LabelCreator::AutomationSettings &settings() const;
    /**
     * Prints @p request: its label named in the labels folder, else
     * @p defaultLabel; its printer, else @p defaultPrinter, else the default.
     */
    LabelCreator::PrintOutcome print(const LabelCreator::PrintRequest &request, const QString &defaultLabel = {}, const QString &defaultPrinter = {});

    /** Whether the service can take requests over HTTP (built with Qt HTTP Server). */
    static bool hasHttp();
    /** The port requests over HTTP are taken on; 0 when they are not. */
    quint16 httpPort() const;

private:
    void reload();
    void processFile(const LabelCreator::AutomationSettings::Folder &folder, const QString &fileName);
    /** Moves a job done with into @p subfolder of its folder, with @p message beside it. */
    void putAway(const QString &fileName, const QString &subfolder, const QString &message);

    QString m_settingsFile;
    LabelCreator::AutomationSettings m_settings;
    QFileSystemWatcher *m_watcher;
    QTimer *m_reloadTimer;
    QTimer *m_scanTimer;
    // A file is printed once its size and time stay the same between looks.
    struct Seen {
        qint64 size = -1;
        QDateTime modified;
    };
    QHash<QString, Seen> m_seen;
    // Jobs that could not be put away, as they were then.
    QHash<QString, QDateTime> m_stuck;
    HttpApi *m_http = nullptr;
};
