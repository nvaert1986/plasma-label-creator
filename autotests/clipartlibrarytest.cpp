/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "clipartlibrary.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

class ClipartLibraryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsDotNetResources()
    {
        QFile file(QStringLiteral(LABELCREATOR_TEST_DATA "/fake-dls.dll"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QMap<QString, QByteArray> entries = DotNetResources::read(file.readAll());
        QCOMPARE(entries.size(), 3);
        QVERIFY(entries.value(u"resources/clipart/clipart.xml"_s).contains("<Clipart>"));
        QVERIFY(!QImage::fromData(entries.value(u"resources/clipart/square.png"_s)).isNull());
    }

    void importsFromTheDlsProgram()
    {
        QTemporaryDir dir;
        ClipartLibrary library(dir.path());
        QVERIFY(library.load());
        QString error;
        QCOMPARE(library.importDlsProgram(QStringLiteral(LABELCREATOR_TEST_DATA "/fake-dls.dll"), &error), 1);

        ClipartLibrary reloaded(dir.path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.items().size(), 1);
        const ClipartItem item = reloaded.items().constFirst();
        QCOMPARE(item.categoryId, u"Shapes"_s);
        QVERIFY(item.builtIn);
        QVERIFY(!QImage(reloaded.path(item)).isNull());

        bool found = false;
        for (const ClipartCategory &category : reloaded.categories()) {
            if (category.id == u"Shapes"_s) {
                QCOMPARE(category.names.value(u"nl"_s), u"Vormen"_s);
                found = true;
            }
        }
        QVERIFY(found);

        // Importing again replaces rather than duplicates.
        QCOMPARE(reloaded.importDlsProgram(QStringLiteral(LABELCREATOR_TEST_DATA "/fake-dls.dll")), 1);
        QCOMPARE(reloaded.items().size(), 1);
    }

    void rejectsOtherPrograms()
    {
        QTemporaryDir dir;
        ClipartLibrary library(dir.path());
        QVERIFY(library.load());
        QString error;
        QCOMPARE(library.importDlsProgram(QStringLiteral(LABELCREATOR_TEST_DATA "/addresses.ABX"), &error), 0);
        QVERIFY(!error.isEmpty());
    }

    void exportAndImportBetweenLibraries()
    {
        QTemporaryDir source;
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::black);
        const QString imageFile = source.filePath(u"logo.png"_s);
        QVERIFY(image.save(imageFile));

        ClipartLibrary first(source.filePath(u"library"_s));
        QVERIFY(first.load());
        QCOMPARE(first.importImages({imageFile}), 1);
        QCOMPARE(first.items().constFirst().categoryId, ClipartLibrary::UserCategory);

        QTemporaryDir exported;
        QString error;
        QVERIFY2(first.exportTo(exported.path(), {}, &error), qPrintable(error));
        QVERIFY(QFile::exists(exported.filePath(u"Clipart.xml"_s)));
        QVERIFY(QFile::exists(exported.filePath(u"logo.png"_s)));

        QTemporaryDir target;
        ClipartLibrary second(target.path());
        QVERIFY(second.load());
        QCOMPARE(second.importDlsCatalog(exported.filePath(u"Clipart.xml"_s), &error), 1);
        QCOMPARE(second.items().size(), 1);
        QVERIFY(!QImage(second.path(second.items().constFirst())).isNull());

        QVERIFY(second.remove(second.items().constFirst()));
        QVERIFY(second.items().isEmpty());
    }

    void dlsProgramIfAvailable()
    {
        // The user's own copy of DYMO.DLS.dll, if it was put in the
        // reference folder; it never goes into the repository.
        const QString dll = QStringLiteral(LABELCREATOR_TEST_DATA "/../../reference/dls/DYMO.DLS.dll");
        if (!QFile::exists(dll)) {
            QSKIP("reference/dls/DYMO.DLS.dll is not there");
        }
        QTemporaryDir dir;
        ClipartLibrary library(dir.path());
        QVERIFY(library.load());
        QString error;
        const int imported = library.importDlsProgram(dll, &error);
        QVERIFY2(imported > 100, qPrintable(error));
        qInfo("imported %d images in %lld categories", imported, qlonglong(library.categories().size()));

        // An extracted library can be imported again from its Clipart.xml.
        QTemporaryDir copy;
        ClipartLibrary second(copy.path());
        QVERIFY(second.load());
        QCOMPARE(second.importDlsCatalog(dir.filePath(u"Clipart.xml"_s), &error), imported);
    }
};

QTEST_GUILESS_MAIN(ClipartLibraryTest)

#include "clipartlibrarytest.moc"
