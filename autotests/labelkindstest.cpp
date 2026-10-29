/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelkinds.h"
#include "tapelayout.h"
#include "units.h"

#include <QFile>
#include <QTest>

#include <cmath>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QString textOf(const LabelDocument &document, int index)
{
    return std::get<TextObject>(document.objects.at(index)).plainText();
}

QString cellText(const LabelDocument &document, const TapeCell &cell)
{
    return cell.object >= 0 ? textOf(document, cell.object) : QString();
}

// A die-cut label 89 x 28 mm, landscape, as DYMO's 30252.
LabelDocument dieCut()
{
    LabelDocument document;
    document.orientation = PaperOrientation::Landscape;
    document.paperName = u"30252 Address"_s;
    document.paperOutline.addRect(QRectF(0, 0, Units::mmToTwips(28), Units::mmToTwips(89)));
    return document;
}

} // namespace

class LabelKindsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void patchPanelsOnTape()
    {
        LabelKinds::Ports ports;
        ports.count = 24;
        ports.pitchMm = 16.5;
        ports.leadMm = 5;
        LabelDocument label = LabelKinds::ports(TapeLayout::newLabel(9), ports);
        TapeLayout::layout(label);
        QCOMPARE(label.tape->lengthMode, CellLengthMode::Fixed);
        QVERIFY(qAbs(Units::twipsToMm(label.tape->length) - (5 + 24 * 16.5)) < 0.01);
        QCOMPARE(label.tape->root.subcells.size(), 25); // the lead, then the ports
        QCOMPARE(label.tape->root.subcells.at(0).object, -1);
        const TapeCell &first = label.tape->root.subcells.at(1);
        QCOMPARE(cellText(label, first), u"1"_s);
        QVERIFY(first.borderWidth > 0);
        QVERIFY(qAbs(Units::twipsToMm(first.bounds.width()) - 16.5) < 0.01);
        QCOMPARE(cellText(label, label.tape->root.subcells.constLast()), u"24"_s);
    }

    void portNumbering()
    {
        LabelKinds::Ports ports;
        ports.count = 6;
        ports.first = 1;
        ports.step = 2;
        ports.prefix = u"P"_s;
        ports.digits = 2;
        QCOMPARE(LabelKinds::portText(ports, 0), u"P01"_s);
        QCOMPARE(LabelKinds::portText(ports, 5), u"P11"_s);
        ports.reversed = true;
        QCOMPARE(LabelKinds::portText(ports, 0), u"P11"_s);
        ports.arrow = LabelKinds::Arrow::Down;
        QCOMPARE(LabelKinds::portText(ports, 5), u"▼ P01"_s);
        ports.texts = {QString(), u"Uplink"_s};
        QCOMPARE(LabelKinds::portText(ports, 1), u"Uplink"_s);
    }

    void portsOnADieCutLabel()
    {
        // 89 mm long: five 16.5 mm ports fit, the sixth is cut off.
        LabelKinds::Ports ports;
        ports.count = 6;
        ports.pitchMm = 16.5;
        const LabelDocument label = LabelKinds::ports(dieCut(), ports);
        QVERIFY(!label.tape);
        int texts = 0;
        for (const LabelObject &object : label.objects) {
            const QRectF bounds = objectBase(object).bounds;
            QVERIFY(bounds.right() <= label.canvasSize().width() + 1);
            texts += std::holds_alternative<TextObject>(object) ? 1 : 0;
        }
        QCOMPARE(texts, 6); // the last one only partly
    }

    void rackMarkers()
    {
        LabelKinds::RackMarkers rack;
        rack.firstUnit = 1;
        rack.lastUnit = 8;
        rack.descending = true;
        LabelDocument label = LabelKinds::rackMarkers(TapeLayout::newLabel(12), rack);
        TapeLayout::layout(label);
        QVERIFY(qAbs(Units::twipsToMm(label.tape->length) - 8 * 44.45) < 0.01);
        QCOMPARE(label.tape->root.subcells.size(), 8);
        QCOMPARE(cellText(label, label.tape->root.subcells.at(0)), u"U8"_s);
        QCOMPARE(cellText(label, label.tape->root.subcells.at(7)), u"U1"_s);
        QVERIFY(label.tape->root.subcells.at(0).borderWidth > 0);
    }

    void cableFlags()
    {
        LabelKinds::CableFlag flag;
        flag.text = u"SW1-P12"_s;
        flag.diameterMm = 6;
        flag.flagLengthMm = 30;
        LabelDocument label = LabelKinds::cableFlag(TapeLayout::newLabel(12), flag);
        TapeLayout::layout(label);
        QVERIFY(qAbs(Units::twipsToMm(label.tape->length) - (60 + M_PI * 6)) < 0.01);
        const QList<TapeCell> &cells = label.tape->root.subcells;
        QCOMPARE(cells.size(), 3);
        QCOMPARE(cellText(label, cells.at(0)), u"SW1-P12"_s);
        QCOMPARE(cells.at(1).object, -1); // around the cable
        QVERIFY(qAbs(Units::twipsToMm(cells.at(1).bounds.width()) - M_PI * 6) < 0.01);
        QCOMPARE(cellText(label, cells.at(2)), u"SW1-P12"_s);
    }

    void cableWraps()
    {
        LabelKinds::CableWrap wrap;
        wrap.text = u"PSU-A"_s;
        wrap.diameterMm = 8;
        wrap.font.size = 8;
        LabelDocument label = LabelKinds::cableWrap(TapeLayout::newLabel(12), wrap);
        TapeLayout::layout(label);
        // Once around the cable, the text repeated and turned to read along it.
        QVERIFY(qAbs(Units::twipsToMm(label.tape->length) - M_PI * 8) < 0.01);
        QVERIFY(label.tape->root.subcells.size() >= 3);
        for (const LabelObject &object : label.objects) {
            QCOMPARE(objectBase(object).rotation, Rotation::Rotation90);
        }
    }

    void readsRhinoTemplates()
    {
        QFile file(QStringLiteral(LABELCREATOR_TEST_DATA "/patchpanel.rhino"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString error;
        const auto rhino = LabelKinds::readRhinoTemplate(file.readAll(), &error);
        QVERIFY2(rhino, qPrintable(error));
        QCOMPARE(rhino->kind, u"PatchPanelLabel"_s);
        QCOMPARE(rhino->tapeMillimetres, 9);
        QVERIFY(rhino->font.bold);
        QCOMPARE(rhino->font.size, 10.0);
        QVERIFY(rhino->ports);
        QCOMPARE(rhino->ports->count, 4);
        QVERIFY(qAbs(rhino->ports->pitchMm - 16.51) < 0.01); // 936 twips
        QVERIFY(rhino->ports->separators);
        QCOMPARE(rhino->labels.size(), 2);
        QCOMPARE(rhino->ports->texts, (QStringList{u"A-01"_s, u"A-02"_s}));
        // Cells without a text are numbered; but a label made from a
        // template's texts leaves the ones it has empty.
        QCOMPARE(LabelKinds::portText(*rhino->ports, 3), u"4"_s);
        LabelDocument fromTemplate = LabelKinds::fromRhinoTemplate(*rhino, 1); // "B-01" only
        QCOMPARE(fromTemplate.tape->root.subcells.size(), 4);
        QCOMPARE(textOf(fromTemplate, 0), u"B-01"_s);
        QCOMPARE(textOf(fromTemplate, 1), QString());

        QVERIFY(!LabelKinds::readRhinoTemplate("<html/>", &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(LabelKindsTest)

#include "labelkindstest.moc"
