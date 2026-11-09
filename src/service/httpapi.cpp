/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "httpapi.h"

#include "automationservice.h"
#include "fields.h"
#include "labelcreator_version.h"
#include "labelformat.h"
#include "labelprinter.h"

#include <KLocalizedString>
#include <QHttpServer>
#include <QHttpServerConfiguration>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// A print request is a few fields and perhaps rows of data; more is not one.
constexpr qint64 MaxBodySize = 4 * 1024 * 1024;

QHttpServerResponse error(QHttpServerResponse::StatusCode code, const QString &message)
{
    return QHttpServerResponse(QJsonObject{{"error"_L1, message}}, code);
}

QString kindName(PrintQuestion::Kind kind)
{
    switch (kind) {
    case PrintQuestion::Kind::Text:
        return u"text"_s;
    case PrintQuestion::Kind::Choice:
        return u"choice"_s;
    case PrintQuestion::Kind::Number:
        return u"number"_s;
    case PrintQuestion::Kind::Date:
        return u"date"_s;
    case PrintQuestion::Kind::Check:
        return u"check"_s;
    }
    return u"text"_s;
}

// The same length and bytes, without telling by its timing how much of a
// guess was right.
bool sameToken(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size() || a.isEmpty()) {
        return false;
    }
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i) {
        difference |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    }
    return difference == 0;
}

} // namespace

HttpApi::HttpApi(AutomationService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_server(new QHttpServer(this))
{
    const auto guarded = [this](auto handler) {
        return [this, handler](const QHttpServerRequest &request) {
            if (!authorised(request)) {
                QHttpServerResponse response = error(QHttpServerResponse::StatusCode::Unauthorized, i18n("A valid token is needed: Authorization: Bearer <token>."));
                QHttpHeaders headers = response.headers();
                headers.append(QHttpHeaders::WellKnownHeader::WWWAuthenticate, "Bearer");
                response.setHeaders(std::move(headers));
                return response;
            }
            return handler(request);
        };
    };
    m_server->route(u"/v1/status"_s, QHttpServerRequest::Method::Get, guarded([this](const QHttpServerRequest &) {
                        return status();
                    }));
    m_server->route(u"/v1/printers"_s, QHttpServerRequest::Method::Get, guarded([this](const QHttpServerRequest &) {
                        return printers();
                    }));
    m_server->route(u"/v1/labels"_s, QHttpServerRequest::Method::Get, guarded([this](const QHttpServerRequest &) {
                        return labels();
                    }));
    m_server->route(u"/v1/print"_s, QHttpServerRequest::Method::Post, guarded([this](const QHttpServerRequest &request) {
                        return print(request);
                    }));
    m_server->setMissingHandler(this, [](const QHttpServerRequest &, QHttpServerResponder &responder) {
        responder.sendResponse(error(QHttpServerResponse::StatusCode::NotFound, i18n("No such request: see GET /v1/status, /v1/printers, /v1/labels and POST /v1/print.")));
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    QHttpServerConfiguration configuration = m_server->configuration();
    configuration.setMaximumBodySize(MaxBodySize);
    m_server->setConfiguration(configuration);
#endif
}

HttpApi::~HttpApi() = default;

QString HttpApi::listen(const AutomationSettings &settings)
{
    // Anyone who can reach it could print: never without a token.
    if (settings.httpToken.size() < 16) {
        return i18n("no token is set (Settings > Automation in Plasma Label Creator makes one).");
    }
    m_tcp = new QTcpServer(this);
    const QHostAddress address = settings.httpNetwork ? QHostAddress(QHostAddress::Any) : QHostAddress(QHostAddress::LocalHost);
    if (!m_tcp->listen(address, quint16(settings.httpPort))) {
        const QString problem = u"port %1: %2"_s.arg(settings.httpPort).arg(m_tcp->errorString());
        delete m_tcp;
        m_tcp = nullptr;
        return problem;
    }
    if (!m_server->bind(m_tcp)) {
        return i18n("could not take requests on port %1.", QString::number(settings.httpPort));
    }
    qInfo("Taking print requests on %s port %d.", settings.httpNetwork ? "every network interface," : "this computer only,", int(m_tcp->serverPort()));
    return {};
}

quint16 HttpApi::port() const
{
    return m_tcp ? m_tcp->serverPort() : 0;
}

bool HttpApi::authorised(const QHttpServerRequest &request) const
{
    const QByteArray header = request.headers().value(QHttpHeaders::WellKnownHeader::Authorization).toByteArray().trimmed();
    if (!header.startsWith("Bearer ")) {
        return false;
    }
    return sameToken(header.mid(7).trimmed(), m_service->settings().httpToken.toUtf8());
}

QHttpServerResponse HttpApi::status() const
{
    const AutomationSettings &settings = m_service->settings();
    QJsonArray folders;
    for (const AutomationSettings::Folder &folder : settings.folders) {
        folders.append(QJsonObject{{"folder"_L1, folder.folder}, {"label"_L1, folder.label}, {"printer"_L1, folder.printer}, {"copies"_L1, folder.copies}});
    }
    return QHttpServerResponse(QJsonObject{
        {"service"_L1, u"plasma-label-creator-automation"_s},
        {"version"_L1, QStringLiteral(LABELCREATOR_VERSION_STRING)},
        {"printer"_L1, settings.printer},
        {"labelsFolder"_L1, settings.labelsFolder},
        {"folders"_L1, folders},
    });
}

QHttpServerResponse HttpApi::printers() const
{
    QJsonArray list;
    for (const PrinterInfo &printer : LabelPrinter::printers()) {
        const QueueStatus queue = LabelPrinter::queueStatus(printer.name);
        list.append(QJsonObject{
            {"name"_L1, printer.name},
            {"description"_L1, printer.description},
            {"labelPrinter"_L1, printer.hasLabelDriver()},
            {"state"_L1, queue.stateText()},
            {"problems"_L1, QJsonArray::fromStringList(queue.problems())},
            {"jobsWaiting"_L1, queue.activeJobs},
            {"default"_L1, printer.name == m_service->settings().printer},
        });
    }
    return QHttpServerResponse(QJsonObject{{"printers"_L1, list}});
}

QHttpServerResponse HttpApi::labels() const
{
    const QString folder = m_service->settings().labelsFolder;
    if (folder.isEmpty()) {
        return error(QHttpServerResponse::StatusCode::NotFound, i18n("No labels folder is set in the automation settings."));
    }
    QJsonArray list;
    for (const QString &name : Automation::labelNames(folder)) {
        const auto file = Automation::labelInFolder(folder, name);
        LabelReader reader;
        const auto document = file ? reader.readFile(*file) : std::nullopt;
        if (!document) {
            continue;
        }
        QJsonArray questions;
        for (const PrintQuestion &question : document->questions) {
            questions.append(QJsonObject{
                {"name"_L1, question.name},
                {"prompt"_L1, question.prompt.isEmpty() ? question.name : question.prompt},
                {"kind"_L1, kindName(question.kind)},
                {"required"_L1, question.required},
                {"default"_L1, question.defaultValue},
                {"choices"_L1, QJsonArray::fromStringList(question.choices)},
            });
        }
        list.append(QJsonObject{
            {"name"_L1, name},
            {"paper"_L1, document->paperName},
            {"fields"_L1, QJsonArray::fromStringList(Fields::used(*document))},
            {"questions"_L1, questions},
        });
    }
    return QHttpServerResponse(QJsonObject{{"labels"_L1, list}});
}

QHttpServerResponse HttpApi::print(const QHttpServerRequest &request)
{
    // JSON only: a web page elsewhere cannot send that without asking first.
    const QByteArray type = request.headers().value(QHttpHeaders::WellKnownHeader::ContentType).toByteArray().toLower();
    if (!type.startsWith("application/json")) {
        return error(QHttpServerResponse::StatusCode::UnsupportedMediaType, i18n("Send the request as JSON (Content-Type: application/json)."));
    }
    if (request.body().size() > MaxBodySize) {
        return error(QHttpServerResponse::StatusCode::PayloadTooLarge, i18n("The request is too large."));
    }
    QString problem;
    const auto job = PrintRequest::fromJson(request.body(), &problem);
    if (!job) {
        return error(QHttpServerResponse::StatusCode::BadRequest, problem);
    }
    const PrintOutcome outcome = m_service->print(*job, i18n("HTTP from %1", request.remoteAddress().toString()));
    using Code = QHttpServerResponse::StatusCode;
    switch (outcome.status) {
    case PrintOutcome::Status::Printed:
        return QHttpServerResponse(QJsonObject{
            {"job"_L1, outcome.job},
            {"labels"_L1, outcome.labels},
            {"message"_L1, outcome.message},
            {"warnings"_L1, QJsonArray::fromStringList(outcome.warnings)},
        });
    case PrintOutcome::Status::BadRequest:
        return error(Code::BadRequest, outcome.message);
    case PrintOutcome::Status::NotFound:
        return error(Code::NotFound, outcome.message);
    case PrintOutcome::Status::NotReady:
        return error(Code::Conflict, outcome.message);
    case PrintOutcome::Status::Failed:
        break;
    }
    return error(Code::InternalServerError, outcome.message);
}
