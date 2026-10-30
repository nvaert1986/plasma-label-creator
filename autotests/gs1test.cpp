/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gs1.h"

#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

class Gs1Test : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsAndWritesBracketedData()
    {
        const QString data = u"[01]09501101530003[17]261231[10]{Batch}[21]A[90]x"_s;
        const auto elements = Gs1::parse(data);
        QVERIFY(elements);
        QCOMPARE(elements->size(), 5);
        QCOMPARE(elements->at(2), (Gs1::Element{u"10"_s, u"{Batch}"_s}));
        // "[B]" is not an AI: the whole text is not GS1 data written with brackets.
        QVERIFY(!Gs1::parse(u"[01]1[B]"_s));
        QVERIFY(!Gs1::parse(u"0950110153"_s));
        QCOMPARE(Gs1::parse(QString())->size(), 0);
        const QList<Gs1::Element> built = {{u"01"_s, u"09501101530003"_s}, {u"10"_s, u"AB-12"_s}};
        QCOMPARE(Gs1::compose(built), u"[01]09501101530003[10]AB-12"_s);
        QCOMPARE(*Gs1::parse(Gs1::compose(built)), built);
    }

    void fieldsInValuesStayTogether()
    {
        // A field's braces may hold anything, even a [.
        const auto elements = Gs1::parse(u"[10]{Batch|replace:[:}[21]{Serial}"_s);
        QVERIFY(elements);
        QCOMPARE(elements->size(), 2);
        QCOMPARE(elements->at(0).value, u"{Batch|replace:[:}"_s);
    }

    void checksValues_data()
    {
        QTest::addColumn<QString>("ai");
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("right");
        QTest::newRow("GTIN") << u"01"_s << u"09501101530003"_s << true;
        QTest::newRow("GTIN check digit") << u"01"_s << u"09501101530004"_s << false;
        QTest::newRow("GTIN too short") << u"01"_s << u"9501101530003"_s << false;
        QTest::newRow("SSCC") << u"00"_s << u"106141411234567897"_s << true;
        QTest::newRow("GLN") << u"414"_s << u"9501101530003"_s << true;
        QTest::newRow("GLN check digit") << u"414"_s << u"9501101530004"_s << false;
        QTest::newRow("GLN right") << u"414"_s << u"9501101530010"_s << true;
        QTest::newRow("date") << u"17"_s << u"261231"_s << true;
        QTest::newRow("end of month") << u"15"_s << u"270200"_s << true;
        QTest::newRow("no 31 November") << u"17"_s << u"261131"_s << false;
        QTest::newRow("batch") << u"10"_s << u"AB-12/3"_s << true;
        QTest::newRow("batch space") << u"10"_s << u"AB 12"_s << false;
        QTest::newRow("batch too long") << u"10"_s << u"123456789012345678901"_s << false;
        QTest::newRow("weight") << u"3103"_s << u"001250"_s << true;
        QTest::newRow("weight letters") << u"3103"_s << u"0012a0"_s << false;
        QTest::newRow("postal with country") << u"421"_s << u"5281234AB"_s << true;
        QTest::newRow("postal without country") << u"421"_s << u"NL1234AB"_s << false;
        QTest::newRow("expiry time") << u"7003"_s << u"2612312359"_s << true;
        QTest::newRow("expiry 25:00") << u"7003"_s << u"2612312500"_s << false;
        QTest::newRow("a field") << u"01"_s << u"{GTIN}"_s << true;
        QTest::newRow("unknown AI") << u"8017"_s << u"ABC"_s << true;
        QTest::newRow("not an AI") << u"1"_s << u"ABC"_s << false;
    }

    void checksValues()
    {
        QFETCH(QString, ai);
        QFETCH(QString, value);
        QFETCH(bool, right);
        const auto problem = Gs1::problem({ai, value});
        QVERIFY2(problem.has_value() != right, qPrintable(problem.value_or(u"no problem"_s)));
    }

    void computesCheckDigits()
    {
        QCOMPARE(Gs1::checkDigit(u"0950110153000"_s), u'3');
        QCOMPARE(Gs1::checkDigit(u"10614141123456789"_s), u'7');
        QCOMPARE(Gs1::identifier(u"01"_s)->format(), u"N14, with check digit"_s);
        QCOMPARE(Gs1::identifier(u"10"_s)->format(), u"X..20"_s);
        QCOMPARE(Gs1::identifier(u"17"_s)->format(), u"N6 (YYMMDD)"_s);
    }
};

QTEST_GUILESS_MAIN(Gs1Test)

#include "gs1test.moc"
