/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelformat.h"
#include "printrun.h"

#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

LabelDocument labelWith(const QString &text, const QString &paper = u"30252 Address"_s, const QSizeF &size = QSizeF(1581, 5040))
{
    LabelDocument document;
    document.paperName = paper;
    document.paperOutline.addRect(QRectF(QPointF(0, 0), size));
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

} // namespace

class PrintRunTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void oneLabelPerRowWithItsCopies()
    {
        PrintRun::Request request;
        request.document = labelWith(u"{Serial} {Lot}"_s);
        request.document.dataSource.kind = DataSource::Kind::File;
        request.document.dataSource.copiesColumn = u"Quantity"_s;
        request.answers = {{u"Lot"_s, u"L7"_s}};
        request.rows = {{{u"Serial"_s, u"A"_s}, {u"Quantity"_s, u"2"_s}},
                        {{u"Serial"_s, u"B"_s}, {u"Quantity"_s, u"0"_s}}, // none of this one
                        {{u"Serial"_s, u"C"_s}, {u"Quantity"_s, QString()}}}; // empty: one
        request.copies = 3;
        QString error;
        const auto run = PrintRun::make(request, &error);
        QVERIFY2(run, qPrintable(error));
        QCOMPARE(run->labels.size(), 3);
        QCOMPARE(textOf(run->labels.at(0)), u"A L7"_s);
        QCOMPARE(textOf(run->labels.at(1)), u"A L7"_s);
        QCOMPARE(textOf(run->labels.at(2)), u"C L7"_s);
        QCOMPARE(run->copies, 3); // the printer makes the copies
        QCOMPARE(run->headerLabels, 0);

        // A number that is not one, and nothing at all to print, are said.
        request.rows = {{{u"Serial"_s, u"A"_s}, {u"Quantity"_s, u"two"_s}}};
        QVERIFY(!PrintRun::make(request, &error));
        QVERIFY(error.contains(u"Row 1"_s));
        request.rows = {{{u"Serial"_s, u"A"_s}, {u"Quantity"_s, u"0"_s}}};
        QVERIFY(!PrintRun::make(request, &error));
    }

    void aLabelPerRowFromAColumn()
    {
        QTemporaryDir dir;
        const QString main = dir.filePath(u"main.label"_s);
        LabelDocument document = labelWith(u"main {Serial}"_s);
        document.dataSource.kind = DataSource::Kind::File;
        document.dataSource.labelColumn = u"Label"_s;
        QVERIFY(LabelWriter::writeFile(document, main));
        LabelDocument cable = labelWith(u"cable {Serial} {Colour}"_s);
        PrintQuestion colour;
        colour.name = u"Colour"_s;
        colour.defaultValue = u"red"_s;
        cable.questions = {colour};
        QVERIFY(LabelWriter::writeFile(cable, dir.filePath(u"cable.label"_s)));
        QVERIFY(LabelWriter::writeFile(labelWith(u"small"_s, u"30334 2-1/4 in x 1-1/4 in"_s, QSizeF(3240, 1800)), dir.filePath(u"small.label"_s)));

        PrintRun::Request request;
        request.document = document;
        request.labelFile = main;
        request.rows = {{{u"Serial"_s, u"1"_s}, {u"Label"_s, u"cable"_s}}, {{u"Serial"_s, u"2"_s}, {u"Label"_s, QString()}}};
        QString error;
        auto run = PrintRun::make(request, &error);
        QVERIFY2(run, qPrintable(error));
        QCOMPARE(textOf(run->labels.at(0)), u"cable 1 red"_s); // its own question's default
        QCOMPARE(textOf(run->labels.at(1)), u"main 2"_s);

        // Another label type, or no such label: refused, with the row.
        request.rows = {{{u"Serial"_s, u"1"_s}, {u"Label"_s, u"small.label"_s}}};
        QVERIFY(!PrintRun::make(request, &error));
        QVERIFY(error.contains(u"one label type"_s));
        request.rows = {{{u"Serial"_s, u"1"_s}, {u"Label"_s, u"missing"_s}}};
        QVERIFY(!PrintRun::make(request, &error));
        QVERIFY(error.contains(u"Row 1"_s));
        // Not saved: other labels cannot be found.
        request.labelFile.clear();
        request.rows = {{{u"Serial"_s, u"1"_s}, {u"Label"_s, u"cable"_s}}};
        QVERIFY(!PrintRun::make(request, &error));
    }

    void headerAndTrailerAroundTheRun()
    {
        QTemporaryDir dir;
        const QString main = dir.filePath(u"main.label"_s);
        LabelDocument document = labelWith(u"{Serial}"_s);
        document.headerLabel = u"header.label"_s;
        document.trailerLabel = u"trailer"_s;
        QVERIFY(LabelWriter::writeFile(document, main));
        QVERIFY(LabelWriter::writeFile(labelWith(u"BATCH {Lot}: {Count} labels"_s), dir.filePath(u"header.label"_s)));
        QVERIFY(LabelWriter::writeFile(labelWith(u"END {Lot}"_s), dir.filePath(u"trailer.label"_s)));

        PrintRun::Request request;
        request.document = document;
        request.labelFile = main;
        request.answers = {{u"Lot"_s, u"L7"_s}};
        request.rows = {{{u"Serial"_s, u"A"_s}}, {{u"Serial"_s, u"B"_s}}};
        request.copies = 2;
        QString error;
        const auto run = PrintRun::make(request, &error);
        QVERIFY2(run, qPrintable(error));
        // The copies laid out here, so the header and trailer come once.
        QCOMPARE(run->copies, 1);
        QCOMPARE(run->labels.size(), 6);
        QCOMPARE(run->headerLabels, 1);
        QCOMPARE(run->trailerLabels, 1);
        QCOMPARE(textOf(run->labels.at(0)), u"BATCH L7: 4 labels"_s);
        QCOMPARE(textOf(run->labels.at(1)), u"A"_s);
        QCOMPARE(textOf(run->labels.at(2)), u"A"_s);
        QCOMPARE(textOf(run->labels.at(4)), u"B"_s);
        QCOMPARE(textOf(run->labels.at(5)), u"END L7"_s);
    }

    void picturesAndConditionsAsTheLabelSays()
    {
        LabelDocument document = labelWith(u"{Serial}"_s);
        objectBase(document.objects[0]).condition = {PrintCondition::Test::IsNotEmpty, u"Serial"_s, QString()};
        ImageObject photo;
        photo.imageLocation = u"photos/{Serial}.png"_s;
        document.objects << photo;
        ImageObject stored;
        stored.imageLocation = u"{Picture}"_s;
        document.objects << stored;
        PrintRun::Request request;
        request.document = document;
        request.pictureFolder = u"/srv/data"_s;
        request.rows = {{{u"Serial"_s, u"A"_s}, {u"Picture"_s, u"data:image/png;base64,AAAA"_s}}, {{u"Serial"_s, QString()}}};
        const auto run = PrintRun::make(request);
        QVERIFY(run);
        QCOMPARE(*std::get<ImageObject>(run->labels.at(0).objects.at(1)).imageLocation, u"/srv/data/photos/A.png"_s);
        // A picture in the field itself has no folder.
        QCOMPARE(*std::get<ImageObject>(run->labels.at(0).objects.at(2)).imageLocation, u"data:image/png;base64,AAAA"_s);
        QVERIFY(objectBase(run->labels.at(0).objects.at(0)).printed);
        QVERIFY(!objectBase(run->labels.at(1).objects.at(0)).printed);
    }
};

QTEST_GUILESS_MAIN(PrintRunTest)

#include "printruntest.moc"
