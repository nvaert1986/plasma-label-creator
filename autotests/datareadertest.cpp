/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "datareader.h"

#include <QLocale>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

DataSource fileSource(const QString &name, const QString &sheet = {})
{
    DataSource source;
    source.kind = DataSource::Kind::File;
    source.fileName = QStringLiteral(LABELCREATOR_TEST_DATA "/") + name;
    source.sheet = sheet;
    return source;
}

} // namespace

class DataReaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Dates come out in the user's short format.
        QLocale::setDefault(QLocale(u"nl_NL"_s));
    }

    void readsCsv()
    {
        // Dutch Excel: semicolons, quotes around fields with line breaks and quotes.
        const QByteArray data = "Name;Location;Note\r\nSRV-HV01;\"Rack A\nU12\";\"says \"\"hi\"\"\"\r\nSRV-HV02;Rack B;\r\n\r\n";
        QString error;
        const auto table = DataReader::parseCsv(data, QChar(), true, &error);
        QVERIFY2(table, qPrintable(error));
        QCOMPARE(table->columns, (QStringList{u"Name"_s, u"Location"_s, u"Note"_s}));
        QCOMPARE(table->rows.size(), 2);
        QCOMPARE(table->rows.at(0), (QStringList{u"SRV-HV01"_s, u"Rack A\nU12"_s, u"says \"hi\""_s}));
        QCOMPARE(table->values(1).value(u"Location"_s), u"Rack B"_s);
    }

    void detectsDelimitersAndEncodings()
    {
        QCOMPARE(DataReader::detectDelimiter(u"a;b;c"_s), QChar(u';'));
        QCOMPARE(DataReader::detectDelimiter(u"a,b,c"_s), QChar(u','));
        QCOMPARE(DataReader::detectDelimiter(u"a\tb"_s), QChar(u'\t'));
        // Commas inside quotes do not count.
        QCOMPARE(DataReader::detectDelimiter(u"\"a,b,c\";d;e"_s), QChar(u';'));

        // Windows-1252, as Excel writes CSV on Windows: é is one byte.
        const auto latin = DataReader::parseCsv("Naam\ncaf\xe9\n", QChar(), true);
        QCOMPARE(latin->rows.at(0).at(0), u"café"_s);
        // UTF-8 with a byte order mark.
        const auto utf8 = DataReader::parseCsv("\xEF\xBB\xBFNaam\ncaf\xC3\xA9\n", QChar(), true);
        QCOMPARE(utf8->columns.at(0), u"Naam"_s);
        QCOMPARE(utf8->rows.at(0).at(0), u"café"_s);
    }

    void namesColumnsWithoutAHeaderRow()
    {
        const auto table = DataReader::parseCsv("1,2\n3,4,5\n", u',', false);
        QCOMPARE(table->columns, (QStringList{u"A"_s, u"B"_s, u"C"_s}));
        QCOMPARE(table->rows.at(0), (QStringList{u"1"_s, u"2"_s, QString()})); // padded
        QCOMPARE(DataReader::columnName(25), u"Z"_s);
        QCOMPARE(DataReader::columnName(26), u"AA"_s);
        QCOMPARE(DataReader::columnName(27), u"AB"_s);
        // Unnamed or repeated header cells get a letter too; braces are dropped.
        const auto named = DataReader::parseCsv("Name,,Name,{Rack}\na,b,c,d\n", u',', true);
        QCOMPARE(named->columns, (QStringList{u"Name"_s, u"B"_s, u"C"_s, u"Rack"_s}));
        QVERIFY(!DataReader::parseCsv("a,\"open\n", u',', true));
    }

    void readsExcel()
    {
        QString error;
        QCOMPARE(DataReader::sheets(QStringLiteral(LABELCREATOR_TEST_DATA "/assets.xlsx")), (QStringList{u"Servers"_s, u"Racks"_s}));
        const auto table = DataReader::read(fileSource(u"assets.xlsx"_s), {}, &error);
        QVERIFY2(table, qPrintable(error));
        QCOMPARE(table->columns, (QStringList{u"Name"_s, u"Rack"_s, u"Installed"_s, u"Units"_s}));
        // Row 3 is missing in the file: an empty row, kept in its place.
        QCOMPARE(table->rows.size(), 3);
        QCOMPARE(table->rows.at(0), (QStringList{u"SRV-HV01"_s, u"Rack A"_s, u"29-10-2026"_s, u"2"_s}));
        QCOMPARE(table->rows.at(1), (QStringList{QString(), QString(), QString(), QString()}));
        // A custom date format, a skipped cell and a boolean.
        QCOMPARE(table->rows.at(2), (QStringList{u"SRV-HV02"_s, QString(), u"29-10-2026"_s, u"TRUE"_s}));

        const auto racks = DataReader::read(fileSource(u"assets.xlsx"_s, u"Racks"_s), {}, &error);
        QCOMPARE(racks->columns, (QStringList{u"U12"_s}));
        QVERIFY(!DataReader::read(fileSource(u"assets.xlsx"_s, u"Nope"_s), {}, &error));
        QVERIFY(error.contains(u"Nope"_s));
    }

    void readsOpenDocument()
    {
        QString error;
        QCOMPARE(DataReader::sheets(QStringLiteral(LABELCREATOR_TEST_DATA "/assets.ods")), (QStringList{u"Servers"_s, u"Second"_s}));
        const auto table = DataReader::read(fileSource(u"assets.ods"_s), {}, &error);
        QVERIFY2(table, qPrintable(error));
        // The thousand repeated empty columns and million empty rows are left out.
        QCOMPARE(table->columns, (QStringList{u"Name"_s, u"Location"_s, u"Installed"_s}));
        QCOMPARE(table->rows.size(), 4);
        QCOMPARE(table->rows.at(0), (QStringList{u"SRV-HV01"_s, u"Rack  A\nU12"_s, u"29-10-2026"_s}));
        QCOMPARE(table->rows.at(3), (QStringList{QString(), QString(), u"3,5"_s}));
        QCOMPARE(DataReader::read(fileSource(u"assets.ods"_s, u"Second"_s), {}, &error)->columns, (QStringList{u"Other"_s}));
    }

    void readsADatabase()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"assets.sqlite"_s);
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"setup"_s);
            database.setDatabaseName(file);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(u"CREATE TABLE assets (name TEXT, rack TEXT, active INTEGER)"_s));
            QVERIFY(query.exec(u"INSERT INTO assets VALUES ('SRV-HV01', 'A', 1), ('SRV-OLD', 'B', 0), ('SRV-HV02', 'C', 1)"_s));
            database.close();
        }
        QSqlDatabase::removeDatabase(u"setup"_s);

        DataSource source;
        source.kind = DataSource::Kind::Database;
        source.driver = u"QSQLITE"_s;
        source.database = file;
        source.query = u"SELECT name AS Name, rack AS Rack FROM assets WHERE active = 1 -- only those in use\nORDER BY name"_s;
        QString error;
        const auto table = DataReader::read(source, {}, &error);
        QVERIFY2(table, qPrintable(error));
        QCOMPARE(table->columns, (QStringList{u"Name"_s, u"Rack"_s}));
        QCOMPARE(table->rows, (QList<QStringList>{{u"SRV-HV01"_s, u"A"_s}, {u"SRV-HV02"_s, u"C"_s}}));

        // Writing is refused before anything runs.
        source.query = u"DELETE FROM assets"_s;
        QVERIFY(!DataReader::read(source, {}, &error));
        source.query = u"SELECT 1; DROP TABLE assets"_s;
        QVERIFY(!DataReader::read(source, {}, &error));
        source.query = u"SELECT name FROM assets"_s;
        QCOMPARE(DataReader::read(source, {}, &error)->rows.size(), 3);
    }

    void onlyRunsQueriesThatRead()
    {
        QVERIFY(DataReader::isReadOnlyQuery(u"SELECT * FROM t"_s));
        QVERIFY(DataReader::isReadOnlyQuery(u"  select a from t where b = 'delete me; please';"_s));
        QVERIFY(DataReader::isReadOnlyQuery(u"WITH x AS (SELECT 1) SELECT * FROM x"_s));
        QVERIFY(DataReader::isReadOnlyQuery(u"/* report */ SELECT 1 -- DROP TABLE t"_s));
        QVERIFY(!DataReader::isReadOnlyQuery(u"UPDATE t SET a = 1"_s));
        QVERIFY(!DataReader::isReadOnlyQuery(u"SELECT * INTO copy FROM t"_s));
        QVERIFY(!DataReader::isReadOnlyQuery(u"SELECT 1; DELETE FROM t"_s));
        QVERIFY(!DataReader::isReadOnlyQuery(u"EXEC dbo.Cleanup"_s));
        QVERIFY(!DataReader::isReadOnlyQuery(u"WITH x AS (SELECT 1) DELETE FROM t"_s));
        QVERIFY(!DataReader::isReadOnlyQuery(QString()));
    }
};

QTEST_GUILESS_MAIN(DataReaderTest)

#include "datareadertest.moc"
