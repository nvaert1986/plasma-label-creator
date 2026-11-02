/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "automation.h"

#include <QObject>

class AutomationService;
class QHttpServer;
class QHttpServerRequest;
class QHttpServerResponse;
class QTcpServer;

/**
 * Print requests over HTTP, for the automation service:
 *
 *   GET  /v1/status    the service and its settings
 *   GET  /v1/printers  the printers and how they are
 *   GET  /v1/labels    the labels in the labels folder, with their questions
 *   POST /v1/print     prints, see PrintRequest
 *
 * Every request carries the token from the settings
 * (Authorization: Bearer <token>); answers are JSON.
 */
class HttpApi : public QObject
{
    Q_OBJECT

public:
    HttpApi(AutomationService *service, QObject *parent = nullptr);
    ~HttpApi() override;

    /** Listens as @p settings say; why not, or empty when it does. */
    QString listen(const LabelCreator::AutomationSettings &settings);
    /** The port listened on (the one asked for, or the one chosen for port 0). */
    quint16 port() const;

private:
    bool authorised(const QHttpServerRequest &request) const;
    QHttpServerResponse status() const;
    QHttpServerResponse printers() const;
    QHttpServerResponse labels() const;
    QHttpServerResponse print(const QHttpServerRequest &request);

    AutomationService *m_service;
    QHttpServer *m_server;
    QTcpServer *m_tcp = nullptr;
};
