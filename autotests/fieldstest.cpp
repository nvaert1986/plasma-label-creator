/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fields.h"
#include "labelformat.h"

#include <QBuffer>
#include <QLocale>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

LabelDocument labelWith(const QString &text)
{
    LabelDocument document;
    document.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
    TextObject object;
    object.name = u"Text"_s;
    object.spans = {{text, LabelFont(), Qt::black}};
    object.bounds = QRectF(100, 100, 4000, 800);
    document.objects << object;
    return document;
}

QString textOf(const LabelDocument &document, int index = 0)
{
    return std::get<TextObject>(document.objects.at(index)).plainText();
}

} // namespace

class FieldsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fillsFields()
    {
        const QHash<QString, QString> values = {{u"Name"_s, u"SRV-HV01"_s}, {u"Rack"_s, u"A"_s}};
        QCOMPARE(Fields::fill(u"{Name} in rack {Rack}"_s, values), u"SRV-HV01 in rack A"_s);
        // Case does not matter when nothing matches exactly.
        QCOMPARE(Fields::fill(u"{name}"_s, values), u"SRV-HV01"_s);
        // An unknown field stays, so the mistake shows.
        QCOMPARE(Fields::fill(u"{Nmae}"_s, values), u"{Nmae}"_s);
        // Doubled braces are braces; unclosed ones are text.
        QCOMPARE(Fields::fill(u"{{Name}} {Name"_s, values), u"{Name} {Name"_s);
        QCOMPARE(Fields::fill(u"{ Rack }"_s, values), u"A"_s);
    }

    void listsTheFieldsUsed()
    {
        LabelDocument document = labelWith(u"{Name}, {Rack} and {Name}"_s);
        BarcodeObject barcode;
        barcode.text = u"{Serial}"_s;
        document.objects << barcode;
        ImageObject image;
        image.imageLocation = u"/photos/{Photo}.png"_s;
        document.objects << image;
        CounterObject counter;
        counter.preText = u"{Batch}-"_s;
        document.objects << counter;
        QCOMPARE(Fields::used(document), (QStringList{u"Name"_s, u"Rack"_s, u"Serial"_s, u"Photo"_s, u"Batch"_s}));

        const LabelDocument filled = Fields::fill(document, {{u"Name"_s, u"N"_s}, {u"Rack"_s, u"R"_s}, {u"Serial"_s, u"42"_s}, {u"Photo"_s, u"p1"_s}, {u"Batch"_s, u"B7"_s}});
        QCOMPARE(textOf(filled), u"N, R and N"_s);
        QCOMPARE(std::get<BarcodeObject>(filled.objects.at(1)).text, u"42"_s);
        QCOMPARE(*std::get<ImageObject>(filled.objects.at(2)).imageLocation, u"/photos/p1.png"_s);
        QCOMPARE(std::get<CounterObject>(filled.objects.at(3)).preText, u"B7-"_s);
        // The original keeps its fields.
        QCOMPARE(textOf(document), u"{Name}, {Rack} and {Name}"_s);
    }

    void addressesGetAFontPerFilledLine()
    {
        LabelDocument document;
        AddressObject address;
        address.spans = {{u"{Address}"_s, LabelFont(), Qt::black}};
        address.fitLineFonts();
        document.objects << address;
        const LabelDocument filled = Fields::fill(document, {{u"Address"_s, u"Jan\nStraat 1\n1234 AB Ergens"_s}});
        QCOMPARE(std::get<AddressObject>(filled.objects.at(0)).lineFonts.size(), 3);
    }

    void worksThingsOut_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");
        // Printed on 30 October 2026 at 14:05.
        QTest::newRow("date") << u"{Date}"_s << u"30-10-2026"_s;
        QTest::newRow("best before") << u"{Date+30}"_s << u"29-11-2026"_s;
        QTest::newRow("weeks back") << u"{Date - 2w}"_s << u"16-10-2026"_s;
        QTest::newRow("months") << u"{Date+3m|date:yyMMdd}"_s << u"270130"_s;
        QTest::newRow("a date field") << u"{Installed+1y}"_s << u"29-10-2027"_s;
        QTest::newRow("its own format") << u"{Installed|date:d MMMM yyyy}"_s << u"29 oktober 2026"_s;
        QTest::newRow("time") << u"{Time}"_s << u"14:05"_s;
        QTest::newRow("hours later") << u"{Time+2h}"_s << u"16:05"_s;
        QTest::newRow("a time field in hours") << u"{Start+1h}"_s << u"09:30"_s;
        QTest::newRow("now and later") << u"{Now+1h}"_s << u"30-10-2026 15:05"_s;
        QTest::newRow("number") << u"{Serial+1}"_s << u"0043"_s;
        QTest::newRow("pad") << u"{Count|pad:6}"_s << u"000007"_s;
        QTest::newRow("pad with") << u"{Count|pad:4:*}"_s << u"***7"_s;
        QTest::newRow("parts") << u"{Name|left:3}-{Name|right:2}-{Name|mid:5:2}"_s << u"SRV-01-HV"_s;
        QTest::newRow("case") << u"{Name|lower|replace:srv:host}"_s << u"host-hv01"_s;
        QTest::newRow("default") << u"{Empty|default:n/a}"_s << u"n/a"_s;
        QTest::newRow("a column called A-1") << u"{A-1}"_s << u"exact"_s;
        QTest::newRow("unknown filter stays") << u"{Name|shout}"_s << u"{Name|shout}"_s;
        QTest::newRow("not a date stays") << u"{Name+1m}"_s << u"{Name+1m}"_s;
        QTest::newRow("unknown stays") << u"{Missing+1}"_s << u"{Missing+1}"_s;
    }

    void worksThingsOut()
    {
        QFETCH(QString, text);
        QFETCH(QString, expected);
        const QLocale before;
        QLocale::setDefault(QLocale(u"nl_NL"_s));
        const QHash<QString, QString> values = {{u"Installed"_s, u"29-10-2026"_s},
                                                {u"Serial"_s, u"0042"_s},
                                                {u"Count"_s, u"7"_s},
                                                {u"Name"_s, u"SRV-HV01"_s},
                                                {u"Empty"_s, QString()},
                                                {u"A-1"_s, u"exact"_s},
                                                {u"Start"_s, u"08:30"_s}};
        const QString result = Fields::fill(text, values, QDateTime(QDate(2026, 10, 30), QTime(14, 5)));
        QLocale::setDefault(before);
        QCOMPARE(result, expected);
    }

    void twoDigitYearsAreThisCentury()
    {
        // en_US writes short dates 10/30/26: 2026, not 1926.
        const QLocale before;
        QLocale::setDefault(QLocale(u"en_US"_s));
        const QString result = Fields::fill(u"{Installed+1y|date:yyyy-MM-dd}"_s, {{u"Installed"_s, u"10/30/26"_s}});
        QLocale::setDefault(before);
        QCOMPARE(result, u"2027-10-30"_s);
    }

    void calculatedFieldsBuildOnEachOther()
    {
        LabelDocument document = labelWith(u"{Code} until {BestBefore}"_s);
        document.calculated = {{u"Code"_s, u"{Prefix}-{Serial|pad:6}"_s}, {u"BestBefore"_s, u"{Date+30|date:yyyy-MM-dd}"_s}, {u"Short"_s, u"{Code|right:6}"_s}};
        const QDateTime now(QDate(2026, 10, 30), QTime(9, 0));
        const LabelDocument filled = Fields::fill(document, {{u"Prefix"_s, u"AB"_s}, {u"Serial"_s, u"123"_s}}, now);
        QCOMPARE(textOf(filled), u"AB-000123 until 2026-11-29"_s);
        QCOMPARE(Fields::withCalculated(document, {{u"Prefix"_s, u"AB"_s}, {u"Serial"_s, u"123"_s}}, now).value(u"Short"_s), u"000123"_s);
        QCOMPARE(Fields::used(u"{Serial+1|pad:6} {Date - 1w}"_s), (QStringList{u"Serial"_s, u"Date"_s}));

        // Kept in the file.
        QBuffer buffer;
        buffer.setData(LabelWriter::toByteArray(document));
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        QCOMPARE(reader.read(&buffer)->calculated, document.calculated);
    }

    void checksAnswers()
    {
        PrintQuestion question;
        question.name = u"Batch"_s;
        question.required = true;
        question.minimumLength = 4;
        question.maximumLength = 6;
        question.allowed = PrintQuestion::Allowed::Digits;
        QVERIFY(Fields::problem(question, u""_s));
        QVERIFY(Fields::problem(question, u"123"_s));
        QVERIFY(Fields::problem(question, u"1234567"_s));
        QVERIFY(Fields::problem(question, u"12a4"_s));
        QVERIFY(!Fields::problem(question, u"1234"_s));

        question.allowed = PrintQuestion::Allowed::LettersAndDigits;
        QVERIFY(!Fields::problem(question, u"AB12"_s));
        QVERIFY(Fields::problem(question, u"AB-12"_s));

        question.allowed = PrintQuestion::Allowed::Letters;
        QVERIFY(Fields::problem(question, u"AB12"_s));

        // A pattern must match the whole answer.
        question.allowed = PrintQuestion::Allowed::Pattern;
        question.pattern = u"[A-Z]{2}-\\d{2}"_s;
        QVERIFY(!Fields::problem(question, u"AB-12"_s));
        QVERIFY(Fields::problem(question, u"xAB-12"_s));
        question.pattern = u"(unclosed"_s;
        QVERIFY(Fields::problem(question, u"AB-12"_s));

        // Optional and empty is fine, whatever the other rules.
        question.required = false;
        QVERIFY(!Fields::problem(question, u""_s));
    }

    void keepsQuestionsAndDataSourceInTheFile()
    {
        LabelDocument document = labelWith(u"{Batch} {Name}"_s);
        PrintQuestion question;
        question.name = u"Batch"_s;
        question.prompt = u"Batch number"_s;
        question.defaultValue = u"0001"_s;
        question.minimumLength = 4;
        question.allowed = PrintQuestion::Allowed::Pattern;
        question.pattern = u"\\d{4}--[A-Z]"_s; // "--" may not appear in an XML comment
        document.questions << question;
        document.dataSource.kind = DataSource::Kind::Database;
        document.dataSource.driver = u"QODBC"_s;
        document.dataSource.database = u"Driver={ODBC Driver 18 for SQL Server};Server=sql01"_s;
        document.dataSource.user = u"labels"_s;
        document.dataSource.query = u"SELECT Name FROM Assets -- only active\nWHERE Active = 1"_s;

        const QByteArray written = LabelWriter::toByteArray(document);
        QVERIFY(written.contains("<!-- plasma-label-creator {"));
        QCOMPARE(written.count("--"), 2); // only the comment's own
        QVERIFY(!written.contains("password"));

        QBuffer buffer;
        buffer.setData(written);
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        const auto read = reader.read(&buffer);
        QVERIFY2(read, qPrintable(reader.errorString()));
        QCOMPARE(read->questions, document.questions);
        QCOMPARE(read->dataSource, document.dataSource);

        // A label without them has no comment.
        QVERIFY(!LabelWriter::toByteArray(labelWith(u"x"_s)).contains("<!--"));

        // A file source too, with a detected delimiter.
        document.questions.clear();
        document.dataSource = DataSource();
        document.dataSource.kind = DataSource::Kind::File;
        document.dataSource.fileName = u"/data/assets.xlsx"_s;
        document.dataSource.sheet = u"Servers"_s;
        buffer.close();
        buffer.setData(LabelWriter::toByteArray(document));
        buffer.open(QIODevice::ReadOnly);
        QCOMPARE(reader.read(&buffer)->dataSource, document.dataSource);
    }
};

QTEST_GUILESS_MAIN(FieldsTest)

#include "fieldstest.moc"
