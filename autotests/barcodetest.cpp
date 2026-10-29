/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "barcode.h"

#include <QImage>
#include <QPainter>
#include <QTest>

#include <zint.h>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QImage render(const Barcode &barcode, const QSize &size)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    barcode.paint(&painter, QRectF(QPointF(0, 0), size));
    return image;
}

int countPixels(const QImage &image, QRgb colour)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if ((line[x] & 0xffffff) == (colour & 0xffffff)) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

class BarcodeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void encodesValidData_data()
    {
        QTest::addColumn<Barcode::Symbology>("symbology");
        QTest::addColumn<QString>("data");

        QTest::newRow("code128") << Barcode::Symbology::Code128 << u"LABELCREATOR-123"_s;
        QTest::newRow("code39") << Barcode::Symbology::Code39 << u"ABC123"_s;
        QTest::newRow("codabar") << Barcode::Symbology::Codabar << u"A40156B"_s;
        QTest::newRow("ean8") << Barcode::Symbology::Ean8 << u"9638507"_s;
        QTest::newRow("ean13") << Barcode::Symbology::Ean13 << u"5901234123457"_s;
        QTest::newRow("upca") << Barcode::Symbology::UpcA << u"036000291452"_s;
        QTest::newRow("upce") << Barcode::Symbology::UpcE << u"0123456"_s;
        QTest::newRow("itf14") << Barcode::Symbology::Itf14 << u"1540014128876"_s;
        QTest::newRow("i2of5") << Barcode::Symbology::Interleaved2of5 << u"12345678"_s;
        QTest::newRow("qr") << Barcode::Symbology::QrCode << u"https://example.org/ünïcode"_s;
        QTest::newRow("datamatrix") << Barcode::Symbology::DataMatrix << u"LABELCREATOR"_s;
        QTest::newRow("pdf417") << Barcode::Symbology::Pdf417 << u"LABELCREATOR"_s;
        // Beyond DLS's set.
        QTest::newRow("aztec") << Barcode::Symbology::Aztec << u"LABELCREATOR"_s;
        QTest::newRow("maxicode") << Barcode::Symbology::MaxiCode << u"LABELCREATOR"_s;
        QTest::newRow("microqr") << Barcode::Symbology::MicroQr << u"12345"_s;
        QTest::newRow("code93") << Barcode::Symbology::Code93 << u"LABEL-93"_s;
        QTest::newRow("code11") << Barcode::Symbology::Code11 << u"123-45"_s;
        QTest::newRow("msi") << Barcode::Symbology::MsiPlessey << u"123456"_s;
        QTest::newRow("pharmacode") << Barcode::Symbology::Pharmacode << u"1234"_s;
        QTest::newRow("gs1-128") << Barcode::Symbology::Gs1_128 << u"[01]09501101530003[17]251231[10]ABC123"_s;
        QTest::newRow("gs1 datamatrix") << Barcode::Symbology::Gs1DataMatrix << u"[01]09501101530003[21]SERIAL1"_s;
        QTest::newRow("gs1 databar") << Barcode::Symbology::Gs1DataBarOmni << u"0950110153000"_s;
        QTest::newRow("gs1 databar expanded") << Barcode::Symbology::Gs1DataBarExpanded << u"[01]09501101530003[3103]000123"_s;
        QTest::newRow("hibc128") << Barcode::Symbology::Hibc128 << u"A123BJC5D6E71"_s;
        QTest::newRow("hibc39") << Barcode::Symbology::Hibc39 << u"A123BJC5D6E71"_s;
        QTest::newRow("hibc datamatrix") << Barcode::Symbology::HibcDataMatrix << u"A123BJC5D6E71"_s;
        QTest::newRow("hibc qr") << Barcode::Symbology::HibcQrCode << u"A123BJC5D6E71"_s;
        QTest::newRow("postnet") << Barcode::Symbology::Postnet << u"12345678901"_s;
        QTest::newRow("planet") << Barcode::Symbology::Planet << u"12345678901"_s;
        QTest::newRow("intelligent mail") << Barcode::Symbology::UspsIntelligentMail << u"01234567094987654321-01234567891"_s;
        QTest::newRow("kix") << Barcode::Symbology::Kix << u"2500GG30250"_s;
        QTest::newRow("royal mail") << Barcode::Symbology::RoyalMail4State << u"LE28HS9Z"_s;
    }

    void encodesValidData()
    {
        QFETCH(Barcode::Symbology, symbology);
        QFETCH(QString, data);

        Barcode barcode(symbology, data);
        QVERIFY2(barcode.encode(), qPrintable(barcode.errorString()));
        QVERIFY(barcode.isValid());
        QVERIFY(!barcode.naturalSize().isEmpty());
    }

    void code128CodeSets()
    {
        const auto modules = [](Barcode::Symbology symbology, const QString &data) {
            Barcode barcode(symbology, data);
            barcode.setShowText(false);
            return barcode.encode() ? barcode.naturalSize().width() : -1.0;
        };

        // Digits pack two to a symbol in set C, which is what Auto picks.
        const qreal automatic = modules(Barcode::Symbology::Code128, u"123456"_s);
        QVERIFY(automatic > 0);
        QCOMPARE(modules(Barcode::Symbology::Code128C, u"123456"_s), automatic);
        QVERIFY(modules(Barcode::Symbology::Code128A, u"123456"_s) > automatic);
        QVERIFY(modules(Barcode::Symbology::Code128B, u"123456"_s) > automatic);
        // FNC1 adds one symbol of 11 modules.
        QCOMPARE(modules(Barcode::Symbology::Code128Fnc1, u"123456"_s), automatic + 11);
    }

    void code128EscapesBackslashes()
    {
        // A backslash in the data must not start a zint escape sequence:
        // each of these encodes as many characters as it has.
        const QStringList data = {u"A\\^1B"_s, u"A\\B"_s, u"\\\\^C"_s, u"A\\"_s};
        for (const QString &text : data) {
            Barcode escaped(Barcode::Symbology::Code128B, text);
            QVERIFY2(escaped.encode(), qPrintable(escaped.errorString()));
            Barcode reference(Barcode::Symbology::Code128B, QString(text.size(), u'X'));
            QVERIFY(reference.encode());
            QVERIFY2(escaped.naturalSize() == reference.naturalSize(), qPrintable(text));
        }
    }

    void code39MatchesZint_data()
    {
        QTest::addColumn<bool>("checkDigit");
        QTest::newRow("plain") << false;
        QTest::newRow("check digit") << true;
    }

    // Plasma Label Creator encodes Code 39 itself; for everything zint accepts, the
    // result must be the same symbol.
    void code39MatchesZint()
    {
        QFETCH(bool, checkDigit);
        const QByteArray data = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%";

        zint_symbol *symbol = ZBarcode_Create();
        symbol->symbology = BARCODE_CODE39;
        symbol->option_2 = checkDigit ? 1 : 0;
        QCOMPARE(ZBarcode_Encode(symbol, reinterpret_cast<const unsigned char *>(data.constData()), int(data.size())), 0);
        QString expected;
        for (int x = 0; x < symbol->width; ++x) {
            expected += (symbol->encoded_data[0][x >> 3] >> (x & 7)) & 1 ? u'1' : u'0';
        }
        ZBarcode_Delete(symbol);

        Barcode barcode(checkDigit ? Barcode::Symbology::Code39WithCheckDigit : Barcode::Symbology::Code39, QString::fromLatin1(data));
        QVERIFY2(barcode.encode(), qPrintable(barcode.errorString()));
        QCOMPARE(int(barcode.naturalSize().width()), int(expected.size()));
        // One pixel per module at a 2:1 ratio, as zint lays it out.
        QImage image(int(barcode.naturalSize().width()), 1, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        barcode.paint(&painter, QRectF(0, 0, image.width(), 1), {1, 2}, Qt::AlignLeft);
        painter.end();
        QString actual;
        for (int x = 0; x < image.width(); ++x) {
            actual += (image.pixel(x, 0) & 0xffffff) == 0 ? u'1' : u'0';
        }
        QCOMPARE(actual, expected);
    }

    void code39EncodesAsterisks()
    {
        // As DLS does: "**123" is five data characters between start and
        // stop, 7 characters of 5 bars.
        Barcode barcode(Barcode::Symbology::Code39, u"**123"_s);
        QVERIFY2(barcode.encode(), qPrintable(barcode.errorString()));
        QImage image(int(barcode.naturalSize().width()), 1, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        barcode.paint(&painter, QRectF(0, 0, image.width(), 1), {1, 2}, Qt::AlignLeft);
        painter.end();
        int bars = 0;
        for (int x = 0; x < image.width(); ++x) {
            bars += (image.pixel(x, 0) & 0xffffff) == 0 && (x == 0 || (image.pixel(x - 1, 0) & 0xffffff) != 0);
        }
        QCOMPARE(bars, 35);
    }

    void code39RejectsLowercase()
    {
        Barcode barcode(Barcode::Symbology::Code39, u"abc"_s);
        QVERIFY(!barcode.encode());
        QVERIFY(barcode.errorString().contains(u"Lowercase"_s));
    }

    void rejectsInvalidData()
    {
        Barcode barcode(Barcode::Symbology::Ean13, u"not digits"_s);
        QVERIFY(!barcode.encode());
        QVERIFY(!barcode.isValid());
        QVERIFY(!barcode.errorString().isEmpty());
    }

    void rejectsGs1DataWithoutApplicationIdentifiers()
    {
        // GS1 data names each field: "[01]..." rather than bare digits.
        Barcode barcode(Barcode::Symbology::Gs1_128, u"09501101530003"_s);
        QVERIFY(!barcode.encode());
        QVERIFY(!barcode.errorString().isEmpty());
    }

    void dataMatrixIsSquare()
    {
        // Data that fits a smaller rectangular symbol still gets a square one.
        for (const auto symbology : {Barcode::Symbology::DataMatrix, Barcode::Symbology::Gs1DataMatrix}) {
            Barcode barcode(symbology, symbology == Barcode::Symbology::DataMatrix ? u"ABCDEFGHIJKLMNOPQRS"_s : u"[01]09501101530003[21]SN42"_s);
            QVERIFY2(barcode.encode(), qPrintable(barcode.errorString()));
            QCOMPARE(barcode.naturalSize().width(), barcode.naturalSize().height());
        }
    }

    void newTwoDimensionalCodesKeepTheirShape()
    {
        for (const Barcode::Symbology symbology : {Barcode::Symbology::Aztec, Barcode::Symbology::MaxiCode, Barcode::Symbology::MicroQr}) {
            QVERIFY(Barcode::isTwoDimensional(symbology));
        }
        QVERIFY(Barcode::takesGs1Data(Barcode::Symbology::Gs1DataMatrix));
        QVERIFY(!Barcode::takesGs1Data(Barcode::Symbology::DataMatrix));
    }

    void changingTextVisibilityNeedsReencode()
    {
        Barcode barcode(Barcode::Symbology::Code128, u"ABC"_s);
        QVERIFY(barcode.encode());
        barcode.setShowText(false);
        QVERIFY(!barcode.isValid());
        QVERIFY(barcode.encode());
    }

    void hidingTextMakesSymbolShorter()
    {
        Barcode withText(Barcode::Symbology::Code128, u"ABC"_s);
        Barcode withoutText(Barcode::Symbology::Code128, u"ABC"_s);
        withoutText.setShowText(false);
        QVERIFY(withText.encode());
        QVERIFY(withoutText.encode());
        QVERIFY(withoutText.naturalSize().height() < withText.naturalSize().height());
    }

    void printRenderIsPureBlackAndWhite()
    {
        Barcode barcode(Barcode::Symbology::QrCode, u"LABELCREATOR"_s);
        QVERIFY(barcode.encode());

        const QImage image = render(barcode, QSize(300, 300));
        const int black = countPixels(image, qRgb(0, 0, 0));
        const int white = countPixels(image, qRgb(255, 255, 255));
        QVERIFY(black > 0);
        QVERIFY(white > 0);
        QCOMPARE(black + white, 300 * 300);
    }

    void twoDimensionalKeepsAspectRatio()
    {
        Barcode barcode(Barcode::Symbology::QrCode, u"LABELCREATOR"_s);
        QVERIFY(barcode.encode());

        // Wide target: the square symbol is centred, so the outer columns stay empty.
        const QImage image = render(barcode, QSize(600, 200));
        for (int y = 0; y < image.height(); ++y) {
            QCOMPARE(image.pixel(0, y) & 0xffffff, 0xffffffu);
            QCOMPARE(image.pixel(599, y) & 0xffffff, 0xffffffu);
        }
    }

    void oneDimensionalFillsTarget()
    {
        Barcode barcode(Barcode::Symbology::Code128, u"ABC"_s);
        barcode.setShowText(false);
        QVERIFY(barcode.encode());

        // Without quiet zones Code 128 starts and ends with a bar, so a
        // stretched symbol touches both edges.
        const QImage image = render(barcode, QSize(600, 100));
        QCOMPARE(image.pixel(0, 50) & 0xffffff, 0u);
        QCOMPARE(image.pixel(599, 50) & 0xffffff, 0u);
    }

    void moduleWidthKeepsBarsNarrow()
    {
        Barcode barcode(Barcode::Symbology::Code128, u"ABC"_s);
        barcode.setShowText(false);
        QVERIFY(barcode.encode());

        // Two pixels per module, left aligned: the symbol ends well before the edge.
        QImage image(600, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, false);
        barcode.paint(&painter, QRectF(0, 0, 600, 100), {2.0}, Qt::AlignLeft | Qt::AlignVCenter);
        painter.end();

        const int symbolWidth = qRound(barcode.naturalSize().width() * 2.0);
        QCOMPARE(image.pixel(0, 50) & 0xffffff, 0u);
        QCOMPARE(image.pixel(symbolWidth - 1, 50) & 0xffffff, 0u);
        for (int x = symbolWidth + 1; x < 600; ++x) {
            QCOMPARE(image.pixel(x, 50) & 0xffffff, 0xffffffu);
        }
    }

    void moduleWidthShrinksToFit()
    {
        Barcode barcode(Barcode::Symbology::Code128, u"A LONGER CODE 128 PAYLOAD"_s);
        barcode.setShowText(false);
        QVERIFY(barcode.encode());

        // Far too wide at 10 px per module; it must still end inside the target.
        const QImage image = render(barcode, QSize(400, 60));
        QImage narrow(400, 60, QImage::Format_RGB32);
        narrow.fill(Qt::white);
        QPainter painter(&narrow);
        barcode.paint(&painter, QRectF(0, 0, 400, 60), {10.0}, Qt::AlignLeft);
        painter.end();
        QCOMPARE(narrow, image);
    }
};

QTEST_MAIN(BarcodeTest)

#include "barcodetest.moc"
