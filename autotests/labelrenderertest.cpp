/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "fields.h"
#include "labelformat.h"
#include "labelprinter.h"
#include "labelrenderer.h"
#include "tapelayout.h"
#include "units.h"

#include <QBuffer>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QFontMetricsF>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr qreal Dpi = 300.0;

LabelDocument readFixture(const QString &name)
{
    LabelReader reader;
    const auto document = reader.readFile(QStringLiteral(LABELCREATOR_TEST_DATA "/") + name);
    if (!document) {
        qFatal("%s: %s", qPrintable(name), qPrintable(reader.errorString()));
    }
    return *document;
}

// A portrait label of the given size with one object on it.
LabelDocument singleObject(const LabelObject &object, const QSizeF &paper = QSizeF(3240, 1800))
{
    LabelDocument document;
    document.orientation = PaperOrientation::Portrait;
    document.paperOutline.addRect(QRectF(QPointF(0, 0), paper));
    document.objects.append(object);
    return document;
}

TextObject textObject(const QString &text, const QRectF &bounds, TextFitMode fitMode, qreal fontSize = 12)
{
    TextObject object;
    object.name = u"Text"_s;
    object.horizontalAlignment = HorizontalAlignment::Center;
    object.fitMode = fitMode;
    object.spans = {{text, LabelFont{u"DejaVu Sans"_s, fontSize}, Qt::black}};
    object.bounds = bounds;
    return object;
}

QRect toDots(const QRectF &twips)
{
    const qreal factor = Units::twipsToDots(1.0, Dpi);
    return QRectF(twips.topLeft() * factor, twips.size() * factor).toAlignedRect();
}

// Lengths of the black and white runs across one row, inside the ink.
QList<int> runLengths(const QImage &image, int y)
{
    QList<int> runs;
    int x = 0;
    while (x < image.width() && (image.pixel(x, y) & 0xffffff) != 0) {
        ++x;
    }
    while (x < image.width()) {
        const QRgb colour = image.pixel(x, y) & 0xffffff;
        int run = 0;
        while (x < image.width() && (image.pixel(x, y) & 0xffffff) == colour) {
            ++run;
            ++x;
        }
        if (x < image.width() || colour == 0) {
            runs << run;
        }
    }
    return runs;
}

BarcodeObject barcodeObject(const QString &type, const QString &text, BarcodeSize size)
{
    BarcodeObject barcode;
    barcode.name = u"Code"_s;
    barcode.type = type;
    barcode.text = text;
    barcode.size = size;
    barcode.textPosition = BarcodeTextPosition::None;
    barcode.horizontalAlignment = HorizontalAlignment::Left;
    barcode.bounds = QRectF(144, 144, 6000, 1440);
    return barcode;
}

// Bounding box of everything that is not white.
QRect inkBounds(const QImage &image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if ((image.pixel(x, y) & 0xffffff) != 0xffffff) {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }
    return bounds;
}

} // namespace

class LabelRendererTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void imageSizeFollowsCanvas()
    {
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(readFixture(u"address.label"_s), Dpi);
        // 3.5 x 1.1 inch, laid out landscape.
        QCOMPARE(image.size(), QSize(1050, 329));
    }

    void printIsBlackAndWhite()
    {
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(readFixture(u"mixed.label"_s), Dpi);

        int black = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = image.pixel(x, y) & 0xffffff;
                QVERIFY2(pixel == 0 || pixel == 0xffffff, qPrintable(u"grey pixel at %1,%2"_s.arg(x).arg(y)));
                black += pixel == 0;
            }
        }
        QVERIFY(black > 0);
        QVERIFY(renderer.warnings().isEmpty());
    }

    void inkStaysInsideObjects()
    {
        const LabelDocument document = readFixture(u"mixed.label"_s);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(document, Dpi);

        QRegion allowed;
        for (const LabelObject &object : document.objects) {
            allowed += toDots(objectBase(object).bounds).adjusted(-1, -1, 1, 1);
        }
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if ((image.pixel(x, y) & 0xffffff) == 0) {
                    QVERIFY2(allowed.contains(QPoint(x, y)), qPrintable(u"ink outside any object at %1,%2"_s.arg(x).arg(y)));
                }
            }
        }
    }

    void shrinkToFitKeepsLongTextInside()
    {
        const QRectF bounds(200, 200, 1440, 300);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(textObject(u"A rather long line that cannot possibly fit at 12 pt"_s, bounds, TextFitMode::ShrinkToFit)), Dpi);

        const QRect ink = inkBounds(image);
        QVERIFY(!ink.isEmpty());
        QVERIFY2(toDots(bounds).adjusted(-1, -1, 1, 1).contains(ink), qPrintable(u"ink %1,%2 %3x%4"_s.arg(ink.x()).arg(ink.y()).arg(ink.width()).arg(ink.height())));
        // Shrunk to the width, so it spans most of it.
        QVERIFY(ink.width() > toDots(bounds).width() * 0.9);
    }

    void shrinkToFitDoesNotGrow()
    {
        const QRectF bounds(200, 200, 2800, 1200);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(textObject(u"Hi"_s, bounds, TextFitMode::ShrinkToFit)), Dpi);

        // 12 pt is 50 dots at 300 dpi; the ink of "Hi" is well under that.
        QVERIFY(inkBounds(image).height() < 50);
    }

    void alwaysFitGrows()
    {
        const QRectF bounds(200, 200, 2800, 1200);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(textObject(u"Hi"_s, bounds, TextFitMode::AlwaysFit)), Dpi);

        const QRect ink = inkBounds(image);
        QVERIFY(toDots(bounds).adjusted(-1, -1, 1, 1).contains(ink));
        QVERIFY(ink.height() > toDots(bounds).height() * 0.6);
    }

    void noFitClipsToBounds()
    {
        const QRectF bounds(200, 200, 600, 300);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(textObject(u"Far too long for this box"_s, bounds, TextFitMode::None)), Dpi);

        QVERIFY(toDots(bounds).adjusted(-1, -1, 1, 1).contains(inkBounds(image)));
    }

    void horizontalLineSitsOnItsAlignment_data()
    {
        QTest::addColumn<LineAlignment>("alignment");
        QTest::addColumn<int>("expectedTop");

        // Bounds 480..960 twips = 100..200 dots; a 48 twip line is 10 dots.
        QTest::newRow("center") << LineAlignment::Center << 145;
        QTest::newRow("top") << LineAlignment::LeftOrTop << 100;
        QTest::newRow("bottom") << LineAlignment::RightOrBottom << 190;
    }

    void horizontalLineSitsOnItsAlignment()
    {
        QFETCH(LineAlignment, alignment);
        QFETCH(int, expectedTop);

        ShapeObject line;
        line.shapeType = ShapeType::HorizontalLine;
        line.lineWidth = 48;
        line.lineAlignment = alignment;
        line.bounds = QRectF(240, 480, 1440, 480);

        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QRect ink = inkBounds(renderer.renderImage(singleObject(line), Dpi));
        QCOMPARE(ink.top(), expectedTop);
        QCOMPARE(ink.height(), 10);
        QCOMPARE(ink.left(), 50);
        QCOMPARE(ink.width(), 300);
    }

    void quarterTurnTurnsText()
    {
        const QRectF bounds(400, 200, 1000, 1000);
        TextObject text = textObject(u"Sideways"_s, bounds, TextFitMode::AlwaysFit);

        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QRect upright = inkBounds(renderer.renderImage(singleObject(text), Dpi));
        text.rotation = Rotation::Rotation90;
        const QRect turned = inkBounds(renderer.renderImage(singleObject(text), Dpi));

        QVERIFY(upright.width() > upright.height() * 2);
        QVERIFY(turned.height() > turned.width() * 2);
    }

    // Bar widths as DLS 8 prints them at 300 dpi: whole dots from its table.
    void barcodeModulesInPrinterDots_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("text");
        QTest::addColumn<BarcodeSize>("size");
        QTest::addColumn<QList<int>>("widths");

        QTest::newRow("code128 small") << u"Code128Auto"_s << u"KD-0042"_s << BarcodeSize::Small << QList<int>{3};
        QTest::newRow("code128 medium") << u"Code128Auto"_s << u"KD-0042"_s << BarcodeSize::Medium << QList<int>{5};
        QTest::newRow("code128 large") << u"Code128Auto"_s << u"KD-0042"_s << BarcodeSize::Large << QList<int>{6};
        QTest::newRow("ean13 medium") << u"Ean13"_s << u"5901234123457"_s << BarcodeSize::Medium << QList<int>{5};
        // Two-width symbologies use DLS's ratio, not zint's.
        QTest::newRow("code39 small") << u"Code39"_s << u"KD42"_s << BarcodeSize::Small << QList<int>{3, 6};
        QTest::newRow("2of5 small") << u"Code2of5"_s << u"12345678"_s << BarcodeSize::Small << QList<int>{3, 6};
        QTest::newRow("codabar medium") << u"Codabar"_s << u"A1234B"_s << BarcodeSize::Medium << QList<int>{5, 10};
    }

    void barcodeModulesInPrinterDots()
    {
        QFETCH(QString, type);
        QFETCH(QString, text);
        QFETCH(BarcodeSize, size);
        QFETCH(QList<int>, widths);

        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(barcodeObject(type, text, size), QSizeF(6480, 1800)), Dpi);
        QVERIFY2(renderer.warnings().isEmpty(), qPrintable(renderer.warnings().join(u'\n')));

        const QRect ink = inkBounds(image);
        const QList<int> runs = runLengths(image, ink.center().y());
        QVERIFY(!runs.isEmpty());
        // Single-width symbologies use multiples of the module; two-width
        // ones only the narrow and wide element.
        const int narrow = widths.first();
        for (int run : runs) {
            if (widths.size() == 1) {
                QVERIFY2(run % narrow == 0, qPrintable(u"run of %1 dots"_s.arg(run)));
            } else {
                QVERIFY2(widths.contains(run), qPrintable(u"run of %1 dots"_s.arg(run)));
            }
        }
        QCOMPARE(*std::min_element(runs.cbegin(), runs.cend()), narrow);
    }

    void moreBarcodeTypesDraw_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("text");
        // DLS's US postal codes, then the types beyond DLS.
        QTest::newRow("postnet") << u"Postnet"_s << u"12345678901"_s;
        QTest::newRow("planet") << u"Planet"_s << u"12345678901"_s;
        QTest::newRow("imail") << u"IMail"_s << u"01234567094987654321-01234567891"_s;
        QTest::newRow("datamatrix") << u"DataMatrix"_s << u"SRV-HV01"_s;
        QTest::newRow("aztec") << u"Aztec"_s << u"SRV-HV01"_s;
        QTest::newRow("maxicode") << u"MaxiCode"_s << u"SRV-HV01"_s;
        QTest::newRow("micro qr") << u"MicroQRCode"_s << u"12345"_s;
        QTest::newRow("code93") << u"Code93"_s << u"SRV-HV01"_s;
        QTest::newRow("code11") << u"Code11"_s << u"123-45"_s;
        QTest::newRow("msi") << u"MsiPlessey"_s << u"123456"_s;
        QTest::newRow("pharmacode") << u"Pharmacode"_s << u"1234"_s;
        QTest::newRow("gs1-128") << u"GS1_128"_s << u"[01]09501101530003[17]251231"_s;
        QTest::newRow("gs1 datamatrix") << u"GS1DataMatrix"_s << u"[01]09501101530003[21]SERIAL1"_s;
        QTest::newRow("gs1 databar") << u"GS1DataBar"_s << u"0950110153000"_s;
        QTest::newRow("gs1 databar expanded") << u"GS1DataBarExpanded"_s << u"[01]09501101530003[3103]000123"_s;
        QTest::newRow("hibc128") << u"HIBC128"_s << u"A123BJC5D6E71"_s;
        QTest::newRow("hibc39") << u"HIBC39"_s << u"A123BJC5D6E71"_s;
        QTest::newRow("hibc datamatrix") << u"HIBCDataMatrix"_s << u"A123BJC5D6E71"_s;
        QTest::newRow("hibc qr") << u"HIBCQRCode"_s << u"A123BJC5D6E71"_s;
        QTest::newRow("kix") << u"KIX"_s << u"2500GG30250"_s;
        QTest::newRow("royal mail") << u"RoyalMail"_s << u"LE28HS9Z"_s;
    }

    void moreBarcodeTypesDraw()
    {
        QFETCH(QString, type);
        QFETCH(QString, text);
        BarcodeObject barcode = barcodeObject(type, text, BarcodeSize::Medium);
        barcode.textPosition = BarcodeTextPosition::None;
        const LabelDocument document = singleObject(barcode, QSizeF(6480, 1800));
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(document, Dpi);
        QVERIFY2(renderer.warnings().isEmpty(), qPrintable(renderer.warnings().join(u'\n')));
        QVERIFY(!inkBounds(image).isEmpty());

        // The type is kept as written in the file.
        LabelReader reader;
        QBuffer buffer;
        buffer.setData(LabelWriter::toByteArray(document));
        buffer.open(QIODevice::ReadOnly);
        const auto reread = reader.read(&buffer);
        QVERIFY(reread);
        QCOMPARE(std::get_if<BarcodeObject>(&reread->objects.constFirst())->type, type);
    }

    void rotatingTheLabelPrintsTheSame_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("landscape address") << u"address.label"_s;
        QTest::newRow("portrait, every object") << u"mixed.label"_s;
        QTest::newRow("counter, date, curved text") << u"variable.label"_s;
        QTest::newRow("tape with cells") << u"tape.label"_s;
    }

    void rotatingTheLabelPrintsTheSame()
    {
        QFETCH(QString, fileName);
        LabelDocument original = readFixture(fileName);
        TapeLayout::layout(original);
        // The paper as the label feeds, in points.
        PaperSize paper;
        paper.size = original.paperSize() / Units::TwipsPerPoint;
        const QDateTime when(QDate(2026, 10, 29), QTime(12, 0));
        const QImage before = LabelPrinter::paperImage(original, paper, Dpi, nullptr, 0, when);

        LabelDocument turned = original;
        turned.rotateQuarter();
        QVERIFY(turned.orientation != original.orientation);
        QCOMPARE(turned.canvasSize(), original.canvasSize().transposed());
        TapeLayout::layout(turned);
        const QImage after = LabelPrinter::paperImage(turned, paper, Dpi, nullptr, 0, when);
        QCOMPARE(after.size(), before.size());
        // Glyph edges may round a dot differently; everything else is the same.
        int different = 0;
        int ink = 0;
        for (int y = 0; y < before.height(); ++y) {
            for (int x = 0; x < before.width(); ++x) {
                different += before.pixelIndex(x, y) != after.pixelIndex(x, y) ? 1 : 0;
                ink += qGray(before.pixel(x, y)) < 128 ? 1 : 0;
            }
        }
        QVERIFY(ink > 100);
        QVERIFY2(different <= ink / 50, qPrintable(u"%1 of %2 ink dots differ"_s.arg(different).arg(ink)));

        // Turned back, it is the label it was.
        turned.rotateQuarter();
        TapeLayout::layout(turned);
        QCOMPARE(turned.orientation, original.orientation);
        for (qsizetype i = 0; i < original.objects.size(); ++i) {
            QCOMPARE(objectBase(turned.objects.at(i)).rotation, objectBase(original.objects.at(i)).rotation);
            const QRectF a = objectBase(turned.objects.at(i)).bounds;
            const QRectF b = objectBase(original.objects.at(i)).bounds;
            QVERIFY2(qAbs(a.x() - b.x()) < 0.01 && qAbs(a.y() - b.y()) < 0.01 && qAbs(a.width() - b.width()) < 0.01 && qAbs(a.height() - b.height()) < 0.01,
                     qPrintable(objectBase(original.objects.at(i)).name));
        }
        if (original.tape) {
            QCOMPARE(turned.tape->root, original.tape->root);
        }
    }

    void picturesComeFromDataRows()
    {
        // Two pictures, one per row: black and white.
        QTemporaryDir dir;
        QImage black(40, 40, QImage::Format_RGB32);
        black.fill(Qt::black);
        QImage white(40, 40, QImage::Format_RGB32);
        white.fill(Qt::white);
        QVERIFY(black.save(dir.filePath(u"SRV-1.png"_s)));
        QVERIFY(white.save(dir.filePath(u"SRV-2.png"_s)));

        ImageObject image;
        image.name = u"Photo"_s;
        image.imageLocation = dir.path() + u"/{Serial}.png"_s;
        image.scaleMode = ImageScaleMode::Fill;
        image.bounds = QRectF(0, 0, 1000, 1000);
        const LabelDocument label = singleObject(image);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const auto inkOf = [&renderer, &label](const QString &serial) {
            return inkBounds(renderer.renderImage(Fields::fill(label, {{u"Serial"_s, serial}}), Dpi));
        };
        QVERIFY(!inkOf(u"SRV-1"_s).isEmpty());
        QVERIFY(renderer.warnings().isEmpty());
        QVERIFY(inkOf(u"SRV-2"_s).isEmpty());
    }

    void writesAPdfOfTheLabelsSize()
    {
        QTemporaryDir dir;
        const QString fileName = dir.filePath(u"label.pdf"_s);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        QVERIFY(renderer.writePdf(readFixture(u"address.label"_s), fileName, u"Address"_s));
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray pdf = file.readAll();
        QVERIFY(pdf.startsWith("%PDF-"));
        // 30252 Address on screen: 5040 x 1581 twips is 252 x 79.05 points.
        const QRegularExpression mediaBox(u"/MediaBox \\[0 0 ([\\d.]+) ([\\d.]+)\\]"_s);
        const QRegularExpressionMatch match = mediaBox.match(QString::fromLatin1(pdf));
        QVERIFY2(match.hasMatch(), "no MediaBox");
        QVERIFY(qAbs(match.captured(1).toDouble() - 252) < 0.5);
        QVERIFY(qAbs(match.captured(2).toDouble() - 79.05) < 0.5);
        // The text stays text, not an image.
        QVERIFY(!pdf.contains("/Subtype /Image"));
    }

    void barcodeTextUsesTheObjectFont_data()
    {
        QTest::addColumn<BarcodeTextPosition>("position");
        QTest::newRow("bottom") << BarcodeTextPosition::Bottom;
        QTest::newRow("top") << BarcodeTextPosition::Top;
    }

    void barcodeTextUsesTheObjectFont()
    {
        QFETCH(BarcodeTextPosition, position);

        BarcodeObject barcode = barcodeObject(u"Code128B"_s, u"123456"_s, BarcodeSize::Medium);
        barcode.textPosition = position;
        barcode.textFont = LabelFont{u"DejaVu Sans"_s, 8};
        barcode.bounds = QRectF(144, 144, 3000, 1000);

        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(barcode), Dpi);
        const QRect box = toDots(barcode.bounds);
        const QRect ink = inkBounds(image);
        QVERIFY(box.adjusted(-1, -1, 1, 1).contains(ink));

        // The first bar leaves exactly one line of 8 pt text free.
        const int textDots = qRound(Units::twipsToDots(QFontMetricsF(toQFont(barcode.textFont)).height(), Dpi));
        int barTop = -1;
        int barBottom = -1;
        for (int y = box.top(); y <= box.bottom(); ++y) {
            if ((image.pixel(ink.left(), y) & 0xffffff) == 0) {
                barBottom = y;
                if (barTop < 0) {
                    barTop = y;
                }
            }
        }
        QVERIFY(barTop >= 0);
        QVERIFY(qAbs((barBottom - barTop + 1) - (box.height() - textDots)) <= 2);

        // And the text is there, in the strip the bars leave free.
        const int stripTop = position == BarcodeTextPosition::Bottom ? barBottom + 1 : box.top();
        const int stripBottom = position == BarcodeTextPosition::Bottom ? box.bottom() : barTop - 1;
        int textInk = 0;
        for (int y = stripTop; y <= stripBottom; ++y) {
            for (int x = box.left(); x <= box.right(); ++x) {
                textInk += (image.pixel(x, y) & 0xffffff) == 0;
            }
        }
        QVERIFY(textInk > 0);
    }

    void eanGuardBarsReachIntoTheText()
    {
        BarcodeObject barcode = barcodeObject(u"Ean13"_s, u"12312312"_s, BarcodeSize::Medium);
        barcode.textPosition = BarcodeTextPosition::Bottom;
        barcode.textFont = LabelFont{u"DejaVu Sans"_s, 8};
        barcode.bounds = QRectF(144, 144, 3000, 1000);

        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(barcode), Dpi);
        QVERIFY2(renderer.warnings().isEmpty(), qPrintable(renderer.warnings().join(u'\n')));
        const QRect box = toDots(barcode.bounds);
        QVERIFY(box.adjusted(-1, -1, 1, 1).contains(inkBounds(image)));

        // Bar bottoms along the row just under the top of the bars.
        const int textDots = qRound(Units::twipsToDots(QFontMetricsF(toQFont(barcode.textFont)).height(), Dpi));
        const int row = inkBounds(image).top() + 2;
        int longest = 0;
        int shortest = box.bottom();
        for (int x = box.left(); x <= box.right(); ++x) {
            if ((image.pixel(x, row) & 0xffffff) != 0) {
                continue;
            }
            int y = row;
            while (y + 1 <= box.bottom() && (image.pixel(x, y + 1) & 0xffffff) == 0) {
                ++y;
            }
            longest = std::max(longest, y);
            shortest = std::min(shortest, y);
        }
        // Guard bars run down to the bottom; normal bars stop a line of text
        // above it.
        QVERIFY(longest >= box.bottom() - 1);
        QVERIFY(qAbs((box.bottom() - shortest) - textDots) <= 2);
    }

    void qrModulesInPrinterDots()
    {
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(barcodeObject(u"QRCode"_s, u"LABELCREATOR"_s, BarcodeSize::Small)), Dpi);

        // Small QR modules are 4 dots; a version 1 symbol is 21 modules.
        const QRect ink = inkBounds(image);
        QCOMPARE(ink.width(), 21 * 4);
        QCOMPARE(ink.height(), 21 * 4);
    }

    void unsupportedBarcodeTypeWarns()
    {
        BarcodeObject barcode;
        barcode.name = u"Code"_s;
        barcode.type = u"Telepen"_s;
        barcode.text = u"123"_s;
        barcode.bounds = QRectF(200, 200, 2000, 600);

        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(singleObject(barcode), Dpi);
        QCOMPARE(renderer.warnings().size(), 1);
        // A print never shows the placeholder.
        QVERIFY(inkBounds(image).isEmpty());
    }

    void invalidBarcodeDataWarns()
    {
        BarcodeObject barcode;
        barcode.name = u"Code"_s;
        barcode.type = u"Ean13"_s;
        barcode.text = u"not a number"_s;
        barcode.bounds = QRectF(200, 200, 2000, 600);

        LabelRenderer renderer(LabelRenderer::Mode::Preview);
        renderer.renderImage(singleObject(barcode), Dpi);
        QCOMPARE(renderer.warnings().size(), 1);
        QVERIFY(renderer.warnings().at(0).startsWith(u"Code: "_s));
    }

    void previewShowsPaperShape()
    {
        LabelRenderer renderer(LabelRenderer::Mode::Preview);
        const QImage image = renderer.renderImage(readFixture(u"address.label"_s), Dpi);

        // Outside the rounded corner is transparent, the middle is paper.
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        QCOMPARE(image.pixel(image.width() / 2, 5) | 0xff000000, 0xffffffffu);
    }

    void laysOutTapeCells()
    {
        LabelDocument document = readFixture(u"tape.label"_s);
        TapeLayout::layout(document);
        const TapeLabel &tape = *document.tape;
        const QRectF band = TapeLayout::printableBand(document);
        // 12 mm tape: the driver prints 2.8 to 31.6 points of its 35.
        QCOMPARE(band.height(), (31.6 - 2.8) * 20);
        QCOMPARE(band.top(), (35 - 31.6) * 20);

        // Cells follow each other along the tape, filling the label.
        qreal position = 0;
        for (const TapeCell &cell : tape.root.subcells) {
            QCOMPARE(cell.bounds.left(), position);
            QCOMPARE(cell.bounds.top(), band.top());
            QCOMPARE(cell.bounds.height(), band.height());
            position += cell.bounds.width();
        }
        QCOMPARE(position, tape.length);
        QCOMPARE(tape.root.subcells.at(1).bounds.width(), 1134.0); // the fixed one

        // Objects sit in their cells, within the band, minus the margins.
        for (const TapeCell &cell : tape.root.subcells) {
            if (cell.object >= 0) {
                const QRectF bounds = objectBase(document.objects.at(cell.object)).bounds;
                QCOMPARE(bounds, cell.bounds.marginsRemoved(cell.objectMargin));
            }
        }
        // Stacked across the tape: two halves of the band.
        const TapeCell &stack = tape.root.subcells.at(3);
        QCOMPARE(stack.subcells.at(0).bounds.height(), band.height() / 2);
        QCOMPARE(stack.subcells.at(1).bounds.top(), band.top() + band.height() / 2);
    }

    void tapeLabelsGrowWithTheirText()
    {
        LabelDocument document = TapeLayout::newLabel(12);
        TextObject text;
        text.fitMode = TextFitMode::AlwaysFit;
        text.spans = {{u"A"_s, LabelFont(), Qt::black}};
        document.addObject(text);
        TapeLayout::layout(document);
        const qreal short_ = document.tape->length;
        std::get<TextObject>(document.objects[0]).spans[0].text = u"A much longer text for the label"_s;
        TapeLayout::layout(document);
        QVERIFY(document.tape->length > 5 * short_);
        // As long as the text at the band's height needs, plus its margins.
        const QRectF bounds = objectBase(document.objects.at(0)).bounds;
        QFont font = toQFont(LabelFont());
        const QFontMetricsF metrics(font);
        const qreal expected = metrics.horizontalAdvance(u"A much longer text for the label"_s) * TapeLayout::printableBand(document).height() / metrics.lineSpacing();
        QVERIFY(qAbs(bounds.width() - expected) < 1);
    }

    void starCellsShareAFixedLabel()
    {
        LabelDocument document = TapeLayout::newLabel(9);
        document.tape->lengthMode = CellLengthMode::Fixed;
        document.tape->length = 3 * 1440; // 3 inches
        for (int i = 0; i < 3; ++i) {
            document.addObject(TextObject());
        }
        document.tape->root.subcells[0].lengthMode = CellLengthMode::Fixed;
        document.tape->root.subcells[0].length = 1440;
        document.tape->root.subcells[1].lengthMode = CellLengthMode::Star;
        document.tape->root.subcells[2].lengthMode = CellLengthMode::Star;
        TapeLayout::layout(document);
        QCOMPARE(document.tape->length, 3 * 1440.0);
        QCOMPARE(document.tape->root.subcells.at(1).bounds.width(), 1440.0);
        QCOMPARE(document.tape->root.subcells.at(2).bounds.width(), 1440.0);
    }

    void tapeBordersPrint()
    {
        LabelDocument document = TapeLayout::newLabel(12);
        document.addObject(TextObject());
        document.tape->lengthMode = CellLengthMode::Fixed;
        document.tape->length = 1440;
        document.tape->root.subcells[0].lengthMode = CellLengthMode::Star;
        document.tape->root.subcells[0].borderWidth = 20; // one point
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(document, 180);
        // The label is 1 inch long: 180 dots; the border runs along the band's edges.
        QCOMPARE(image.width(), 180);
        int black = 0;
        for (int x = 0; x < image.width(); ++x) {
            black += image.pixelColor(x, image.height() / 2).value() < 128 ? 1 : 0;
        }
        QVERIFY(black >= 2); // the cell's left and right borders
    }
};

QTEST_MAIN(LabelRendererTest)

#include "labelrenderertest.moc"
