/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "arrange.h"
#include "labelformat.h"

#include <QBuffer>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// A label with a box at each of @p rects.
LabelDocument labelWith(const QList<QRectF> &rects)
{
    LabelDocument document;
    document.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
    for (const QRectF &rect : rects) {
        ShapeObject shape;
        shape.name = u"Shape"_s + QString::number(document.objects.size());
        shape.bounds = rect;
        document.objects << shape;
    }
    return document;
}

QRectF boundsOf(const LabelDocument &document, int index)
{
    return objectBase(document.objects.at(index)).bounds;
}

} // namespace

class ArrangeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void alignsToTheBoxAroundThem()
    {
        LabelDocument document = labelWith({{100, 100, 200, 100}, {500, 300, 400, 200}});
        QVERIFY(Arrange::align(document, {0, 1}, Qt::AlignLeft));
        QCOMPARE(boundsOf(document, 0).left(), 100.0);
        QCOMPARE(boundsOf(document, 1).left(), 100.0);
        QCOMPARE(boundsOf(document, 1).top(), 300.0); // only horizontally

        QVERIFY(Arrange::align(document, {0, 1}, Qt::AlignRight | Qt::AlignBottom));
        QCOMPARE(boundsOf(document, 0).right(), 500.0);
        QCOMPARE(boundsOf(document, 0).bottom(), 500.0);
        QCOMPARE(boundsOf(document, 1).right(), 500.0);

        QVERIFY(Arrange::align(document, {0, 1}, Qt::AlignHCenter));
        QCOMPARE(boundsOf(document, 0).center().x(), boundsOf(document, 1).center().x());
        // Aligned already: nothing moves.
        QVERIFY(!Arrange::align(document, {0, 1}, Qt::AlignHCenter));
    }

    void lockedObjectsStayAndOthersLineUp()
    {
        LabelDocument document = labelWith({{100, 100, 200, 100}, {500, 300, 400, 200}});
        Arrange::setLocked(document, {1}, true);
        QVERIFY(!Arrange::canMove(document, {1}));
        QVERIFY(Arrange::canMove(document, {0, 1}));
        QVERIFY(Arrange::align(document, {0, 1}, Qt::AlignBottom));
        QCOMPARE(boundsOf(document, 1), QRectF(500, 300, 400, 200));
        QCOMPARE(boundsOf(document, 0).bottom(), 500.0);
    }

    void groupsMoveAsOne()
    {
        LabelDocument document = labelWith({{100, 100, 100, 100}, {300, 100, 100, 100}, {1000, 600, 100, 100}});
        const int id = Arrange::group(document, {0, 1});
        QVERIFY(id >= 0);
        QVERIFY(Arrange::isGrouped(document, 0));
        QVERIFY(!Arrange::isGrouped(document, 2));
        QCOMPARE(Arrange::withGroups(document, {1}), (QList<int>{0, 1}));
        // DLS writes -1 for no group.
        objectBase(document.objects[2]).groupId = -1;
        QCOMPARE(Arrange::withGroups(document, {2}), (QList<int>{2}));

        // The group's right edge lines up, the gap inside it stays.
        QVERIFY(Arrange::align(document, {1, 2}, Qt::AlignRight));
        QCOMPARE(boundsOf(document, 1).right(), 1100.0);
        QCOMPARE(boundsOf(document, 0).right(), 900.0);
        QCOMPARE(boundsOf(document, 2).right(), 1100.0);

        // A second group gets its own id; grouping a grouped object takes its group along.
        LabelDocument more = labelWith({{0, 0, 10, 10}, {20, 0, 10, 10}, {40, 0, 10, 10}, {60, 0, 10, 10}});
        const int first = Arrange::group(more, {0, 1});
        const int second = Arrange::group(more, {2, 3});
        QVERIFY(first != second);
        Arrange::group(more, {1, 2});
        QCOMPARE(Arrange::withGroups(more, {0}), (QList<int>{0, 1, 2, 3}));
        Arrange::ungroup(more, {3});
        for (int i = 0; i < 4; ++i) {
            QVERIFY(!Arrange::isGrouped(more, i));
        }
    }

    void distributesWithEqualGaps()
    {
        // Gaps of 100 and 500 become 300 each; first and last stay.
        LabelDocument document = labelWith({{0, 0, 100, 50}, {200, 0, 200, 50}, {900, 0, 100, 50}});
        QVERIFY(!Arrange::distribute(document, {0, 2}, Qt::Horizontal));
        QVERIFY(Arrange::distribute(document, {2, 0, 1}, Qt::Horizontal));
        QCOMPARE(boundsOf(document, 0).left(), 0.0);
        QCOMPARE(boundsOf(document, 1).left(), 400.0);
        QCOMPARE(boundsOf(document, 2).left(), 900.0);

        document = labelWith({{0, 0, 50, 100}, {0, 150, 50, 100}, {0, 1000, 50, 100}});
        QVERIFY(Arrange::distribute(document, {0, 1, 2}, Qt::Vertical));
        QCOMPARE(boundsOf(document, 1).top(), 500.0);
    }

    void lockAndWrapAreSaved()
    {
        LabelDocument document = labelWith({{100, 100, 200, 100}, {500, 300, 400, 200}});
        TextObject text;
        text.name = u"Text"_s;
        text.spans = {{u"A long line that wraps"_s, LabelFont(), Qt::black}};
        text.bounds = QRectF(100, 600, 1000, 400);
        text.wrap = true;
        document.objects << text;
        Arrange::setLocked(document, {1}, true);
        Arrange::group(document, {0, 1});

        const QByteArray written = LabelWriter::toByteArray(document);
        QVERIFY(written.contains("\"locked\":true"));
        QBuffer buffer;
        buffer.setData(written);
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        const auto read = reader.read(&buffer);
        QVERIFY2(read, qPrintable(reader.errorString()));
        QVERIFY(!objectBase(read->objects.at(0)).locked);
        QVERIFY(objectBase(read->objects.at(1)).locked);
        QVERIFY(std::get<TextObject>(read->objects.at(2)).wrap);
        QCOMPARE(Arrange::withGroups(*read, {0}), (QList<int>{0, 1}));

        // Without them, no comment.
        QVERIFY(!LabelWriter::toByteArray(labelWith({{0, 0, 10, 10}})).contains("<!--"));
    }
};

QTEST_GUILESS_MAIN(ArrangeTest)

#include "arrangetest.moc"
