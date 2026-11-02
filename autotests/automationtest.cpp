/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "automation.h"
#include "dlslabelids.h"
#include "labelformat.h"
#include "zplprinter.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

LabelDocument labelWith(const QString &text)
{
    LabelDocument document;
    document.paperName = u"30252 Address"_s;
    document.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
    TextObject object;
    object.name = u"TEXT"_s;
    object.spans = {{text, LabelFont(), Qt::black}};
    object.bounds = QRectF(100, 100, 4000, 800);
    document.objects << object;
    return document;
}

QString textOf(const LabelDocument &document)
{
    return std::get<TextObject>(document.objects.at(0)).plainText();
}

bool write(const QString &fileName, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(fileName).absolutePath());
    QFile file(fileName);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

} // namespace

class AutomationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void settingsRoundTrip()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"sub/automation.json"_s);
        AutomationSettings settings;
        settings.folders << AutomationSettings::Folder{u"/srv/jobs"_s, u"/srv/labels/box.label"_s, u"Zebra"_s, 3};
        settings.labelsFolder = u"/srv/labels"_s;
        settings.printer = u"LabelWriter"_s;
        settings.saveCounters = false;
        settings.http = true;
        settings.httpNetwork = true;
        settings.httpPort = 9000;
        settings.httpToken = AutomationSettings::newToken();
        QVERIFY(settings.save(file));
        QCOMPARE(AutomationSettings::load(file), settings);
        // It holds the token: for the user only.
        QCOMPARE(QFile::permissions(file) & (QFileDevice::ReadGroup | QFileDevice::ReadOther), QFileDevice::Permissions());
        // Tokens are long and never the same.
        QVERIFY(settings.httpToken.size() >= 40);
        QVERIFY(AutomationSettings::newToken() != AutomationSettings::newToken());
        // No file: the defaults.
        QCOMPARE(AutomationSettings::load(dir.filePath(u"none.json"_s)), AutomationSettings());
    }

    void requestsFromJson()
    {
        QString error;
        const auto request = PrintRequest::fromJson(
            R"({"label": "Rack/patch", "printer": "Zebra", "copies": 2, "values": {"Port": "A12", "Count": 3, "Done": true},
                "rows": [{"Serial": "1"}, {"Serial": 2}], "printAnyway": true})",
            &error);
        QVERIFY2(request, qPrintable(error));
        QCOMPARE(request->label, u"Rack/patch"_s);
        QCOMPARE(request->printer, u"Zebra"_s);
        QCOMPARE(request->copies, 2);
        QCOMPARE(request->values.value(u"Port"_s), u"A12"_s);
        QCOMPARE(request->values.value(u"Count"_s), u"3"_s);
        QCOMPARE(request->values.value(u"Done"_s), u"true"_s);
        QCOMPARE(request->rows.size(), 2);
        QCOMPARE(request->rows.at(1).value(u"Serial"_s), u"2"_s);
        QVERIFY(request->printAnyway);

        // What does not do is said, not guessed.
        for (const QByteArray &bad : {QByteArray("not json"),
                                      QByteArray("[1, 2]"),
                                      QByteArray(R"({"copies": 0})"),
                                      QByteArray(R"({"copies": 2.5})"),
                                      QByteArray(R"({"label": 12})"),
                                      QByteArray(R"({"values": [1]})"),
                                      QByteArray(R"({"values": {"A": {"nested": 1}}})"),
                                      QByteArray(R"({"rows": {"A": 1}})"),
                                      QByteArray(R"({"printAnyway": "yes"})")}) {
            error.clear();
            QVERIFY2(!PrintRequest::fromJson(bad, &error), bad.constData());
            QVERIFY(!error.isEmpty());
        }
    }

    void labelsOnlyFromTheLabelsFolder()
    {
        QTemporaryDir dir;
        const QString labels = dir.filePath(u"labels"_s);
        QVERIFY(write(labels + u"/box.label"_s, "x"));
        QVERIFY(write(labels + u"/Rack/patch.label"_s, "x"));
        QVERIFY(write(dir.filePath(u"secret.label"_s), "x"));
        QVERIFY(QFile::link(dir.filePath(u"secret.label"_s), labels + u"/link.label"_s));

        QCOMPARE(Automation::labelInFolder(labels, u"box"_s), QFileInfo(labels + u"/box.label"_s).canonicalFilePath());
        QCOMPARE(Automation::labelInFolder(labels, u"Rack/patch.label"_s), QFileInfo(labels + u"/Rack/patch.label"_s).canonicalFilePath());
        QVERIFY(!Automation::labelInFolder(labels, u"../secret"_s));
        QVERIFY(!Automation::labelInFolder(labels, dir.filePath(u"secret.label"_s)));
        QVERIFY(!Automation::labelInFolder(labels, u"link"_s)); // leads outside
        QVERIFY(!Automation::labelInFolder(labels, u"none"_s));
        QVERIFY(!Automation::labelInFolder(QString(), u"box"_s));
        QCOMPARE(Automation::labelNames(labels), (QStringList{u"box.label"_s, u"Rack/patch.label"_s}));
    }

    void answersAsTheFormGivesThem()
    {
        LabelDocument document = labelWith(u"{Lot}"_s);
        PrintQuestion lot;
        lot.name = u"Lot"_s;
        lot.allowed = PrintQuestion::Allowed::Digits;
        PrintQuestion weight;
        weight.name = u"Weight"_s;
        weight.kind = PrintQuestion::Kind::Number;
        weight.decimals = 1;
        weight.minimum = 0;
        weight.maximum = 50;
        weight.required = false;
        PrintQuestion packed;
        packed.name = u"Packed"_s;
        packed.kind = PrintQuestion::Kind::Date;
        packed.dateFormat = u"dd-MM-yyyy"_s;
        PrintQuestion fragile;
        fragile.name = u"Fragile"_s;
        fragile.kind = PrintQuestion::Kind::Check;
        fragile.checkedText = u"FRAGILE"_s;
        fragile.uncheckedText = QString();
        PrintQuestion shift;
        shift.name = u"Shift"_s;
        shift.defaultValue = u"Day"_s;
        document.questions = {lot, weight, packed, fragile, shift};

        QString error;
        const auto given = Automation::answers(document, {{u"Lot"_s, u"123"_s}, {u"Weight"_s, u"2.26"_s}, {u"Packed"_s, u"2026-11-02"_s}, {u"Fragile"_s, u"true"_s}}, &error);
        QVERIFY2(given, qPrintable(error));
        QCOMPARE(given->value(u"Weight"_s), u"2.3"_s); // in the question's decimals
        QCOMPARE(given->value(u"Packed"_s), u"02-11-2026"_s);
        QCOMPARE(given->value(u"Fragile"_s), u"FRAGILE"_s);
        QCOMPARE(given->value(u"Shift"_s), u"Day"_s); // the default
        // Not given: today, and the check box not ticked.
        const auto defaults = Automation::answers(document, {{u"Lot"_s, u"7"_s}});
        QVERIFY(defaults);
        QCOMPARE(defaults->value(u"Packed"_s), QDate::currentDate().toString(u"dd-MM-yyyy"_s));
        QCOMPARE(defaults->value(u"Fragile"_s), u"No"_s);

        // As the form starts when nothing is given: a drop-down on its first
        // choice (empty when optional), a number on its default within its
        // range, a date that is not one on today.
        PrintQuestion size;
        size.name = u"Size"_s;
        size.kind = PrintQuestion::Kind::Choice;
        size.choices = {u"S"_s, u"M"_s};
        PrintQuestion colour = size;
        colour.name = u"Colour"_s;
        colour.required = false;
        PrintQuestion count;
        count.name = u"Count"_s;
        count.kind = PrintQuestion::Kind::Number;
        count.defaultValue = u"75"_s;
        count.maximum = 50;
        PrintQuestion due;
        due.name = u"Due"_s;
        due.kind = PrintQuestion::Kind::Date;
        due.defaultValue = u"next week"_s;
        LabelDocument form = labelWith(u"x"_s);
        form.questions = {size, colour, count, due};
        const auto started = Automation::answers(form, {});
        QVERIFY(started);
        QCOMPARE(started->value(u"Size"_s), u"S"_s);
        QCOMPARE(started->value(u"Colour"_s), QString());
        QCOMPARE(started->value(u"Count"_s), u"50"_s);
        QCOMPARE(started->value(u"Due"_s), QDate::currentDate().toString(QLocale().dateFormat(QLocale::ShortFormat)));
        // Given, a choice must be one of them, and 1,234 is not 1234.
        QVERIFY(!Automation::answers(form, {{u"Size"_s, u"XL"_s}}));
        QVERIFY(!Automation::answers(form, {{u"Count"_s, u"1,234"_s}}));

        // The question's rules still hold.
        QVERIFY(!Automation::answers(document, {}, &error)); // Lot is needed
        QVERIFY(error.contains(u"Lot"_s));
        QVERIFY(!Automation::answers(document, {{u"Lot"_s, u"12a"_s}}, &error));
        QVERIFY(!Automation::answers(document, {{u"Lot"_s, u"1"_s}, {u"Weight"_s, u"51"_s}}, &error));
        QVERIFY(!Automation::answers(document, {{u"Lot"_s, u"1"_s}, {u"Weight"_s, u"heavy"_s}}, &error));
        QVERIFY(!Automation::answers(document, {{u"Lot"_s, u"1"_s}, {u"Packed"_s, u"someday"_s}}, &error));
        QVERIFY(!Automation::answers(document, {{u"Lot"_s, u"1"_s}, {u"Fragile"_s, u"maybe"_s}}, &error));
    }

    void preparesALabelPerRow()
    {
        LabelDocument document = labelWith(u"{Serial} {Site} {Lot}"_s);
        PrintQuestion lot;
        lot.name = u"Lot"_s;
        document.questions = {lot};
        ImageObject picture;
        picture.imageLocation = u"pictures/{Serial}.png"_s;
        document.objects << picture;

        PrintRequest request;
        request.values = {{u"Site"_s, u"Utrecht"_s}, {u"Lot"_s, u"L1"_s}};
        request.rows = {{{u"Serial"_s, u"A1"_s}}, {{u"Serial"_s, u"A2"_s}, {u"Site"_s, u"Delft"_s}, {u"Lot"_s, u"from the row"_s}}};
        const auto labels = Automation::prepare(document, u"/srv/labels/box.label"_s, request);
        QVERIFY(labels);
        QCOMPARE(labels->size(), 2);
        QCOMPARE(textOf(labels->at(0)), u"A1 Utrecht L1"_s);
        // A row's own values over the ones for every label; the answers over both.
        QCOMPARE(textOf(labels->at(1)), u"A2 Delft L1"_s);
        // Pictures next to the label.
        QCOMPARE(*std::get<ImageObject>(labels->at(1).objects.at(1)).imageLocation, u"/srv/labels/pictures/A2.png"_s);

        // No rows: one label.
        request.rows.clear();
        QCOMPARE(Automation::prepare(document, u"/srv/labels/box.label"_s, request)->size(), 1);
    }

    void readsTheAppsSettings()
    {
        QTemporaryDir system;
        const QString user = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QVERIFY(write(system.filePath(u"plasma-label-creatorrc"_s),
                      "[Settings]\nHistoryKept[$i]=0\nCounterWaitSeconds=20\n"
                      "[Printer Drivers]\nShared-LW=lw450.ppd\n"));
        QVERIFY(write(user + u"/plasma-label-creatorrc"_s,
                      "[General]\nPaper=30252 Address\n\n[Print Offsets]\nZebra=1.5,-0.5\n"
                      "[Printer Drivers]\nOffice-LW=lw450t.ppd\n"
                      "[Settings]\nHistoryKept=100\nCounterWaitSeconds=5\nAskPrinters=false\n"));
        qputenv("XDG_CONFIG_DIRS", system.path().toUtf8());
        const PrintEnvironment environment = PrintEnvironment::fromAppConfig();
        qunsetenv("XDG_CONFIG_DIRS");
        QFile::remove(user + u"/plasma-label-creatorrc"_s);

        QCOMPARE(environment.drivers.value(u"Shared-LW"_s), u"lw450.ppd"_s);
        QCOMPARE(environment.drivers.value(u"Office-LW"_s), u"lw450t.ppd"_s);
        QCOMPARE(environment.offsetsMm.value(u"Zebra"_s), QPointF(1.5, -0.5));
        QCOMPARE(environment.counterWaitSeconds, 5); // the user's
        QCOMPARE(environment.historyKept, 0); // fixed by the administrator
        QVERIFY(!environment.askPrinters);
        QVERIFY(PrintEnvironment().askPrinters); // by default, as the app
    }

    void printingNeedsAPrinterThatExists()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"box.label"_s);
        QVERIFY(LabelWriter::writeFile(labelWith(u"{Serial}"_s), file));
        PrintEnvironment environment;
        environment.historyDirectory = dir.filePath(u"history"_s);

        PrintRequest request;
        PrintOutcome outcome = Automation::print(file, request, environment);
        QCOMPARE(outcome.status, PrintOutcome::Status::BadRequest);
        request.printer = u"no-such-printer-for-tests"_s;
        outcome = Automation::print(file, request, environment);
        QCOMPARE(outcome.status, PrintOutcome::Status::NotFound);
        QVERIFY(outcome.message.contains(u"no-such-printer-for-tests"_s));
        outcome = Automation::print(dir.filePath(u"none.label"_s), request, environment);
        QCOMPARE(outcome.status, PrintOutcome::Status::NotFound);
        // Nothing printed, nothing in the history.
        QVERIFY(!QFileInfo::exists(environment.historyDirectory));
    }

    void asksZplPrintersAsTheAppDoes()
    {
        // A 30252 label is 89 mm long along the feed.
        const LabelDocument document = labelWith(u"x"_s);
        ZplPrinterInfo info;
        info.dotsPerMm = 8;
        info.labelLengthDots = 712; // 89 mm
        QVERIFY(Automation::zplProblems(document, info, u"Altec"_s).isEmpty());
        info.labelLengthDots = 712 + 16; // 2 mm of gap counted in: still fits
        QVERIFY(Automation::zplProblems(document, info, u"Altec"_s).isEmpty());
        info.labelLengthDots = 200; // 25 mm labels loaded
        QVERIFY(Automation::zplProblems(document, info, u"Altec"_s).join(u' ').contains(u"25.0 mm"_s));
        // Not measured, or a continuous roll: nothing to say about the length.
        info.dotsPerMm = 0;
        QVERIFY(Automation::zplProblems(document, info, u"Altec"_s).isEmpty());
        info.dotsPerMm = 8;
        info.configuration.insert(u"MEDIA TYPE"_s, u"CONTINUOUS"_s);
        QVERIFY(Automation::zplProblems(document, info, u"Altec"_s).isEmpty());
        // What the printer itself reports.
        info.headOpen = true;
        QCOMPARE(Automation::zplProblems(document, info, u"Altec"_s).size(), 1);
    }

    void keepsAListOfRecentJobs()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath(u"data/automation-jobs.jsonl"_s);
        QVERIFY(Automation::recentJobs(log).isEmpty());
        for (int i = 1; i <= 30; ++i) {
            AutomationJob job;
            job.when = QDateTime(QDate(2026, 11, 2), QTime(12, 0)).addSecs(i);
            job.source = u"HTTP from 127.0.0.1"_s;
            job.label = u"box.label"_s;
            job.printer = u"Zebra"_s;
            job.printed = i % 2 == 0;
            job.labels = i;
            job.job = 100 + i;
            job.message = u"line one\nline two"_s;
            QVERIFY(Automation::logJob(job, log, 10));
        }
        const QList<AutomationJob> jobs = Automation::recentJobs(log);
        QVERIFY(jobs.size() >= 10 && jobs.size() <= 30);
        // The newest first, as written.
        QCOMPARE(jobs.first().job, 130);
        QCOMPARE(jobs.first().message, u"line one\nline two"_s);
        QVERIFY(jobs.first().printed);
        QCOMPARE(jobs.first().when, QDateTime(QDate(2026, 11, 2), QTime(12, 0, 30)));
        // Cut back once it grows: never more than a few hundred bytes a job.
        for (int i = 0; i < 200; ++i) {
            QVERIFY(Automation::logJob(jobs.first(), log, 10));
        }
        QVERIFY(Automation::recentJobs(log).size() <= 40);
    }

    void dymoLabelIds()
    {
        QCOMPARE(DlsLabelIds::forPaper(u"30252 Address"_s), u"Address"_s);
        QCOMPARE(DlsLabelIds::forPaper(u"99010 Standard Address"_s), u"Address"_s);
        QCOMPARE(DlsLabelIds::forPaper(u"30334 2-1/4 in x 1-1/4 in"_s), u"Small30334"_s);
        QCOMPARE(DlsLabelIds::forPaper(u"1933081 Durable 1 in x 3-1/2 in"_s), u"LW_DURABLE_25X89mm"_s);
        QVERIFY(!DlsLabelIds::forPaper(u"Altec 50 x 25"_s));

        QVERIFY(DlsLabelIds::isDriverKeyword(u"w79h252"_s));
        QVERIFY(DlsLabelIds::isDriverKeyword(u"w79h252.1"_s));
        QVERIFY(DlsLabelIds::isDriverKeyword(u"1933081_Durable_1_in_x_3-1_2_in"_s));
        QVERIFY(!DlsLabelIds::isDriverKeyword(u"Address"_s));
        QVERIFY(!DlsLabelIds::isDriverKeyword(u"Custom.50x25mm"_s));

        // A driver keyword becomes DYMO's id; DYMO's own ids and other
        // labels stay as they are.
        QCOMPARE(DlsLabelIds::preferred(u"w79h252.1"_s, u"30252 Address"_s), u"Address"_s);
        QCOMPARE(DlsLabelIds::preferred(u"NameBadge"_s, u"30256 Shipping"_s), u"NameBadge"_s);
        QCOMPARE(DlsLabelIds::preferred(u"Custom.50x25mm"_s, u"Altec 50 x 25"_s), u"Custom.50x25mm"_s);
        QCOMPARE(DlsLabelIds::preferred(u"w999h999"_s, u"Unknown paper"_s), u"w999h999"_s);

        // Saving writes DYMO's id.
        LabelDocument document = labelWith(u"x"_s);
        document.id = u"w79h252"_s;
        const QByteArray saved = LabelWriter::toByteArray(document);
        QVERIFY(saved.contains("<Id>Address</Id>"));
        QVERIFY(!saved.contains("w79h252"));
    }
};

QTEST_GUILESS_MAIN(AutomationTest)

#include "automationtest.moc"
