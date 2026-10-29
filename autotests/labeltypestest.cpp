/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "customlabels.h"
#include "labelfit.h"
#include "objectfactory.h"

#include <QStandardPaths>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr qreal TwipsPerMm = 1440 / 25.4;

// DYMO's 30334, as its driver lists it.
const PaperSize Dymo30334{u"30334_2-1_4_in_x_1-1_4_in"_s, u"30334 2-1/4 in x 1-1/4 in"_s, QSizeF(162, 90), QRectF(0, 0, 162, 90), QString(), QRectF(), 0};
const PaperSize Dymo30252{u"w79h252"_s, u"30252 Address"_s, QSizeF(78.96, 252), QRectF(0, 0, 78.96, 252), QString(), QRectF(), 0};

LabelDocument labelOn(const PaperSize &paper)
{
    return ObjectFactory::newLabel(paper.name, paper.keyword, paper.size);
}

} // namespace

class LabelTypesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Never the user's own label sizes.
        QStandardPaths::setTestModeEnabled(true);
    }

    void aDymoLabelOnADymo()
    {
        QCOMPARE(LabelFit::check(labelOn(Dymo30334), {Dymo30334, Dymo30252}, false, std::nullopt), LabelFit::Problem::None);
    }

    void anAltecSizeOnADymo()
    {
        // The wrap label, a custom size: a DYMO takes none, so it is asked about.
        const PaperSize wrap = PaperSize::custom(u"Wrap label 41 × 41.5 mm"_s, QSizeF(41, 41.5));
        QCOMPARE(LabelFit::check(labelOn(wrap), {Dymo30334, Dymo30252}, false, std::nullopt), LabelFit::Problem::NotOffered);
    }

    void aCustomSizeOnAnAltec()
    {
        // Not in the Altec's list, but it takes custom sizes; only the length counts.
        const PaperSize wrap = PaperSize::custom(u"Wrap label 41 × 41.5 mm"_s, QSizeF(41, 41.5));
        const QList<PaperSize> altec = {PaperSize::custom(u"Labels 50 × 25 mm"_s, QSizeF(50, 25))};
        QCOMPARE(LabelFit::check(labelOn(wrap), altec, true, 41.5), LabelFit::Problem::None);
        QCOMPARE(LabelFit::check(labelOn(wrap), altec, true, 25.0), LabelFit::Problem::WrongLength);
    }

    void aDymoLabelOnAnAltec()
    {
        // The first test print: a 30334 on the Altec with 41.5 mm labels loaded.
        const QList<PaperSize> altec = {PaperSize::custom(u"Labels 52 × 40 mm"_s, QSizeF(52, 40))};
        QCOMPARE(LabelFit::check(labelOn(Dymo30334), altec, true, 41.5), LabelFit::Problem::NotOffered);
    }

    void startsOnTheRightLabel()
    {
        const PaperSize fits = PaperSize::custom(u"Fits 52 × 40 mm"_s, QSizeF(52, 40));
        const PaperSize other = PaperSize::custom(u"Other 50 × 25 mm"_s, QSizeF(50, 25));
        // The one used last, if offered.
        QCOMPARE(LabelFit::startingPaper({other, fits}, other.name, 41.5, Dymo30252).name, other.name);
        // Else one that fits the loaded labels.
        QCOMPARE(LabelFit::startingPaper({other, fits}, Dymo30334.name, 41.5, Dymo30252).name, fits.name);
        // Nothing known about the labels: the fallback if offered, else the first.
        QCOMPARE(LabelFit::startingPaper({Dymo30334, Dymo30252}, u"gone"_s, std::nullopt, Dymo30252).name, Dymo30252.name);
        QCOMPARE(LabelFit::startingPaper({other, fits}, u"gone"_s, std::nullopt, Dymo30252).name, other.name);
        // No printer: the fallback.
        QCOMPARE(LabelFit::startingPaper({}, u"gone"_s, std::nullopt, Dymo30252).name, Dymo30252.name);
        // A LabelManager: 12 mm tape, unless another was used last.
        PaperSize tape6 = PaperSize::custom(u"6 mm tape"_s, QSizeF(6, 1411));
        tape6.tape = 6;
        PaperSize tape12 = PaperSize::custom(u"12 mm tape"_s, QSizeF(12, 1411));
        tape12.tape = 12;
        QCOMPARE(LabelFit::startingPaper({tape6, tape12}, u"gone"_s, std::nullopt, Dymo30252).name, tape12.name);
        QCOMPARE(LabelFit::startingPaper({tape6, tape12}, tape6.name, std::nullopt, Dymo30252).name, tape6.name);
    }

    void keepsCustomSizes()
    {
        QVERIFY(CustomLabels::all().isEmpty());
        CustomLabels::add(PaperSize::custom(u"Wrap label 41 × 41.5 mm"_s, QSizeF(41, 41.5)));
        CustomLabels::add(PaperSize::custom(u"Small 20 × 10 mm"_s, QSizeF(20, 10)));
        QList<PaperSize> sizes = CustomLabels::all();
        QCOMPARE(sizes.size(), 2);
        const auto wrap = std::find_if(sizes.cbegin(), sizes.cend(), [](const PaperSize &paper) {
            return paper.name.startsWith(u"Wrap"_s);
        });
        QVERIFY(wrap != sizes.cend());
        QCOMPARE(wrap->keyword, u"Custom.41x41.5mm"_s);
        QVERIFY(!wrap->group.isEmpty());

        // Saved again under the same name: replaced, not added.
        CustomLabels::add(PaperSize::custom(u"Small 20 × 10 mm"_s, QSizeF(22, 10)));
        QCOMPARE(CustomLabels::all().size(), 2);

        CustomLabels::remove(u"Small 20 × 10 mm"_s);
        sizes = CustomLabels::all();
        QCOMPARE(sizes.size(), 1);
        QCOMPARE(sizes.constFirst().name, u"Wrap label 41 × 41.5 mm"_s);
        CustomLabels::remove(u"Wrap label 41 × 41.5 mm"_s);
        QVERIFY(CustomLabels::all().isEmpty());
    }

    void keepsThePrintableArea()
    {
        // The wrap label's silver strip, in points like the size.
        PaperSize wrap = PaperSize::custom(u"Wrap"_s, QSizeF(41, 41.5));
        constexpr qreal PointsPerMm = 72 / 25.4;
        wrap.printArea = QRectF(0, 0.5 * PointsPerMm, 12 * PointsPerMm, 38 * PointsPerMm);
        CustomLabels::add(wrap);
        const auto found = CustomLabels::find(u"Wrap"_s);
        QVERIFY(found);
        QVERIFY(qAbs(found->printArea.width() - wrap.printArea.width()) < 0.01);
        QVERIFY(qAbs(found->printArea.y() - wrap.printArea.y()) < 0.01);
        QVERIFY(qAbs(found->printArea.height() - wrap.printArea.height()) < 0.01);

        // Saved again without one: the whole label prints again.
        wrap.printArea = QRectF();
        CustomLabels::add(wrap);
        QVERIFY(CustomLabels::find(u"Wrap"_s)->printArea.isEmpty());
        CustomLabels::remove(u"Wrap"_s);
        QVERIFY(!CustomLabels::find(u"Wrap"_s));
    }
};

QTEST_MAIN(LabelTypesTest)

#include "labeltypestest.moc"
