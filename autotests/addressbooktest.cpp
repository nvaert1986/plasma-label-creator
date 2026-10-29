/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "addressbook.h"

#include <QFile>
#include <QLocale>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

class AddressBookTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Sorting follows the user's language; pin one for the test.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void loadsDlsAddressBooks()
    {
        QString error;
        const auto book = AddressBook::load(QStringLiteral(LABELCREATOR_TEST_DATA "/addresses.ABX"), &error);
        QVERIFY2(book, qPrintable(error));
        QCOMPARE(book->entries.size(), 3);
        // Sorted by "file as", as DLS lists them.
        QCOMPARE(book->entries.at(0).fileAs, u"Ångström Labs"_s);
        QCOMPARE(book->entries.at(1).fileAs, u"Jansen, Jan"_s);
        QCOMPARE(book->entries.at(1).address, u"Jan Jansen\nDorpsstraat 1\n1234 AB  Ergens"_s);
        QCOMPARE(book->entries.at(2).lastName, u"de Vries"_s);
    }

    void saveRoundTrip()
    {
        const auto book = AddressBook::load(QStringLiteral(LABELCREATOR_TEST_DATA "/addresses.ABX"));
        QVERIFY(book);
        QTemporaryDir dir;
        const QString fileName = dir.filePath(u"out.ABX"_s);
        QString error;
        QVERIFY2(book->save(fileName, &error), qPrintable(error));

        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(3) == QByteArray("\xEF\xBB\xBF"));

        const auto reread = AddressBook::load(fileName);
        QVERIFY(reread);
        QCOMPARE(reread->entries, book->entries);
    }

    void filesAddressesLikeDls()
    {
        const AddressEntry entry = AddressBook::fromAddress(u"Gábor Candrás\nPST Kft\nHunyadi János u.5"_s);
        QCOMPARE(entry.firstName, u"Gábor"_s);
        QCOMPARE(entry.lastName, u"Candrás"_s);
        QCOMPARE(entry.fileAs, u"Candrás, Gábor"_s);
        QVERIFY(entry.fileAsSuggestions().contains(u"PST Kft"_s));
    }

    void csvWithHeader()
    {
        const QString csv = u"Voornaam;Achternaam;Bedrijf;Straat;Postcode;Plaats\n"
                            u"Jan;Jansen;Acme BV;Dorpsstraat 1;1234 AB;Ergens\n"
                            u"\"Piet\";\"Klaas; de Groot\";;\"Lange\nstraat 2\";5678 CD;Elders\n"_s;
        const QList<AddressEntry> entries = AddressBook::parseCsv(csv);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).address, u"Jan Jansen\nAcme BV\nDorpsstraat 1\n1234 AB  Ergens"_s);
        QCOMPARE(entries.at(0).fileAs, u"Jansen, Jan"_s);
        QCOMPARE(entries.at(1).lastName, u"Klaas; de Groot"_s);
        QCOMPARE(entries.at(1).address, u"Piet Klaas; de Groot\nLange\nstraat 2\n5678 CD  Elders"_s);
    }

    void csvWithoutHeader()
    {
        const QList<AddressEntry> entries = AddressBook::parseCsv(u"Alice de Vries,Kerkstraat 9,,5678 CD Elders\r\n"_s);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).address, u"Alice de Vries\nKerkstraat 9\n5678 CD Elders"_s);
        QCOMPARE(entries.at(0).fileAs, u"Vries, Alice de"_s);
    }
};

QTEST_GUILESS_MAIN(AddressBookTest)

#include "addressbooktest.moc"
