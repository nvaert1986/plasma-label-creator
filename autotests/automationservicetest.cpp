/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "automationservice.h"
#include "labelformat.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// No printer of this name exists: nothing is ever printed by these tests.
const QString NoPrinter = u"no-such-printer-for-tests"_s;

bool write(const QString &fileName, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(fileName).absolutePath());
    QFile file(fileName);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray read(const QString &fileName)
{
    QFile file(fileName);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

LabelDocument labelWith(const QString &text)
{
    LabelDocument document;
    document.paperName = u"30252 Address"_s;
    document.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
    TextObject object;
    object.spans = {{text, LabelFont(), Qt::black}};
    object.bounds = QRectF(100, 100, 4000, 800);
    document.objects << object;
    PrintQuestion question;
    question.name = u"Lot"_s;
    question.required = false;
    document.questions << question;
    return document;
}

struct Reply {
    int status = 0;
    QJsonObject json;
};

} // namespace

class AutomationServiceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void printsJobsDroppedInFolders()
    {
        QTemporaryDir dir;
        const QString jobs = dir.filePath(u"jobs"_s);
        QVERIFY(QDir().mkpath(jobs));
        const QString label = dir.filePath(u"labels/box.label"_s);
        QDir().mkpath(dir.filePath(u"labels"_s));
        QVERIFY(LabelWriter::writeFile(labelWith(u"{Serial}"_s), label));
        AutomationSettings settings;
        settings.folders << AutomationSettings::Folder{jobs, label, NoPrinter, 1};
        settings.labelsFolder = dir.filePath(u"labels"_s);
        const QString settingsFile = dir.filePath(u"automation.json"_s);
        QVERIFY(settings.save(settingsFile));

        AutomationService service(settingsFile);
        service.start();
        QVERIFY(write(jobs + u"/rows.csv"_s, "Serial;Site\nA1;Utrecht\nA2;Delft\n"));
        QVERIFY(write(jobs + u"/broken.json"_s, "{not json"));
        QVERIFY(write(jobs + u"/named.json"_s, R"({"label": "../elsewhere"})"));
        QVERIFY(write(jobs + u"/ignored.pdf"_s, "%PDF"));
        // The first look sees them; they are printed once they stay the same.
        service.scanFolders();
        QVERIFY(QFileInfo::exists(jobs + u"/rows.csv"_s));
        service.scanFolders();

        // Each put away with why it failed: no printer of that name.
        QVERIFY(!QFileInfo::exists(jobs + u"/rows.csv"_s));
        QVERIFY(QFileInfo::exists(jobs + u"/failed/rows.csv"_s));
        QVERIFY(read(jobs + u"/failed/rows.csv.txt"_s).contains(NoPrinter.toUtf8()));
        QVERIFY(read(jobs + u"/failed/broken.json.txt"_s).contains("Not JSON"));
        QVERIFY(read(jobs + u"/failed/named.json.txt"_s).contains("no label ../elsewhere"));
        // Not a job.
        QVERIFY(QFileInfo::exists(jobs + u"/ignored.pdf"_s));

        // The same name again: kept beside the first.
        QVERIFY(write(jobs + u"/rows.csv"_s, "Serial\nB1\n"));
        service.scanFolders();
        service.scanFolders();
        QVERIFY(QFileInfo::exists(jobs + u"/failed/rows-2.csv"_s));
    }

    void requestsOverHttp()
    {
        if (!AutomationService::hasHttp()) {
            QSKIP("Built without Qt HTTP Server");
        }
        QTemporaryDir dir;
        const QString labels = dir.filePath(u"labels"_s);
        QDir().mkpath(labels + u"/Rack"_s);
        QVERIFY(LabelWriter::writeFile(labelWith(u"{Port}"_s), labels + u"/Rack/patch.label"_s));
        AutomationSettings settings;
        settings.labelsFolder = labels;
        settings.printer = NoPrinter;
        settings.http = true;
        settings.httpPort = 0;
        settings.httpToken = AutomationSettings::newToken();
        const QString settingsFile = dir.filePath(u"automation.json"_s);
        QVERIFY(settings.save(settingsFile));
        AutomationService service(settingsFile);
        service.start();
        QVERIFY(service.httpPort() > 0);

        QNetworkAccessManager network;
        const auto send = [&](const QByteArray &verb, const QString &path, const QByteArray &token, const QByteArray &body = {}, const QByteArray &type = "application/json") {
            QNetworkRequest request(QUrl(u"http://127.0.0.1:%1%2"_s.arg(service.httpPort()).arg(path)));
            if (!token.isEmpty()) {
                request.setRawHeader("Authorization", "Bearer " + token);
            }
            if (!body.isEmpty()) {
                request.setHeader(QNetworkRequest::ContentTypeHeader, type);
            }
            QNetworkReply *reply = network.sendCustomRequest(request, verb, body);
            QSignalSpy finished(reply, &QNetworkReply::finished);
            finished.wait(10000);
            Reply result;
            result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            result.json = QJsonDocument::fromJson(reply->readAll()).object();
            reply->deleteLater();
            return result;
        };
        const QByteArray token = settings.httpToken.toUtf8();

        // No token, or a wrong one: nothing.
        QCOMPARE(send("GET", u"/v1/status"_s, {}).status, 401);
        QCOMPARE(send("GET", u"/v1/status"_s, "wrong-token-of-some-length").status, 401);
        QCOMPARE(send("POST", u"/v1/print"_s, {}, R"({"label": "Rack/patch"})").status, 401);

        const Reply status = send("GET", u"/v1/status"_s, token);
        QCOMPARE(status.status, 200);
        QCOMPARE(status.json.value("service"_L1).toString(), u"plasma-label-creator-automation"_s);

        const Reply labelList = send("GET", u"/v1/labels"_s, token);
        QCOMPARE(labelList.status, 200);
        const QJsonObject first = labelList.json.value("labels"_L1).toArray().at(0).toObject();
        QCOMPARE(first.value("name"_L1).toString(), u"Rack/patch.label"_s);
        QCOMPARE(first.value("questions"_L1).toArray().at(0).toObject().value("name"_L1).toString(), u"Lot"_s);
        QVERIFY(first.value("fields"_L1).toArray().contains(u"Port"_s));

        QCOMPARE(send("GET", u"/v1/printers"_s, token).status, 200);
        QCOMPARE(send("GET", u"/v1/nothing"_s, token).status, 404);

        // Printing: JSON only, a label from the labels folder, a printer that exists.
        QCOMPARE(send("POST", u"/v1/print"_s, token, R"({"label": "Rack/patch"})", "text/plain").status, 415);
        QCOMPARE(send("POST", u"/v1/print"_s, token, "{nope").status, 400);
        QCOMPARE(send("POST", u"/v1/print"_s, token, R"({"label": "../automation.json"})").status, 404);
        const Reply noPrinter = send("POST", u"/v1/print"_s, token, R"({"label": "Rack/patch", "values": {"Port": "A12"}})");
        QCOMPARE(noPrinter.status, 404);
        QVERIFY(noPrinter.json.value("error"_L1).toString().contains(NoPrinter));
    }
};

QTEST_GUILESS_MAIN(AutomationServiceTest)

#include "automationservicetest.moc"
