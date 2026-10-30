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
        QCOMPARE(Arrange::unitCount(document, {0, 1, 2}), 2);
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

    void reorderingKeepsTheOrderAmongThem()
    {
        LabelDocument document = labelWith({{0, 0, 10, 10}, {1, 0, 10, 10}, {2, 0, 10, 10}, {3, 0, 10, 10}, {4, 0, 10, 10}});
        const auto lefts = [&document]() {
            QList<int> result;
            for (int i = 0; i < document.objects.size(); ++i) {
                result << int(boundsOf(document, i).left());
            }
            return result;
        };
        // Given in any order, with a repeat: positions come back for each.
        QCOMPARE(Arrange::sendToBack(document, {4, 1, 4}), (QList<int>{1, 0, 1}));
        QCOMPARE(lefts(), (QList<int>{1, 4, 0, 2, 3}));
        QCOMPARE(Arrange::bringToFront(document, {0, 2}), (QList<int>{3, 4}));
        QCOMPARE(lefts(), (QList<int>{4, 2, 3, 1, 0}));
    }

    void layersShowPrintAndLock()
    {
        LabelDocument document = labelWith({{0, 0, 10, 10}, {20, 0, 10, 10}, {40, 0, 10, 10}, {60, 0, 10, 10}});
        document.layers = {{u"Photo"_s, true, false, false}, {u"Hidden"_s, false, true, false}, {u"Fixed"_s, true, true, true}};
        objectBase(document.objects[0]).layer = u"Photo"_s;
        objectBase(document.objects[1]).layer = u"Hidden"_s;
        objectBase(document.objects[2]).layer = u"Fixed"_s;
        objectBase(document.objects[3]).printed = false;
        QVERIFY(document.isShown(0) && !document.isPrinted(0) && document.isSelectable(0));
        QVERIFY(!document.isShown(1) && document.isPrinted(1) && !document.isSelectable(1));
        QVERIFY(document.isLocked(2) && !document.isSelectable(2) && document.isPrinted(2));
        QVERIFY(!document.isPrinted(3) && !document.isLocked(3));
        // A locked layer's object stays when lining up.
        QVERIFY(Arrange::align(document, {2, 3}, Qt::AlignLeft));
        QCOMPARE(boundsOf(document, 2).left(), 40.0);
        QCOMPARE(boundsOf(document, 3).left(), 40.0);

        // Renamed layers keep their objects; removed ones hand them to the label.
        document.changeLayers({{u"Product photo"_s, true, false, false}}, {{u"Photo"_s, u"Product photo"_s}, {u"Hidden"_s, QString()}, {u"Fixed"_s, QString()}});
        QCOMPARE(objectBase(document.objects.at(0)).layer, u"Product photo"_s);
        QVERIFY(objectBase(document.objects.at(1)).layer.isEmpty());
        QVERIFY(document.isShown(1) && document.isSelectable(2));

        // Saved and read back.
        QBuffer buffer;
        buffer.setData(LabelWriter::toByteArray(document));
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        const auto read = reader.read(&buffer);
        QVERIFY2(read, qPrintable(reader.errorString()));
        QCOMPARE(read->layers, document.layers);
        QCOMPARE(objectBase(read->objects.at(0)).layer, u"Product photo"_s);
        QVERIFY(!objectBase(read->objects.at(3)).printed);
        QVERIFY(objectBase(read->objects.at(2)).printed);
    }

    void objectsAtAnAngleLineUpByWhatShows()
    {
        // A 400 x 400 box turned 45 degrees reaches about 83 beyond its sides.
        LabelDocument document = labelWith({{1000, 0, 400, 400}, {0, 1000, 400, 400}});
        objectBase(document.objects[0]).angle = 45;
        const QRectF shown = objectOutline(objectBase(document.objects.at(0))).boundingRect();
        QVERIFY(qAbs(shown.width() - 400 * std::sqrt(2.0)) < 0.01);
        QVERIFY(Arrange::align(document, {0, 1}, Qt::AlignLeft));
        QVERIFY(qAbs(objectOutline(objectBase(document.objects.at(0))).boundingRect().left()) < 0.01);
        QCOMPARE(boundsOf(document, 1).left(), 0.0);

        // Kept in the file.
        QBuffer buffer;
        buffer.setData(LabelWriter::toByteArray(document));
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        const auto read = reader.read(&buffer);
        QVERIFY2(read, qPrintable(reader.errorString()));
        QCOMPARE(objectBase(read->objects.at(0)).angle, 45.0);
        QCOMPARE(objectBase(read->objects.at(1)).angle, 0.0);
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

        // On tape the file lists objects as the cells hold them, which need
        // not be their order on the label: the flags follow the objects.
        LabelDocument tape;
        tape.tape = TapeLabel();
        tape.tape->width = 680;
        for (const QString &name : {u"A"_s, u"B"_s, u"C"_s}) {
            TextObject t;
            t.name = name;
            t.spans = {{name, LabelFont(), Qt::black}};
            tape.addObject(t);
        }
        tape.moveCell(0, 1); // B, A, C along the tape
        Arrange::setLocked(tape, {0}, true);
        std::get<TextObject>(tape.objects[2]).wrap = true;
        buffer.close();
        buffer.setData(LabelWriter::toByteArray(tape));
        buffer.open(QIODevice::ReadOnly);
        const auto readTape = reader.read(&buffer);
        QVERIFY2(readTape, qPrintable(reader.errorString()));
        QCOMPARE(readTape->objects.size(), 3);
        for (const LabelObject &object : readTape->objects) {
            const QString name = objectBase(object).name;
            QCOMPARE(objectBase(object).locked, name == u"A"_s);
            QCOMPARE(std::get<TextObject>(object).wrap, name == u"C"_s);
        }

        // Without them, no comment.
        QVERIFY(!LabelWriter::toByteArray(labelWith({{0, 0, 10, 10}})).contains("<!--"));
    }
};

QTEST_GUILESS_MAIN(ArrangeTest)

#include "arrangetest.moc"
