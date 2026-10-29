/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelformat.h"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QString dataFile(const QString &name)
{
    return QStringLiteral(LABELCREATOR_TEST_DATA "/") + name;
}

LabelDocument readFixture(const QString &name)
{
    LabelReader reader;
    const auto document = reader.readFile(dataFile(name));
    if (!document) {
        qFatal("%s: %s", qPrintable(name), qPrintable(reader.errorString()));
    }
    return *document;
}

std::optional<LabelDocument> readBytes(const QByteArray &data, LabelReader &reader)
{
    QByteArray copy = data;
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    return reader.read(&buffer);
}

} // namespace

class LabelFormatTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsAddressLabel()
    {
        const LabelDocument document = readFixture(u"address.label"_s);

        QCOMPARE(document.orientation, PaperOrientation::Landscape);
        QCOMPARE(document.id, u"Address"_s);
        QCOMPARE(document.paperName, u"30252 Address"_s);
        QCOMPARE(document.paperSize(), QSizeF(1581, 5040));
        QCOMPARE(document.canvasSize(), QSizeF(5040, 1581));
        QVERIFY(!document.mediaType);
        QCOMPARE(document.objects.size(), 1);

        const auto *address = std::get_if<AddressObject>(&document.objects.at(0));
        QVERIFY(address);
        QCOMPARE(address->name, u"Address"_s);
        QVERIFY(address->variable);
        QCOMPARE(address->bounds, QRectF(332, 150, 4455, 1260));
        QCOMPARE(address->horizontalAlignment, HorizontalAlignment::Left);
        QCOMPARE(address->fitMode, TextFitMode::ShrinkToFit);
        QCOMPARE(address->plainText(), u"Jan Jansen\nDorpsstraat 1\n1234 AB Ergens"_s);
        QCOMPARE(address->lineFonts.size(), 3);
        QCOMPARE(address->spans.at(0).font.size, 12.0);
    }

    void readsMixedLabel()
    {
        LabelReader reader;
        const auto document = reader.readFile(dataFile(u"mixed.label"_s));
        QVERIFY2(document, qPrintable(reader.errorString()));

        QCOMPARE(document->orientation, PaperOrientation::Portrait);
        QCOMPARE(document->canvasSize(), QSizeF(3240, 1800));
        QCOMPARE(document->mediaType, std::optional<QString>(u"Default"_s));
        QCOMPARE(document->outlined, std::optional<bool>(false));
        QCOMPARE(document->objects.size(), 5);

        const auto *text = std::get_if<TextObject>(&document->objects.at(0));
        QVERIFY(text);
        QCOMPARE(text->groupId, std::optional<int>(-1));
        QCOMPARE(text->outlined, std::optional<bool>(false));
        QCOMPARE(text->verticalAlignment, VerticalAlignment::Middle); // written as "Center"
        QCOMPARE(text->spans.size(), 2);
        QVERIFY(text->spans.at(0).font.bold);
        QVERIFY(text->spans.at(1).font.italic);
        QCOMPARE(text->spans.at(1).text, u" second line "_s);

        const auto *barcode = std::get_if<BarcodeObject>(&document->objects.at(1));
        QVERIFY(barcode);
        QCOMPARE(barcode->text, u"KD-0042"_s);
        QCOMPARE(barcode->type, u"Code128Auto"_s);
        QCOMPARE(barcode->size, BarcodeSize::Medium);
        QCOMPARE(barcode->textPosition, BarcodeTextPosition::Bottom);
        QCOMPARE(barcode->textFont.size, 8.0);

        const auto *image = std::get_if<ImageObject>(&document->objects.at(2));
        QVERIFY(image);
        QVERIFY(!image->imageLocation);
        QCOMPARE(QImage::fromData(image->imageData).size(), QSize(16, 16));

        const auto *shape = std::get_if<ShapeObject>(&document->objects.at(3));
        QVERIFY(shape);
        QCOMPARE(shape->shapeType, ShapeType::HorizontalLine);
        QCOMPARE(shape->lineWidth, 48.0);

        const auto *unknown = std::get_if<UnknownObject>(&document->objects.at(4));
        QVERIFY(unknown);
        QCOMPARE(unknown->elementName, u"FutureObject"_s);
        QCOMPARE(unknown->bounds, QRectF(120, 1400, 3000, 300));

        QCOMPARE(reader.warnings().size(), 1);
        QVERIFY(reader.warnings().at(0).contains(u"FutureObject"_s));
    }

    void readsVariableObjects()
    {
        LabelReader reader;
        const auto document = reader.readFile(dataFile(u"variable.label"_s));
        QVERIFY2(document, qPrintable(reader.errorString()));
        QVERIFY2(reader.warnings().isEmpty(), qPrintable(reader.warnings().join(u'\n')));
        QCOMPARE(document->objects.size(), 3);

        const auto *counter = std::get_if<CounterObject>(&document->objects.at(0));
        QVERIFY(counter);
        QCOMPARE(counter->preText, u"No. "_s);
        QCOMPARE(counter->current, 7);
        QCOMPARE(counter->increment, 2);
        QCOMPARE(counter->formatWidth, 4);
        QVERIFY(counter->useLeadingZeros);
        QCOMPARE(counter->layout.horizontalAlignment, HorizontalAlignment::Left);
        QCOMPARE(counter->text(), u"No. 0007"_s);
        QCOMPARE(counter->text(3), u"No. 0013"_s);

        const auto *date = std::get_if<DateTimeObject>(&document->objects.at(1));
        QVERIFY(date);
        QCOMPARE(date->format, DateFormat::DayMonthLongYear);
        QCOMPARE(date->culture, u"nl-NL"_s);
        QVERIFY(date->includeTime);
        QCOMPARE(date->text(QDateTime(QDate(2026, 10, 28), QTime(14, 5))), u"Printed 28-10-2026 14:05"_s);

        const auto *curved = std::get_if<CircularTextObject>(&document->objects.at(2));
        QVERIFY(curved);
        QCOMPARE(curved->text, u"DYMO-compatible"_s);
        QCOMPARE(curved->mode, CircularTextMode::Circle);
        QCOMPARE(curved->circleAlignment, CircleAlignment::StartAngle);
        QCOMPARE(curved->startAngle, 45.0);
    }

    void dateFormats_data()
    {
        QTest::addColumn<DateFormat>("format");
        QTest::addColumn<QString>("culture");
        QTest::addColumn<QString>("expected");
        QTest::newRow("long en") << DateFormat::LongMonthDayLongYear << u"en-US"_s << u"October 28, 2026"_s;
        QTest::newRow("long nl") << DateFormat::DayLongMonthLongYear << u"nl-NL"_s << u"28 oktober 2026"_s;
        QTest::newRow("short us") << DateFormat::MonthDayYear << u"en-US"_s << u"10/28/26"_s;
        QTest::newRow("short nl") << DateFormat::DayMonthYear << u"nl-NL"_s << u"28-10-26"_s;
        QTest::newRow("weekday") << DateFormat::AbbrWeekdayDayAbbrMonthLongYear << u"en-GB"_s << u"Wed 28 Oct 2026"_s;
    }

    void dateFormats()
    {
        QFETCH(DateFormat, format);
        QFETCH(QString, culture);
        QFETCH(QString, expected);
        DateTimeObject date;
        date.format = format;
        date.culture = culture;
        QCOMPARE(date.text(QDateTime(QDate(2026, 10, 28), QTime(9, 30))), expected);
    }

    void roundTrip_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("address") << u"address.label"_s;
        QTest::newRow("mixed") << u"mixed.label"_s;
        QTest::newRow("variable") << u"variable.label"_s;
    }

    void roundTrip()
    {
        QFETCH(QString, fileName);
        const LabelDocument original = readFixture(fileName);

        const QByteArray saved = LabelWriter::toByteArray(original);
        LabelReader reader;
        const auto reread = readBytes(saved, reader);
        QVERIFY2(reread, qPrintable(reader.errorString()));
        QCOMPARE(*reread, original);

        // Writing is stable, so a second save is byte-identical.
        QCOMPARE(LabelWriter::toByteArray(*reread), saved);
    }

    void unknownObjectIsWrittenBack()
    {
        const QByteArray saved = LabelWriter::toByteArray(readFixture(u"mixed.label"_s));
        QVERIFY(saved.contains("<FutureObject>"));
        QVERIFY(saved.contains("<Setting>Value</Setting>"));
        QVERIFY(saved.contains("<PreText>Printed on </PreText>"));
    }

    void preservesSignificantWhitespace()
    {
        const QByteArray saved = LabelWriter::toByteArray(readFixture(u"mixed.label"_s));
        QVERIFY(saved.contains("<String xml:space=\"preserve\"> second line </String>"));
    }

    void readsTapeLabels()
    {
        // DLS's ContinuousLabel: a tape by name and a tree of cells.
        LabelReader reader;
        const auto document = reader.readFile(dataFile(u"tape.label"_s));
        QVERIFY2(document, qPrintable(reader.errorString()));
        QVERIFY2(reader.warnings().isEmpty(), qPrintable(reader.warnings().join(u"; "_s)));
        QVERIFY(document->tape);
        const TapeLabel &tape = *document->tape;
        QCOMPARE(tape.width, 700.0); // the driver's 12 mm tape: 35 points
        QCOMPARE(tape.lengthMode, CellLengthMode::Auto);
        QCOMPARE(document->objects.size(), 5);
        QCOMPARE(tape.root.subcells.size(), 4);
        const TapeCell &port = tape.root.subcells.at(1);
        QCOMPARE(port.lengthMode, CellLengthMode::Fixed);
        QCOMPARE(port.length, 1134.0);
        QCOMPARE(port.borderWidth, 20.0);
        QCOMPARE(objectBase(document->objects.at(port.object)).name, u"Port"_s);
        QCOMPARE(port.objectMargin, QMarginsF(72, 0, 72, 0));
        const TapeCell &stack = tape.root.subcells.at(3);
        QCOMPARE(stack.object, -1);
        QCOMPARE(stack.subcellsOrientation, Qt::Vertical);
        QCOMPARE(stack.subcells.size(), 2);

        // The bare minimum: one tape, no cells.
        QVERIFY(reader.readFile(dataFile(u"continuous.label"_s)));
    }

    void tapeLabelsRoundTrip()
    {
        LabelReader reader;
        const auto document = reader.readFile(dataFile(u"tape.label"_s));
        QVERIFY(document);
        const QByteArray written = LabelWriter::toByteArray(*document);
        QVERIFY(written.contains("<ContinuousLabel"));
        QVERIFY(!written.contains("<ObjectInfo>"));
        QBuffer buffer;
        buffer.setData(written);
        buffer.open(QIODevice::ReadOnly);
        const auto again = reader.read(&buffer);
        QVERIFY2(again, qPrintable(reader.errorString()));
        QVERIFY(*again == *document);
    }

    void tapeObjectsKeepTheirCells()
    {
        LabelReader reader;
        auto document = *reader.readFile(dataFile(u"tape.label"_s));
        // Removing "Port" (object 1) drops its cell and renumbers the rest.
        document.removeObject(1);
        QCOMPARE(document.objects.size(), 4);
        QCOMPARE(document.tape->root.subcells.size(), 3);
        QCOMPARE(objectBase(document.objects.at(document.tape->root.subcells.at(1).object)).name, u"Code"_s);
        // Reordering keeps every cell on its object.
        document.moveObject(0, 3);
        QCOMPARE(objectBase(document.objects.at(document.tape->root.subcells.at(0).object)).name, u"Name"_s);
        QCOMPARE(objectBase(document.objects.at(document.tape->root.subcells.at(1).object)).name, u"Code"_s);
        // A new object gets a cell at the end.
        const int added = document.addObject(TextObject());
        QCOMPARE(document.tape->root.subcells.constLast().object, added);
    }

    void rejectsOtherFiles_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("truncated") << QByteArray("<DieCutLabel Version=\"8.0\" Units=\"twips\"><Id>x</Id>");
        QTest::newRow("other root") << QByteArray("<html><body/></html>");
        QTest::newRow("no outline") << QByteArray("<DieCutLabel Version=\"8.0\" Units=\"twips\"><Id>x</Id></DieCutLabel>");
        QTest::newRow("other units") << QByteArray("<DieCutLabel Version=\"8.0\" Units=\"mm\"/>");
    }

    void rejectsOtherFiles()
    {
        QFETCH(QByteArray, data);
        LabelReader reader;
        QVERIFY(!readBytes(data, reader));
        QVERIFY(!reader.errorString().isEmpty());
    }

    void writesNewDocument()
    {
        LabelDocument document;
        document.orientation = PaperOrientation::Landscape;
        document.id = u"Address"_s;
        document.paperName = u"30252 Address"_s;
        document.paperOutline.addRoundedRect(QRectF(0, 0, 1581, 5040), 270, 270);

        TextObject text;
        text.name = u"Text"_s;
        text.spans = {{u"Hello"_s, LabelFont(), Qt::black}};
        text.bounds = QRectF(300, 150, 4400, 1200);
        document.objects.append(text);

        LabelReader reader;
        const auto reread = readBytes(LabelWriter::toByteArray(document), reader);
        QVERIFY2(reread, qPrintable(reader.errorString()));
        QCOMPARE(reread->canvasSize(), QSizeF(5040, 1581));
        QCOMPARE(reread->objects, document.objects);
    }

    // Set LABELCREATOR_CORPUS to one or more directories (separated by ':') of
    // real .label files to check that each of them survives a round trip.
    void corpus()
    {
        const QString corpus = qEnvironmentVariable("LABELCREATOR_CORPUS");
        if (corpus.isEmpty()) {
            QSKIP("LABELCREATOR_CORPUS is not set");
        }

        int checked = 0;
        for (const QString &directory : corpus.split(u':', Qt::SkipEmptyParts)) {
            const QDir dir(directory);
            for (const QString &name : dir.entryList({u"*.label"_s}, QDir::Files)) {
                LabelReader reader;
                const auto document = reader.readFile(dir.filePath(name));
                if (!document) {
                    qInfo("skipped %s: %s", qPrintable(name), qPrintable(reader.errorString()));
                    continue;
                }
                const auto reread = readBytes(LabelWriter::toByteArray(*document), reader);
                QVERIFY2(reread, qPrintable(name + u": "_s + reader.errorString()));
                QVERIFY2(*reread == *document, qPrintable(name));
                ++checked;
            }
        }
        qInfo("round-tripped %d files", checked);
        QVERIFY(checked > 0);
    }
};

QTEST_GUILESS_MAIN(LabelFormatTest)

#include "labelformattest.moc"
