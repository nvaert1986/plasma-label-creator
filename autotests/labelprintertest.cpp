/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelformat.h"
#include "labelprinter.h"
#include "tapelayout.h"

#include <QElapsedTimer>
#include <QTemporaryFile>
#include <QTest>

#include <cups/raster.h>

#include <fcntl.h>
#include <unistd.h>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr qreal Dpi = 300.0;

// The 30252 and 30334 entries of the LabelWriter 450 driver.
const PaperSize Address{u"w79h252"_s, u"30252 Address"_s, QSizeF(78.96, 252), QRectF(QPointF(4.32, 16.56), QPointF(76.08, 247.68)), QString(), QRectF(), 0, {}};
const PaperSize Small{u"30334_2-1_4_in_x_1-1_4_in"_s, u"30334 2-1/4 in x 1-1/4 in"_s, QSizeF(162, 90), QRectF(QPointF(2.88, 4.32), QPointF(159.12, 85.68)), QString(), QRectF(), 0, {}};

LabelDocument document(PaperOrientation orientation, const QSizeF &paper)
{
    LabelDocument document;
    document.orientation = orientation;
    document.paperOutline.addRect(QRectF(QPointF(0, 0), paper));
    return document;
}

// A filled rectangle where canvas objects are placed.
LabelDocument withBlock(LabelDocument document, const QRectF &bounds)
{
    ShapeObject block;
    block.shapeType = ShapeType::Rectangle;
    block.lineWidth = 1;
    block.fillColor = Qt::black;
    block.bounds = bounds;
    document.objects.append(block);
    return document;
}

QRect inkBounds(const QImage &image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) == qRgb(0, 0, 0)) {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }
    return bounds;
}

} // namespace

class LabelPrinterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void matchesByPaperName()
    {
        LabelDocument label = document(PaperOrientation::Landscape, QSizeF(1581, 5040));
        label.paperName = u"30252 Address"_s;
        QCOMPARE(LabelPrinter::matchPaper(label, {Small, Address}), std::optional<PaperSize>(Address));
    }

    void matchesBySizeWhenTheNameIsUnknown()
    {
        LabelDocument label = document(PaperOrientation::Portrait, QSizeF(3240, 1800));
        label.paperName = u"Some custom name"_s;
        QCOMPARE(LabelPrinter::matchPaper(label, {Address, Small}), std::optional<PaperSize>(Small));
    }

    void noMatch()
    {
        LabelDocument label = document(PaperOrientation::Portrait, QSizeF(5000, 5000));
        label.paperName = u"Unknown"_s;
        QVERIFY(!LabelPrinter::matchPaper(label, {Address, Small}));
    }

    void portraitIsDrawnAsIs()
    {
        const LabelDocument label = withBlock(document(PaperOrientation::Portrait, QSizeF(3240, 1800)), QRectF(144, 144, 480, 240));
        const QImage page = LabelPrinter::paperImage(label, Small, Dpi);

        QCOMPARE(page.size(), QSize(675, 375));
        QCOMPARE(page.format(), QImage::Format_Mono);
        QCOMPARE(inkBounds(page), QRect(30, 30, 100, 50));
    }

    void landscapeLeftEdgeLeadsThroughThePrinter()
    {
        // A block at the canvas's top-left corner. DLS puts the canvas's left
        // edge at the paper's leading edge (the first rows) and its top edge
        // at the paper's right side.
        const LabelDocument label = withBlock(document(PaperOrientation::Landscape, QSizeF(1581, 5040)), QRectF(0, 0, 480, 240));
        const QImage page = LabelPrinter::paperImage(label, Address, Dpi);

        QCOMPARE(page.size(), QSize(329, 1050));
        const QRect ink = inkBounds(page);
        QCOMPARE(ink.top(), 0);
        QCOMPARE(ink.height(), 100);
        QCOMPARE(ink.width(), 50);
        QVERIFY(ink.right() >= page.width() - 2);
    }

    void writesOneBitRaster()
    {
        const LabelDocument label = withBlock(document(PaperOrientation::Portrait, QSizeF(3240, 1800)), QRectF(144, 144, 480, 240));
        const QImage page = LabelPrinter::paperImage(label, Small, Dpi);

        QTemporaryFile file;
        QVERIFY(file.open());
        QString error;
        QVERIFY2(LabelPrinter::writeRaster(file.handle(), page, 2, Small, Dpi, &error), qPrintable(error));
        file.close();

        const int fd = ::open(QFile::encodeName(file.fileName()).constData(), O_RDONLY);
        QVERIFY(fd >= 0);
        cups_raster_t *raster = cupsRasterOpen(fd, CUPS_RASTER_READ);
        QVERIFY(raster);

        int pages = 0;
        cups_page_header2_t header;
        while (cupsRasterReadHeader2(raster, &header)) {
            ++pages;
            QCOMPARE(QByteArray(header.cupsPageSizeName), QByteArray("30334_2-1_4_in_x_1-1_4_in"));
            QCOMPARE(header.HWResolution[0], 300u);
            QCOMPARE(header.PageSize[0], 162u);
            QCOMPARE(header.PageSize[1], 90u);
            QCOMPARE(header.cupsWidth, 675u);
            QCOMPARE(header.cupsHeight, 375u);
            QCOMPARE(header.cupsBitsPerPixel, 1u);
            QCOMPARE(header.cupsBytesPerLine, 85u);
            QCOMPARE(int(header.cupsColorSpace), int(CUPS_CSPACE_K));

            // Row 30 runs through the block: dots 30 to 129 are set.
            QByteArray line(int(header.cupsBytesPerLine), '\0');
            int black = 0;
            for (unsigned y = 0; y < header.cupsHeight; ++y) {
                QCOMPARE(cupsRasterReadPixels(raster, reinterpret_cast<unsigned char *>(line.data()), header.cupsBytesPerLine), header.cupsBytesPerLine);
                for (int x = 0; x < int(header.cupsWidth); ++x) {
                    const bool set = uchar(line.at(x >> 3)) & (0x80 >> (x & 7));
                    black += set;
                    if (y == 30) {
                        QCOMPARE(set, x >= 30 && x < 130);
                    }
                }
            }
            QCOMPARE(black, 100 * 50);
        }
        cupsRasterClose(raster);
        ::close(fd);
        QCOMPARE(pages, 2);
    }
    void countersAdvancePerLabel()
    {
        LabelDocument label = document(PaperOrientation::Portrait, QSizeF(3240, 1800));
        CounterObject counter;
        counter.font = LabelFont{u"DejaVu Sans"_s, 20};
        counter.current = 1;
        counter.bounds = QRectF(144, 144, 2880, 1440);
        label.objects.append(counter);

        const QImage first = LabelPrinter::paperImage(label, Small, Dpi, nullptr, 0);
        const QImage second = LabelPrinter::paperImage(label, Small, Dpi, nullptr, 1);
        const QImage again = LabelPrinter::paperImage(label, Small, Dpi, nullptr, 0);
        QVERIFY(first != second);
        QCOMPARE(first, again);
    }
    void guessesTheDriver_data()
    {
        QTest::addColumn<QStringList>("hints");
        QTest::addColumn<QString>("expected");
        QTest::newRow("usb uri") << QStringList{u"usb://DYMO/LabelWriter%20450%20Turbo?serial=01010112345600"_s} << u"lw450t.ppd"_s;
        QTest::newRow("make and model") << QStringList{u"DYMO LabelWriter 450"_s} << u"lw450.ppd"_s;
        QTest::newRow("usb product") << QStringList{u"Local Raw Printer"_s, u"LabelWriter 450 DUO Label"_s} << u"lw450dl.ppd"_s;
        QTest::newRow("twin turbo") << QStringList{u"LabelWriter 450 Twin Turbo"_s} << u"lw450tt.ppd"_s;
        QTest::newRow("no dymo") << QStringList{u"Brother MFC-L2700DW"_s, u"socket://192.0.2.1:9100"_s} << QString();
    }

    void guessesTheDriver()
    {
        QFETCH(QStringList, hints);
        QFETCH(QString, expected);
        const QList<LabelWriterDriver> drivers = {
            {u"lw450.ppd"_s, u"DYMO LabelWriter 450"_s},
            {u"lw450t.ppd"_s, u"DYMO LabelWriter 450 Turbo"_s},
            {u"lw450tt.ppd"_s, u"DYMO LabelWriter 450 Twin Turbo"_s},
            {u"lw450dl.ppd"_s, u"DYMO LabelWriter 450 DUO Label"_s},
        };
        const auto driver = LabelPrinter::guessDriver(hints, drivers);
        QCOMPARE(driver ? driver->ppdName : QString(), expected);
    }

    // Needs CUPS with the DYMO driver installed, as on the development machine.
    void convertsWithTheDymoDriver()
    {
        const QList<LabelWriterDriver> drivers = LabelPrinter::installedDrivers();
        const auto driver = LabelPrinter::guessDriver({u"LabelWriter 450 Turbo"_s}, drivers);
        if (!driver || LabelPrinter::filterProgram().isEmpty()) {
            QSKIP("The DYMO LabelWriter driver is not installed");
        }

        const LabelDocument label = withBlock(document(PaperOrientation::Portrait, QSizeF(3240, 1800)), QRectF(144, 144, 480, 240));
        QTemporaryFile raster;
        QVERIFY(raster.open());
        QVERIFY(LabelPrinter::writeRaster(raster.handle(), LabelPrinter::paperImage(label, Small, Dpi), 1, Small, Dpi));
        raster.close();

        QTemporaryFile output;
        QVERIFY(output.open());
        output.close();
        QString error;
        QVERIFY2(LabelPrinter::convertWithDriver(raster.fileName(), driver->ppdName, Small, {}, output.fileName(), &error), qPrintable(error));

        QFile data(output.fileName());
        QVERIFY(data.open(QIODevice::ReadOnly));
        const QByteArray bytes = data.readAll();
        // LabelWriter commands: ESC L sets the label length, ESC E feeds
        // to the next label (LabelWriter 450 technical reference).
        QVERIFY(bytes.startsWith('\x1b'));
        QVERIFY(bytes.contains("\x1bL"));
        QVERIFY(bytes.contains("\x1b" "E"));
    }

    void knowsEachDriversFilter()
    {
        const QList<LabelWriterDriver> drivers = LabelPrinter::installedDrivers();
        const auto has = [&drivers](const QString &ppd) {
            return std::any_of(drivers.cbegin(), drivers.cend(), [&ppd](const LabelWriterDriver &driver) {
                return driver.ppdName == ppd;
            });
        };
        if (!has(u"lw450t.ppd"_s) || !has(u"lw550t.ppd"_s)) {
            QSKIP("Needs both DYMO drivers: net-print/dymo-cups-drivers and net-print/dymo-lw5xx-cups-drivers");
        }
        QCOMPARE(LabelPrinter::driverFilter(u"lw450t.ppd"_s), u"raster2dymolw"_s);
        QCOMPARE(LabelPrinter::driverFilter(u"lw550t.ppd"_s), u"raster2dymolw_v2"_s);
        QVERIFY(!LabelPrinter::needsDirectConnection(u"raster2dymolw"_s));
        QVERIFY(LabelPrinter::needsDirectConnection(u"raster2dymolw_v2"_s));
        QVERIFY(LabelPrinter::driverFilter(u"no-such-driver.ppd"_s).isEmpty());
        // The 550 Turbo is told apart from the 450 Turbo by its model number.
        QCOMPARE(LabelPrinter::guessDriver({u"usb://DYMO/LabelWriter%20550%20Turbo?serial=1"_s}, drivers)->ppdName, u"lw550t.ppd"_s);
    }

    void convertsForAZebraLabelPrinter()
    {
        // CUPS's own ZPL driver, which Altec printers understand too; a
        // 50 x 25 mm label it does not list, at 600 dpi.
        if (LabelPrinter::filterProgram(u"rastertolabel"_s).isEmpty() || LabelPrinter::driverFilter(u"drv:///sample.drv/zebra.ppd"_s) != u"rastertolabel"_s) {
            QSKIP("CUPS's label drivers (rastertolabel) are not installed");
        }
        const PaperSize paper = PaperSize::custom(u"Altec 50 x 25 mm"_s, QSizeF(50, 25));
        QCOMPARE(paper.keyword, u"Custom.50x25mm"_s);
        QVERIFY(paper.isCustom());
        QVERIFY(qAbs(paper.size.width() - 141.73) < 0.01);

        LabelDocument label = withBlock(document(PaperOrientation::Portrait, QSizeF(2835, 1417.5)), QRectF(144, 144, 480, 240));
        QTemporaryFile raster;
        QVERIFY(raster.open());
        QVERIFY(LabelPrinter::writeRaster(raster.handle(), LabelPrinter::paperImage(label, paper, 600), 1, paper, 600));
        raster.close();
        QTemporaryFile output;
        QVERIFY(output.open());
        output.close();
        QString error;
        QVERIFY2(LabelPrinter::convertWithDriver(raster.fileName(), u"drv:///sample.drv/zebra.ppd"_s, paper, {}, output.fileName(), &error), qPrintable(error));

        QFile data(output.fileName());
        QVERIFY(data.open(QIODevice::ReadOnly));
        const QByteArray zpl = data.readAll();
        // One label 50 mm wide and 25 mm long at 600 dpi: 1181 x 591 dots,
        // sent as one stored graphic and printed.
        // The driver sends no label length; the printer finds the gaps itself.
        QVERIFY2(zpl.contains("~DGR:CUPS.GRF,87468,148,"), zpl.left(100).constData()); // 148 bytes x 591 lines
        QVERIFY(zpl.contains("^PW1181"));
        QVERIFY(zpl.contains("^MNY")); // gap sensing
        QVERIFY(zpl.contains("^XGR:CUPS.GRF,1,1^FS"));
        QVERIFY(zpl.contains("^XZ"));
    }

    void readsTheDriversResolution()
    {
        // CUPS's ZPL driver defaults to 203 dpi; the fallback is 300.
        if (LabelPrinter::driverFilter(u"drv:///sample.drv/zebra.ppd"_s).isEmpty()) {
            QSKIP("CUPS's label drivers are not installed");
        }
        LabelPrinter printer(u"unused"_s, u"drv:///sample.drv/zebra.ppd"_s);
        QVERIFY2(printer.load(), qPrintable(printer.errorString()));
        QCOMPARE(printer.resolution(), 203.0);
        QVERIFY(printer.supportsCustomSizes());

        // A label it does not list prints at its own size: Altec's wrap
        // label, 41 mm across the roll and 41.5 mm along it.
        LabelDocument wrap = document(PaperOrientation::Landscape, QSizeF(41 * 1440 / 25.4, 41.5 * 1440 / 25.4));
        wrap.paperName = u"Wrap label 41 × 41.5 mm"_s;
        const auto paper = printer.paperFor(wrap);
        QVERIFY(paper);
        QCOMPARE(paper->keyword, u"Custom.41x41.5mm"_s);
        QCOMPARE(paper->name, u"Wrap label 41 × 41.5 mm"_s);
    }

    void labelsAcrossPrintInRows()
    {
        // Three labels, two across with a 2 mm gap: two rows, the second
        // with an empty place.
        PaperSize paper = PaperSize::custom(u"Row"_s, QSizeF(2 * 20 + 2, 10));
        paper.roll.across = 2;
        paper.roll.gapMm = 2;
        const qreal dpi = 254; // 10 dots per millimetre
        QList<QImage> labels;
        for (int i = 0; i < 3; ++i) {
            QImage label(200, 100, QImage::Format_Mono);
            label.fill(0); // black: the whole label
            labels << label;
        }
        const QList<QImage> rows = LabelPrinter::rowsAcross(labels, paper, dpi);
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).size(), QSize(420, 100));
        const auto black = [](const QImage &image, int x) {
            return image.pixelColor(x, 50).lightness() < 128;
        };
        QVERIFY(black(rows.at(0), 0) && black(rows.at(0), 199));
        QVERIFY(!black(rows.at(0), 200) && !black(rows.at(0), 219)); // the gap
        QVERIFY(black(rows.at(0), 220) && black(rows.at(0), 419));
        QVERIFY(black(rows.at(1), 100) && !black(rows.at(1), 300));
    }

    void rollLayoutsPrintAtTheirOwnSize()
    {
        if (LabelPrinter::filterProgram(u"rastertolabel"_s).isEmpty() || LabelPrinter::driverFilter(u"drv:///sample.drv/zebra.ppd"_s) != u"rastertolabel"_s) {
            QSKIP("CUPS's label drivers (rastertolabel) are not installed");
        }
        LabelPrinter printer(u"unused"_s, u"drv:///sample.drv/zebra.ppd"_s);
        QVERIFY2(printer.load(), qPrintable(printer.errorString()));

        // Two 40 x 14.4 mm labels across, 2 mm apart: an 82 mm wide page.
        LabelDocument across = withBlock(document(PaperOrientation::Portrait, QSizeF(40 * 1440 / 25.4, 14.4 * 1440 / 25.4)), QRectF(72, 72, 400, 200));
        across.paperName = u"ADC 40 x 14.4 mm (2 across)"_s;
        across.roll.across = 2;
        across.roll.gapMm = 2;
        const auto paper = printer.paperFor(across);
        QVERIFY(paper);
        QCOMPARE(paper->keyword, u"Custom.82x14.4mm"_s);
        LabelPrinter::Options options;
        options.copies = 3;
        options.compensateTopMargin = false;
        const QList<QImage> pages = printer.pages({across}, options);
        QCOMPARE(pages.size(), 2); // three labels in two rows
        QCOMPARE(pages.at(0).width(), qRound(82 * 203 / 25.4));

        // A continuous roll: the driver is told there are no gaps to find.
        LabelDocument continuous = withBlock(document(PaperOrientation::Portrait, QSizeF(55 * 1440 / 25.4, 30 * 1440 / 25.4)), QRectF(72, 72, 400, 200));
        continuous.paperName = u"Industrial polyester 55 mm, continuous"_s;
        continuous.roll.continuous = true;
        const auto roll = printer.paperFor(continuous);
        QVERIFY(roll);
        QCOMPARE(roll->keyword, u"Custom.55x30mm"_s);
        QVERIFY(roll->roll.continuous);
        QTemporaryFile raster;
        QVERIFY(raster.open());
        QVERIFY(LabelPrinter::writeRaster(raster.handle(), LabelPrinter::paperImage(continuous, *roll, 203), 1, *roll, 203));
        raster.close();
        QTemporaryFile output;
        QVERIFY(output.open());
        output.close();
        QString error;
        QVERIFY2(LabelPrinter::convertWithDriver(raster.fileName(), u"drv:///sample.drv/zebra.ppd"_s, *roll, {}, output.fileName(), &error), qPrintable(error));
        QFile data(output.fileName());
        QVERIFY(data.open(QIODevice::ReadOnly));
        const QByteArray zpl = data.readAll();
        QVERIFY2(zpl.contains("^MNN"), zpl.left(200).constData()); // continuous media
        QVERIFY(!zpl.contains("^MNY"));
    }

    void dymoDriversPrintOnlyTheirOwnSizes()
    {
        if (LabelPrinter::driverFilter(u"lw450t.ppd"_s).isEmpty()) {
            QSKIP("The DYMO LabelWriter 450 driver is not installed");
        }
        // DYMO's PPDs accept custom sizes, but a LabelWriter label that is not
        // DYMO's (an Altec wrap label, say) must not print on it silently.
        LabelPrinter printer(u"unused"_s, u"lw450t.ppd"_s);
        QVERIFY2(printer.load(), qPrintable(printer.errorString()));
        QVERIFY(!printer.supportsCustomSizes());
        QCOMPARE(printer.resolution(), 300.0);
        LabelDocument wrap = document(PaperOrientation::Landscape, QSizeF(41 * 1440 / 25.4, 41.5 * 1440 / 25.4));
        wrap.paperName = u"Wrap label 41 × 41.5 mm"_s;
        QVERIFY(!printer.paperFor(wrap));
        // Its own labels still match.
        LabelDocument small = document(PaperOrientation::Portrait, QSizeF(3240, 1800));
        small.paperName = u"30334 2-1/4 in x 1-1/4 in"_s;
        QVERIFY(printer.paperFor(small));
    }

    void printOffsetsMoveWhatIsOnScreen_data()
    {
        QTest::addColumn<int>("orientation");
        QTest::addRow("portrait") << int(PaperOrientation::Portrait);
        QTest::addRow("landscape") << int(PaperOrientation::Landscape);
    }

    void printOffsetsMoveWhatIsOnScreen()
    {
        // A dot on the label, printed with an offset meant as "3 mm right,
        // 2 mm up" on screen, must land 3 mm right and 2 mm up of where it
        // lands without, in the label's own (screen) directions.
        QFETCH(int, orientation);
        const auto paperOrientation = PaperOrientation(orientation);
        const PaperSize paper = PaperSize::custom(u"41 x 41.5"_s, QSizeF(41, 41.5));
        LabelDocument label = withBlock(document(paperOrientation, QSizeF(41 * 1440 / 25.4, 41.5 * 1440 / 25.4)), QRectF(1000, 800, 60, 60));
        const qreal dpi = 600;
        const QPointF shiftMm(3, -2);
        QCOMPARE(LabelPrinter::canvasShift(paperOrientation, LabelPrinter::paperShift(paperOrientation, shiftMm)), shiftMm);

        const auto dot = [&](const QPointF &paperOffsetMm) {
            const QImage page = LabelPrinter::paperImage(label, paper, dpi, nullptr, 0, QDateTime(), paperOffsetMm * 72.0 / 25.4);
            QPointF sum;
            int count = 0;
            for (int y = 0; y < page.height(); ++y) {
                for (int x = 0; x < page.width(); ++x) {
                    if (page.pixel(x, y) == qRgb(0, 0, 0)) {
                        sum += QPointF(x, y);
                        ++count;
                    }
                }
            }
            return count ? sum / count : QPointF();
        };
        // The same block moved on the canvas by the shift, printed without
        // an offset, is where the offset must put it.
        LabelDocument moved = label;
        objectBase(moved.objects[0]).bounds.translate(shiftMm * 1440 / 25.4);
        const auto movedDot = [&]() {
            const LabelDocument original = label;
            label = moved;
            const QPointF result = dot(QPointF());
            label = original;
            return result;
        };
        const QPointF expected = movedDot();
        const QPointF actual = dot(LabelPrinter::paperShift(paperOrientation, shiftMm));
        QVERIFY2((actual - expected).manhattanLength() < 2, // dots, at 600 dpi
                 qPrintable(u"offset put it at %1,%2, the moved block is at %3,%4"_s.arg(actual.x()).arg(actual.y()).arg(expected.x()).arg(expected.y())));
    }

    void printAreasLandWhereTheyPrint_data()
    {
        QTest::addColumn<int>("orientation");
        QTest::addRow("portrait") << int(PaperOrientation::Portrait);
        QTest::addRow("landscape") << int(PaperOrientation::Landscape);
    }

    void printAreasLandWhereTheyPrint()
    {
        // The wrap label's silver strip: 12 mm on the printer's left, along
        // the first 38 mm. Filled where the canvas shows it, the printed
        // image must be black exactly there.
        QFETCH(int, orientation);
        const auto paperOrientation = PaperOrientation(orientation);
        constexpr qreal TwipsPerMm = 1440 / 25.4;
        const QSizeF paperTwips(41 * TwipsPerMm, 41.5 * TwipsPerMm);
        const QRectF stripTwips(0, 0.5 * TwipsPerMm, 12 * TwipsPerMm, 38 * TwipsPerMm);
        QCOMPARE(LabelPrinter::paperRect(paperOrientation, paperTwips, LabelPrinter::canvasRect(paperOrientation, paperTwips, stripTwips)), stripTwips);

        const LabelDocument label = withBlock(document(paperOrientation, paperTwips), LabelPrinter::canvasRect(paperOrientation, paperTwips, stripTwips));
        const PaperSize paper = PaperSize::custom(u"wrap"_s, QSizeF(41, 41.5));
        const qreal dpi = 300;
        const QImage page = LabelPrinter::paperImage(label, paper, dpi);
        QRect black;
        for (int y = 0; y < page.height(); ++y) {
            for (int x = 0; x < page.width(); ++x) {
                if (page.pixel(x, y) == qRgb(0, 0, 0)) {
                    black |= QRect(x, y, 1, 1);
                }
            }
        }
        const qreal dotsPerTwip = dpi / 1440;
        const QRectF expected(stripTwips.topLeft() * dotsPerTwip, stripTwips.size() * dotsPerTwip);
        QVERIFY2(qAbs(black.left() - expected.left()) <= 2 && qAbs(black.right() + 1 - expected.right()) <= 2 && qAbs(black.top() - expected.top()) <= 2
                     && qAbs(black.bottom() + 1 - expected.bottom()) <= 2,
                 qPrintable(u"printed at %1,%2 %3x%4 dots, expected %5,%6 %7x%8"_s.arg(black.x())
                                .arg(black.y())
                                .arg(black.width())
                                .arg(black.height())
                                .arg(expected.x())
                                .arg(expected.y())
                                .arg(expected.width())
                                .arg(expected.height())));
    }

    void labelManagersPrintTape()
    {
        if (LabelPrinter::driverFilter(u"lm450.ppd"_s).isEmpty()) {
            QSKIP("The DYMO LabelManager 450 driver is not installed");
        }
        LabelPrinter printer(u"unused"_s, u"lm450.ppd"_s);
        QVERIFY2(printer.load(), qPrintable(printer.errorString()));
        QCOMPARE(printer.resolution(), 180.0);

        // One label type per tape width, from the driver's Continuous sizes.
        const QList<PaperSize> tapes = LabelPrinter::tapeTypes(printer.paperSizes());
        QList<int> widths;
        for (const PaperSize &tape : tapes) {
            widths << tape.tape;
        }
        QCOMPARE(widths, (QList<int>{6, 9, 12, 19, 24}));
        QCOMPARE(tapes.at(2).keyword, u"w35h4000"_s);
        QCOMPARE(tapes.at(2).name, u"12 mm tape"_s);

        // A 12 mm tape label 50 mm long: that tape, that long, no margins.
        LabelDocument label = TapeLayout::newLabel(12);
        label.tape->lengthMode = CellLengthMode::Fixed;
        label.tape->length = 50 * 1440 / 25.4;
        TapeLayout::layout(label);
        const auto paper = printer.paperFor(label);
        QVERIFY(paper);
        QCOMPARE(paper->keyword, u"w35h4000"_s);
        QCOMPARE(paper->tape, 12);
        QVERIFY(qAbs(paper->size.height() - 50 * 72 / 25.4) < 0.01);
        QCOMPARE(paper->imageableArea.top(), 0.0);
        // Not a LabelWriter label on it.
        QVERIFY(!printer.paperFor(document(PaperOrientation::Portrait, QSizeF(3240, 1800))));

        // The raster tells the driver the tape: code 2 for 12 mm.
        QTemporaryFile raster;
        QVERIFY(raster.open());
        const QImage page = LabelPrinter::paperImage(label, *paper, printer.resolution());
        QVERIFY(LabelPrinter::writeRaster(raster.handle(), page, 1, *paper, printer.resolution()));
        raster.close();
        QFile file(raster.fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        cups_raster_t *reader = cupsRasterOpen(file.handle(), CUPS_RASTER_READ);
        cups_page_header2_t header;
        QVERIFY(cupsRasterReadHeader2(reader, &header));
        cupsRasterClose(reader);
        QCOMPARE(header.cupsMediaType, 2u);
        QCOMPARE(header.HWResolution[0], 180u);
        QCOMPARE(int(header.cupsHeight), qRound(50 / 25.4 * 180));

        // DYMO's own LabelManager filter takes it.
        QTemporaryFile output;
        QVERIFY(output.open());
        output.close();
        QString error;
        QVERIFY2(LabelPrinter::convertWithDriver(raster.fileName(), u"lm450.ppd"_s, *paper, {}, output.fileName(), &error), qPrintable(error));
        QFile data(output.fileName());
        QVERIFY(data.open(QIODevice::ReadOnly));
        QVERIFY(data.size() > 0);
        // LabelManager commands: ESC C and ESC D set the tape up, then one
        // SYN per print line (the 450's driver source).
        const QByteArray bytes = data.readAll();
        QVERIFY(bytes.contains("\x1b" "C"));
        QVERIFY(bytes.contains("\x1b" "D"));
        QVERIFY(bytes.count('\x16') >= qRound(50 / 25.4 * 180) / 2);
    }

    void the550IsNotConvertedHere()
    {
        if (LabelPrinter::driverFilter(u"lw550t.ppd"_s).isEmpty()) {
            QSKIP("The DYMO LabelWriter 550 driver is not installed");
        }
        const LabelDocument label = withBlock(document(PaperOrientation::Portrait, QSizeF(3240, 1800)), QRectF(144, 144, 480, 240));
        QTemporaryFile raster;
        QVERIFY(raster.open());
        QVERIFY(LabelPrinter::writeRaster(raster.handle(), LabelPrinter::paperImage(label, Small, Dpi), 1, Small, Dpi));
        raster.close();

        // Refused at once with the reason, instead of the driver waiting
        // 30 seconds for a printer status that never comes.
        QElapsedTimer timer;
        timer.start();
        QString error;
        QVERIFY(!LabelPrinter::convertWithDriver(raster.fileName(), u"lw550t.ppd"_s, Small, {}, u"/dev/null"_s, &error));
        QVERIFY(timer.elapsed() < 5000);
        QVERIFY2(error.contains(u"550"_s), qPrintable(error));
    }
};

QTEST_MAIN(LabelPrinterTest)

#include "labelprintertest.moc"
