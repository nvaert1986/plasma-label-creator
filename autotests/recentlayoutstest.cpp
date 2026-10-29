/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "recentlayouts.h"

#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

LabelDocument label(const QString &text)
{
    LabelDocument document;
    document.paperName = u"30334 2-1/4 in x 1-1/4 in"_s;
    document.id = u"w162h90"_s;
    document.paperOutline.addRect(QRectF(0, 0, 3240, 1800));
    TextObject object;
    object.name = u"TEXT"_s;
    object.bounds = QRectF(144, 144, 2952, 1512);
    object.spans = {{text, LabelFont(), Qt::black}};
    document.objects = {object};
    return document;
}

QString textOf(const LabelDocument &document)
{
    return std::get<TextObject>(document.objects.at(0)).plainText();
}

} // namespace

class RecentLayoutsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsPrintedLabelsNewestFirst()
    {
        QTemporaryDir dir;
        RecentLayouts recent(dir.path());
        QVERIFY(recent.layouts().isEmpty());

        QVERIFY(recent.add(label(u"first"_s), QString()));
        QVERIFY(recent.add(label(u"second"_s), u"/labels/Shelf.label"_s));
        const QList<RecentLayout> layouts = recent.layouts();
        QCOMPARE(layouts.size(), 2);
        QCOMPARE(layouts.at(0).title, u"Shelf"_s);
        QCOMPARE(layouts.at(0).source, u"/labels/Shelf.label"_s);
        QCOMPARE(layouts.at(1).title, u"30334 2-1/4 in x 1-1/4 in"_s);

        const auto loaded = recent.load(layouts.at(1));
        QVERIFY(loaded);
        QCOMPARE(textOf(*loaded), u"first"_s);

        // A thumbnail to choose by, its longer side ThumbnailSize pixels.
        const QImage thumbnail(layouts.at(0).thumbnail);
        QVERIFY(!thumbnail.isNull());
        QCOMPARE(std::max(thumbnail.width(), thumbnail.height()), RecentLayouts::ThumbnailSize);
    }

    void printingAgainMovesToTheFront()
    {
        QTemporaryDir dir;
        RecentLayouts recent(dir.path());
        QVERIFY(recent.add(label(u"a"_s), QString()));
        QVERIFY(recent.add(label(u"b"_s), u"/labels/B.label"_s));
        QVERIFY(recent.add(label(u"c"_s), QString()));

        // The same label again, unsaved: no second copy.
        QVERIFY(recent.add(label(u"a"_s), QString()));
        QCOMPARE(recent.layouts().size(), 3);
        QCOMPARE(textOf(*recent.load(recent.layouts().at(0))), u"a"_s);

        // The same file, changed since: replaces its earlier copy.
        QVERIFY(recent.add(label(u"b, changed"_s), u"/labels/B.label"_s));
        const QList<RecentLayout> layouts = recent.layouts();
        QCOMPARE(layouts.size(), 3);
        QCOMPARE(layouts.at(0).title, u"B"_s);
        QCOMPARE(textOf(*recent.load(layouts.at(0))), u"b, changed"_s);
    }

    void keepsOnlyTheMostRecent()
    {
        QTemporaryDir dir;
        RecentLayouts recent(dir.path());
        for (int i = 0; i < RecentLayouts::MaxLayouts + 3; ++i) {
            QVERIFY(recent.add(label(QString::number(i)), QString()));
        }
        const QList<RecentLayout> layouts = recent.layouts();
        QCOMPARE(layouts.size(), RecentLayouts::MaxLayouts);
        QCOMPARE(textOf(*recent.load(layouts.constFirst())), QString::number(RecentLayouts::MaxLayouts + 2));
        // The dropped ones' files are gone too: a label and a thumbnail each, plus the index.
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 2 * RecentLayouts::MaxLayouts + 1);
    }

    void removesAndClears()
    {
        QTemporaryDir dir;
        RecentLayouts recent(dir.path());
        QVERIFY(recent.add(label(u"a"_s), QString()));
        QVERIFY(recent.add(label(u"b"_s), QString()));
        const RecentLayout first = recent.layouts().at(0);
        recent.remove(first);
        QCOMPARE(recent.layouts().size(), 1);
        QVERIFY(!QFile::exists(first.fileName));
        QVERIFY(!QFile::exists(first.thumbnail));
        recent.clear();
        QVERIFY(recent.layouts().isEmpty());
    }
};

QTEST_MAIN(RecentLayoutsTest)

#include "recentlayoutstest.moc"
