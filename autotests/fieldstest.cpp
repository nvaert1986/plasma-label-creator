/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fields.h"
#include "labelformat.h"

#include <QBuffer>
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
