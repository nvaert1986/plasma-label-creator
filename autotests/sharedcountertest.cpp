/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelformat.h"
#include "sharedcounter.h"

#include <QBuffer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

class SharedCounterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void countsOnFromTheFile()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"labels.counter"_s);
        SharedCounter first(file);
        QVERIFY(first.lock(0));
        // No file yet: the label's own value.
        QCOMPARE(first.read(41), std::optional<int>(41));
        QVERIFY(first.write(45));

        // Another computer waits while this one prints.
        SharedCounter second(file);
        QVERIFY(!second.lock(0));
        QVERIFY(second.errorString().contains(u"in use"_s));
        first.unlock();
        QVERIFY(second.lock(0));
        QCOMPARE(second.read(1), std::optional<int>(45));
        second.unlock();

        // Something else in the file is not taken for a number.
        QFile broken(file);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("forty");
        broken.close();
        QVERIFY(!second.read(1));
    }

    void theFileIsKeptWithTheLabel()
    {
        LabelDocument document;
        document.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
        CounterObject counter;
        counter.name = u"COUNTER"_s;
        counter.bounds = QRectF(100, 100, 2000, 500);
        counter.sharedFile = u"/mnt/labels/rack.counter"_s;
        document.objects << counter;
        QBuffer buffer;
        buffer.setData(LabelWriter::toByteArray(document));
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        const auto read = reader.read(&buffer);
        QVERIFY2(read, qPrintable(reader.errorString()));
        QCOMPARE(std::get<CounterObject>(read->objects.at(0)).sharedFile, counter.sharedFile);
    }
};

QTEST_GUILESS_MAIN(SharedCounterTest)

#include "sharedcountertest.moc"
