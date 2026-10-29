/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "units.h"

#include <QTest>

using namespace LabelCreator;

class UnitsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void inchRoundTrip()
    {
        QCOMPARE(Units::mmToTwips(25.4), 1440.0);
        QCOMPARE(Units::twipsToMm(1440.0), 25.4);
    }

    void points()
    {
        QCOMPARE(Units::twipsToPoints(20.0), 1.0);
        QCOMPARE(Units::pointsToTwips(12.0), 240.0);
    }

    void dots()
    {
        // One inch at the LabelWriter 450's 300 dpi head.
        QCOMPARE(Units::twipsToDots(1440.0, 300.0), 300.0);
    }
};

QTEST_GUILESS_MAIN(UnitsTest)

#include "unitstest.moc"
