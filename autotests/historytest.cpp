/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelformat.h"
#include "labelversions.h"
#include "printhistory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

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

QString textIn(const QString &fileName)
{
    LabelReader reader;
    return std::get<TextObject>(reader.readFile(fileName)->objects.at(0)).plainText();
}

} // namespace

class HistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsEarlierVersions()
    {
        QTemporaryDir labels;
        QTemporaryDir store;
        LabelVersions versions(store.path());
        const QString file = labels.filePath(u"rack.label"_s);
        // A new file: nothing to keep yet.
        QVERIFY(versions.keep(file));
        QVERIFY(versions.versions(file).isEmpty());

        for (const QString &text : {u"first"_s, u"second"_s, u"third"_s}) {
            QVERIFY(versions.keep(file, 2)); // before saving over it
            QVERIFY(LabelWriter::writeFile(labelWith(text), file));
            QThread::msleep(5); // versions are named by the time
        }
        // Kept: what "third" and "second" saved over, newest first; at most two.
        const QList<LabelVersions::Version> kept = versions.versions(file);
        QCOMPARE(kept.size(), 2);
        QCOMPARE(textIn(kept.at(0).fileName), u"second"_s);
        QCOMPARE(textIn(kept.at(1).fileName), u"first"_s);
        QVERIFY(kept.at(0).saved >= kept.at(1).saved);

        // Saved again unchanged: no new version.
        QVERIFY(versions.keep(file, 5));
        QVERIFY(versions.keep(file, 5));
        QCOMPARE(versions.versions(file).size(), 3);
        // Another label has its own.
        QVERIFY(versions.versions(labels.filePath(u"other.label"_s)).isEmpty());
    }

    void keepsVersionsNextToTheLabel()
    {
        QTemporaryDir labels;
        const QString file = labels.filePath(u"rack.label"_s);
        QVERIFY(LabelWriter::writeFile(labelWith(u"first"_s), file));
        LabelVersions versions = LabelVersions::nextToLabels();
        QVERIFY(versions.keep(file));
        QVERIFY(QFileInfo::exists(labels.filePath(u".versions/rack.label"_s)));
        QCOMPARE(versions.versions(file).size(), 1);
        QVERIFY(versions.versions(file).constFirst().fileName.startsWith(labels.filePath(u".versions/rack.label/"_s)));
    }

    void keepsAsManyPrintsAsSet()
    {
        QTemporaryDir store;
        PrintHistory history(store.path());
        history.setMaxRecords(3);
        PrintRecord record;
        record.title = u"rack.label"_s;
        for (int job = 1; job <= 8; ++job) {
            record.job = job;
            QVERIFY(history.add(record, labelWith(u"x"_s)));
        }
        // Trimmed now and then, to the last three.
        QVERIFY(history.records().size() <= 3 + 3 / 10 + 1);
        QCOMPARE(history.records().constFirst().job, 8);
    }

    void remembersPrints()
    {
        QTemporaryDir store;
        PrintHistory history(store.path());
        PrintRecord first;
        first.when = QDateTime(QDate(2026, 11, 2), QTime(9, 0));
        first.title = u"rack.label"_s;
        first.source = u"/labels/rack.label"_s;
        first.printer = u"Altec"_s;
        first.labels = 2;
        first.job = 41;
        first.answers = {{u"Batch"_s, u"1234"_s}};
        first.rows = {{{u"Name"_s, u"SRV-1"_s}}, {{u"Name"_s, u"SRV-2"_s}}};
        QVERIFY(history.add(first, labelWith(u"{Name}"_s)));
        PrintRecord second = first;
        second.when = first.when.addSecs(60);
        second.job = 42;
        second.rows.clear();
        QVERIFY(history.add(second, labelWith(u"{Name}"_s)));

        const QList<PrintRecord> records = history.records();
        QCOMPARE(records.size(), 2);
        QCOMPARE(records.at(0).job, 42); // newest first
        QCOMPARE(records.at(1).rows.size(), 2);
        QCOMPARE(records.at(1).rows.at(1).value(u"Name"_s), u"SRV-2"_s);
        QCOMPARE(records.at(1).answers.value(u"Batch"_s), u"1234"_s);
        // The same label is kept once.
        QCOMPARE(records.at(0).labelCopy, records.at(1).labelCopy);
        QCOMPARE(QDir(store.filePath(u"labels"_s)).entryList(QDir::Files).size(), 1);
        const auto label = history.label(records.at(0));
        QVERIFY(label);
        QCOMPARE(std::get<TextObject>(label->objects.at(0)).plainText(), u"{Name}"_s);

        history.clear();
        QVERIFY(history.records().isEmpty());
    }
};

QTEST_GUILESS_MAIN(HistoryTest)

#include "historytest.moc"
