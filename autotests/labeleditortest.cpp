/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "alteclabels.h"
#include "dragsupport.h"
#include "labelclipboard.h"
#include "inlinetexteditor.h"
#include "labeleditor.h"
#include "labelview.h"
#include "objectfactory.h"
#include "printpreviewdialog.h"
#include "labelprinter.h"
#include "labelrenderer.h"
#include "units.h"

#include <KLocalizedString>

#include <QDropEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

LabelDocument addressLabel()
{
    LabelDocument document = ObjectFactory::newLabel(u"30252 Address"_s, u"w79h252"_s, QSizeF(78.96, 252));
    document.objects.append(ObjectFactory::create(ObjectFactory::Kind::Text, document));
    return document;
}

// Saves a picture of @p widget when LABELCREATOR_TEST_GRAB names a folder.
void grab(QWidget *widget, const QString &name)
{
    if (qEnvironmentVariableIsSet("LABELCREATOR_TEST_GRAB")) {
        widget->grab().save(qEnvironmentVariable("LABELCREATOR_TEST_GRAB") + u'/' + name + u".png"_s);
    }
}

} // namespace

class LabelEditorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("plasma-label-creator");
    }

    void newLabelsAreLandscapeWhenLong()
    {
        const LabelDocument address = ObjectFactory::newLabel(u"30252 Address"_s, u"w79h252"_s, QSizeF(78.96, 252));
        QCOMPARE(address.orientation, PaperOrientation::Landscape);
        QVERIFY(address.canvasSize().width() > address.canvasSize().height());

        const LabelDocument small = ObjectFactory::newLabel(u"30334"_s, u"w162h90"_s, QSizeF(162, 90));
        QCOMPARE(small.orientation, PaperOrientation::Portrait);
    }

    void namesAreUnique()
    {
        LabelDocument document = addressLabel();
        QCOMPARE(objectBase(document.objects.at(0)).name, u"TEXT"_s);
        document.objects.append(ObjectFactory::create(ObjectFactory::Kind::Text, document));
        document.objects.append(ObjectFactory::create(ObjectFactory::Kind::Text, document));
        QCOMPARE(objectBase(document.objects.at(1)).name, u"TEXT_1"_s);
        QCOMPARE(objectBase(document.objects.at(2)).name, u"TEXT_2"_s);
    }

    void newObjectsFitOnTheLabel_data()
    {
        QTest::addColumn<int>("kind");
        for (int kind = int(ObjectFactory::Kind::Text); kind <= int(ObjectFactory::Kind::CurvedText); ++kind) {
            QTest::addRow("kind %d", kind) << kind;
        }
    }

    void newObjectsFitOnTheLabel()
    {
        QFETCH(int, kind);
        const LabelDocument document = addressLabel();
        const LabelObject object = ObjectFactory::create(ObjectFactory::Kind(kind), document);
        const QRectF canvas(QPointF(0, 0), document.canvasSize());
        QVERIFY(canvas.contains(objectBase(object).bounds));
        QVERIFY(!objectBase(object).bounds.isEmpty());
    }

    void undoAndRedo()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        QVERIFY(!editor.isModified());

        LabelDocument changed = editor.document();
        changed.objects.append(ObjectFactory::create(ObjectFactory::Kind::Barcode, changed));
        editor.apply(changed, u"Insert"_s, 1);
        QCOMPARE(editor.document().objects.size(), 2);
        QCOMPARE(editor.selection(), 1);
        QVERIFY(editor.isModified());

        editor.undoStack()->undo();
        QCOMPARE(editor.document().objects.size(), 1);
        QCOMPARE(editor.selection(), -1);
        QVERIFY(!editor.isModified());

        editor.undoStack()->redo();
        QCOMPARE(editor.document().objects.size(), 2);
        QCOMPARE(editor.selection(), 1);
    }

    void unchangedDocumentAddsNoUndoStep()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        editor.apply(editor.document(), u"Nothing"_s);
        QCOMPARE(editor.undoStack()->count(), 0);
    }

    void saveAndLoad()
    {
        QTemporaryDir dir;
        const QString fileName = dir.filePath(u"test.label"_s);
        LabelEditor editor;
        editor.reset(addressLabel());
        LabelDocument changed = editor.document();
        changed.objects.append(ObjectFactory::create(ObjectFactory::Kind::Rectangle, changed));
        editor.apply(changed, u"Insert"_s);
        QString error;
        QVERIFY2(editor.save(fileName, &error), qPrintable(error));
        QVERIFY(!editor.isModified());

        LabelEditor reopened;
        QVERIFY2(reopened.load(fileName, &error), qPrintable(error));
        QCOMPARE(reopened.document().objects, editor.document().objects);
        QCOMPARE(reopened.document().canvasSize(), editor.document().canvasSize());
        QCOMPARE(reopened.fileName(), fileName);
    }

    void dragMovesTheObject()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setZoom(1.0);

        const QRectF before = objectBase(editor.document().objects.at(0)).bounds;
        const qreal pixelsPerTwip = view.logicalDpiX() / 1440.0;
        // The centre of the object on screen.
        const QSizeF canvas = editor.document().canvasSize() * pixelsPerTwip;
        const QPointF origin(std::max(32.0, (view.width() - canvas.width()) / 2), std::max(32.0, (view.height() - canvas.height()) / 2));
        const QPoint start = (origin + before.center() * pixelsPerTwip).toPoint();

        QTest::mousePress(&view, Qt::LeftButton, {}, start);
        QCOMPARE(editor.selection(), 0);
        QTest::mouseMove(&view, start + QPoint(20, 0));
        QTest::mouseMove(&view, start + QPoint(40, 10));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, start + QPoint(40, 10));

        const QRectF after = objectBase(editor.document().objects.at(0)).bounds;
        QCOMPARE(after.size(), before.size());
        QVERIFY(qAbs(after.x() - before.x() - 40 / pixelsPerTwip) < 1 / pixelsPerTwip);
        QVERIFY(qAbs(after.y() - before.y() - 10 / pixelsPerTwip) < 1 / pixelsPerTwip);
        QCOMPARE(editor.undoStack()->count(), 1);

        editor.undoStack()->undo();
        QCOMPARE(objectBase(editor.document().objects.at(0)).bounds, before);
    }

    void dragSnapsToTheGrid()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setZoom(1.0);
        const qreal grid = Units::mmToTwips(2);
        view.setGridSpacing(grid);
        view.setSnapToGrid(true);

        const QRectF before = objectBase(editor.document().objects.at(0)).bounds;
        const QPoint start = (view.labelOrigin() + before.center() * view.scale()).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, {}, start);
        QTest::mouseMove(&view, start + QPoint(20, 0));
        QTest::mouseMove(&view, start + QPoint(37, 11));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, start + QPoint(37, 11));

        // The top-left corner lands on the grid; the size stays.
        const QRectF after = objectBase(editor.document().objects.at(0)).bounds;
        QCOMPARE(after.size(), before.size());
        QVERIFY(qAbs(std::remainder(after.x(), grid)) < 0.01);
        QVERIFY(qAbs(std::remainder(after.y(), grid)) < 0.01);
        QVERIFY(after.x() > before.x());

        QCOMPARE(LabelView::snapped(130, 100), 100.0);
        QCOMPARE(LabelView::snapped(-60, 100), -100.0);
        QCOMPARE(LabelView::snapped(42, 0), 42.0);
    }

    void textPrintsWhiteOnBlack()
    {
        LabelDocument document = addressLabel();
        LabelObject &text = document.objects[0];
        QVERIFY(std::holds_alternative<TextObject>(text));
        QVERIFY(ObjectFactory::canInvert(text));
        QVERIFY(!ObjectFactory::isInverted(text));
        ObjectFactory::setInverted(text, true);
        QVERIFY(ObjectFactory::isInverted(text));
        QCOMPARE(objectBase(text).backColor, QColor(Qt::black));
        QCOMPARE(std::get_if<TextObject>(&text)->spans.constFirst().color, QColor(Qt::white));
        ObjectFactory::setInverted(text, false);
        QVERIFY(!ObjectFactory::isInverted(text));
        QCOMPARE(objectBase(text).backColor.alpha(), 0);

        // Counters use the object's colour rather than the runs'.
        LabelObject counter = ObjectFactory::create(ObjectFactory::Kind::Counter, document);
        ObjectFactory::setInverted(counter, true);
        QVERIFY(ObjectFactory::isInverted(counter));
        QCOMPARE(objectBase(counter).foreColor, QColor(Qt::white));

        // Shapes and barcodes have no text to invert.
        LabelObject shape = ShapeObject();
        QVERIFY(!ObjectFactory::canInvert(shape));
        ObjectFactory::setInverted(shape, true);
        QVERIFY(!ObjectFactory::isInverted(shape));

        // White on black prints: the box is black, the text leaves it white.
        LabelDocument label = addressLabel();
        ObjectFactory::setInverted(label.objects[0], true);
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        const QImage image = renderer.renderImage(label, 150);
        const QRectF box = objectBase(label.objects.at(0)).bounds;
        int black = 0;
        int white = 0;
        const QRect pixels(Units::twipsToDots(box.x(), 150), Units::twipsToDots(box.y(), 150), Units::twipsToDots(box.width(), 150), Units::twipsToDots(box.height(), 150));
        for (int y = pixels.top() + 1; y < pixels.bottom(); ++y) {
            for (int x = pixels.left() + 1; x < pixels.right(); ++x) {
                (qGray(image.pixel(x, y)) < 128 ? black : white)++;
            }
        }
        QVERIFY(black > white); // mostly box
        QVERIFY(white > 50); // and the letters
    }

    void printPreviewStepsThroughTheLabels()
    {
        // Three labels of a run, the way a counter makes them differ.
        LabelDocument label = addressLabel();
        label.objects = {ObjectFactory::create(ObjectFactory::Kind::Counter, label)};
        objectBase(label.objects[0]).bounds = QRectF(200, 200, 3000, 1000);
        PaperSize paper;
        paper.keyword = u"w79h252"_s;
        paper.name = u"30252 Address"_s;
        paper.size = QSizeF(79, 252);
        paper.imageableArea = QRectF(4.32, 16.56, 71.76, 231.12);
        QList<QImage> pages;
        for (int step = 0; step < 3; ++step) {
            pages << LabelPrinter::paperImage(label, paper, 300, nullptr, step);
        }
        QVERIFY(pages.at(0) != pages.at(1));

        PrintPreviewDialog dialog(pages, 300, u"DYMO LabelWriter 450"_s, {});
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        const auto positionShows = [&dialog](const QString &text) {
            const auto labels = dialog.findChildren<QLabel *>();
            return std::any_of(labels.cbegin(), labels.cend(), [&text](const QLabel *label) {
                return label->text() == text;
            });
        };
        QVERIFY(positionShows(u"Label 1 of 3"_s));
        QPushButton *next = nullptr;
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text().contains(u"Next"_s)) {
                next = button;
            }
        }
        QVERIFY(next);
        next->click();
        QVERIFY(positionShows(u"Label 2 of 3"_s));
        next->click();
        QVERIFY(positionShows(u"Label 3 of 3"_s));
        QVERIFY(!next->isEnabled());
        if (qEnvironmentVariableIsSet("LABELCREATOR_TEST_GRAB")) {
            dialog.grab().save(qEnvironmentVariable("LABELCREATOR_TEST_GRAB"));
        }
    }

    void clickOnEmptySpaceDeselects()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        editor.setSelection(0);
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTest::mouseClick(&view, Qt::LeftButton, {}, QPoint(5, 5));
        QCOMPARE(editor.selection(), -1);
        QCOMPARE(editor.undoStack()->count(), 0);
    }

    void severalObjectsSelectAndMoveTogether()
    {
        LabelDocument document = addressLabel();
        document.objects.clear();
        for (const QRectF &rect : {QRectF(200, 200, 600, 400), QRectF(1200, 200, 600, 400), QRectF(2200, 600, 600, 400)}) {
            ShapeObject shape;
            shape.bounds = rect;
            document.objects << shape;
        }
        objectBase(document.objects[2]).locked = true;
        LabelEditor editor;
        editor.reset(document);
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setZoom(1.0);
        const auto centreOf = [&](int index) {
            return (view.labelOrigin() + objectBase(editor.document().objects.at(index)).bounds.center() * view.scale()).toPoint();
        };

        // Ctrl adds and takes out again; the last one clicked is the current one.
        QTest::mouseClick(&view, Qt::LeftButton, {}, centreOf(0));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::ControlModifier, centreOf(1));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::ShiftModifier, centreOf(2));
        QCOMPARE(editor.selectedObjects(), (QList<int>{0, 1, 2}));
        QCOMPARE(editor.selection(), 2);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::ControlModifier, centreOf(1));
        QCOMPARE(editor.selectedObjects(), (QList<int>{0, 2}));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::ControlModifier, centreOf(1));

        // A drag takes along everything selected except what is locked.
        const QPoint start = centreOf(0);
        QTest::mousePress(&view, Qt::LeftButton, {}, start);
        QCOMPARE(editor.selectedObjects(), (QList<int>{0, 1, 2}));
        QTest::mouseMove(&view, start + QPoint(20, 0));
        QTest::mouseMove(&view, start + QPoint(40, 0));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, start + QPoint(40, 0));
        const qreal moved = objectBase(editor.document().objects.at(0)).bounds.left() - 200;
        QVERIFY(moved > 0);
        QCOMPARE(objectBase(editor.document().objects.at(1)).bounds.left(), 1200 + moved);
        QCOMPARE(objectBase(editor.document().objects.at(2)).bounds.left(), 2200.0);
        QCOMPARE(editor.undoStack()->count(), 1);
        QCOMPARE(editor.selectedObjects(), (QList<int>{0, 1, 2}));

        // The arrow keys too.
        QTest::keyClick(&view, Qt::Key_Down);
        QCOMPARE(objectBase(editor.document().objects.at(1)).bounds.top(), 220.0);
        QCOMPARE(objectBase(editor.document().objects.at(2)).bounds.top(), 600.0);

        // Undo brings the selection back with the document.
        editor.setSelection(-1);
        editor.undoStack()->undo();
        QCOMPARE(editor.selectedObjects(), (QList<int>{0, 1, 2}));

        // A band from empty space selects what it encloses: not the third.
        const QRectF second = objectBase(editor.document().objects.at(1)).bounds;
        const QPoint end = (view.labelOrigin() + second.bottomRight() * view.scale()).toPoint() + QPoint(3, 3);
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(2, 2));
        QCOMPARE(editor.selectedObjects(), QList<int>());
        QTest::mouseMove(&view, end);
        QTest::mouseRelease(&view, Qt::LeftButton, {}, end);
        QCOMPARE(editor.selectedObjects(), (QList<int>{0, 1}));
        QCOMPARE(editor.undoStack()->count(), 2);

        // A group selects as one.
        LabelDocument grouped = editor.document();
        objectBase(grouped.objects[1]).groupId = 0;
        objectBase(grouped.objects[2]).groupId = 0;
        editor.apply(grouped, u"Group"_s, -1);
        QTest::mouseClick(&view, Qt::LeftButton, {}, centreOf(1));
        QCOMPARE(editor.selectedObjects(), (QList<int>{1, 2}));
        QCOMPARE(editor.selection(), 1);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::ControlModifier, centreOf(0));
        grab(&view, u"multiselect"_s);
    }

    void severalObjectsCopyWithTheirGroup()
    {
        LabelDocument document = addressLabel();
        LabelObject second = document.objects.at(0);
        objectBase(second).name = u"TEXT_2"_s;
        objectBase(second).bounds.translate(0, 400);
        document.objects << second;
        objectBase(document.objects[0]).groupId = 0;
        objectBase(document.objects[1]).groupId = 0;
        std::unique_ptr<QMimeData> data(LabelClipboard::mimeData({document.objects.at(0), document.objects.at(1)}, document));
        QCOMPARE(data->text(), u"Text\nText"_s);
        const QList<LabelObject> pasted = LabelClipboard::objectsToPaste(data.get(), document);
        QCOMPARE(pasted.size(), 2);
        // Moved on together, and a group of their own.
        QCOMPARE(objectBase(pasted.at(0)).bounds, objectBase(document.objects.at(0)).bounds.translated(144, 144));
        QCOMPARE(objectBase(pasted.at(1)).bounds, objectBase(document.objects.at(1)).bounds.translated(144, 144));
        QCOMPARE(objectBase(pasted.at(0)).groupId, std::optional<int>(1));
        QCOMPARE(objectBase(pasted.at(1)).groupId, std::optional<int>(1));
    }

    void arrowKeysNudge()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        editor.setSelection(0);
        LabelView view(&editor);
        const QRectF before = objectBase(editor.document().objects.at(0)).bounds;
        QTest::keyClick(&view, Qt::Key_Right);
        QTest::keyClick(&view, Qt::Key_Down, Qt::ShiftModifier);
        const QRectF after = objectBase(editor.document().objects.at(0)).bounds;
        QCOMPARE(after.topLeft(), before.topLeft() + QPointF(20, 144));
    }

    void droppedObjectsStayOnTheLabel()
    {
        const LabelDocument document = addressLabel();
        const QSizeF canvas = document.canvasSize();
        LabelObject object = ObjectFactory::create(ObjectFactory::Kind::Barcode, document);
        const QSizeF size = objectBase(object).bounds.size();

        ObjectFactory::centreOn(object, document, QPointF(canvas.width() / 2, canvas.height() / 2));
        QCOMPARE(objectBase(object).bounds.center(), QPointF(canvas.width() / 2, canvas.height() / 2));

        // Dropped near a corner: pushed back onto the label, size unchanged.
        ObjectFactory::centreOn(object, document, QPointF(-500, canvas.height() + 500));
        QCOMPARE(objectBase(object).bounds.size(), size);
        QCOMPARE(objectBase(object).bounds.left(), 0.0);
        QCOMPARE(objectBase(object).bounds.bottom(), canvas.height());
    }

    void copiesKeepTheirName()
    {
        LabelDocument document = addressLabel(); // has TEXT
        QCOMPARE(ObjectFactory::copyName(document, u"TEXT"_s), u"TEXT_1"_s);
        TextObject named;
        named.name = u"MY_TEXT"_s;
        document.objects.append(named);
        QCOMPARE(ObjectFactory::copyName(document, u"MY_TEXT"_s), u"MY_TEXT_1"_s);
        QCOMPARE(ObjectFactory::copyName(document, u"MY_TEXT_7"_s), u"MY_TEXT_1"_s);
    }

    void resizingALineKeepsItsThickness()
    {
        LabelDocument document = addressLabel();
        document.objects = {ObjectFactory::create(ObjectFactory::Kind::HorizontalLine, document)};
        LabelEditor editor;
        editor.reset(document);
        editor.setSelection(0);
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        const QRectF before = objectBase(editor.document().objects.at(0)).bounds;
        const QPoint right = (view.labelOrigin() + QPointF(before.right(), before.center().y()) * view.scale()).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, {}, right);
        QTest::mouseMove(&view, right - QPoint(30, 0));
        QTest::mouseMove(&view, right - QPoint(60, 0));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, right - QPoint(60, 0));

        const QRectF after = objectBase(editor.document().objects.at(0)).bounds;
        QVERIFY(after.width() < before.width());
        QCOMPARE(after.height(), before.height());
        QCOMPARE(after.top(), before.top());
    }

    void escapeDuringADragIsHarmless()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        const QRectF before = objectBase(editor.document().objects.at(0)).bounds;
        const QPoint centre = (view.labelOrigin() + before.center() * view.scale()).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, {}, centre);
        QTest::keyClick(&view, Qt::Key_Escape);
        QCOMPARE(editor.selection(), -1);
        QTest::mouseMove(&view, centre + QPoint(20, 0));
        QTest::mouseMove(&view, centre + QPoint(40, 0));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, centre + QPoint(40, 0));
        QVERIFY(objectBase(editor.document().objects.at(0)).bounds.left() > before.left());
    }

    void undoDuringADragWins()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        editor.setSelection(0);
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        const QRectF original = objectBase(editor.document().objects.at(0)).bounds;
        QTest::keyClick(&view, Qt::Key_Down); // something to undo
        const QRectF nudged = objectBase(editor.document().objects.at(0)).bounds;
        const QPoint centre = (view.labelOrigin() + nudged.center() * view.scale()).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, {}, centre);
        QTest::mouseMove(&view, centre + QPoint(20, 0));
        QTest::mouseMove(&view, centre + QPoint(40, 0));
        editor.undoStack()->undo();
        QTest::mouseMove(&view, centre + QPoint(60, 0));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, centre + QPoint(60, 0));

        // The undo stands; the drag that started before it is dropped.
        QCOMPARE(objectBase(editor.document().objects.at(0)).bounds, original);
        QVERIFY(!editor.undoStack()->canUndo());
    }

    void copiedObjectsPasteWithANewName()
    {
        const LabelDocument document = addressLabel(); // one TEXT
        std::unique_ptr<QMimeData> data(LabelClipboard::mimeData({document.objects.at(0)}, document));
        QVERIFY(LabelClipboard::canPaste(data.get()));
        QCOMPARE(data->text(), u"Text"_s); // other programs get the text

        // Pasted into the same label: a name of its own, not on top of the original.
        const QList<LabelObject> pasted = LabelClipboard::objectsToPaste(data.get(), document);
        QCOMPARE(pasted.size(), 1);
        QCOMPARE(objectBase(pasted.at(0)).name, u"TEXT_1"_s);
        QCOMPARE(objectBase(pasted.at(0)).bounds, objectBase(document.objects.at(0)).bounds.translated(144, 144));
        LabelObject expected = document.objects.at(0);
        objectBase(expected).name = u"TEXT_1"_s;
        objectBase(expected).bounds.translate(144, 144);
        QVERIFY(pasted.at(0) == expected);

        // Into an empty label: unchanged.
        LabelDocument empty = document;
        empty.objects.clear();
        QVERIFY(LabelClipboard::objectsToPaste(data.get(), empty).at(0) == document.objects.at(0));
    }

    void textAndImagesFromOtherProgramsPaste()
    {
        const LabelDocument document = addressLabel();
        QMimeData text;
        text.setText(u"Hello\nworld "_s);
        const QList<LabelObject> fromText = LabelClipboard::objectsToPaste(&text, document);
        QCOMPARE(fromText.size(), 1);
        QCOMPARE(std::get<TextObject>(fromText.at(0)).plainText(), u"Hello\nworld"_s);

        QMimeData image;
        QImage picture(10, 10, QImage::Format_RGB32);
        picture.fill(Qt::black);
        image.setImageData(picture);
        const QList<LabelObject> fromImage = LabelClipboard::objectsToPaste(&image, document);
        QCOMPARE(fromImage.size(), 1);
        QCOMPARE(QImage::fromData(std::get<ImageObject>(fromImage.at(0)).imageData).size(), QSize(10, 10));

        QMimeData nothing;
        QVERIFY(!LabelClipboard::canPaste(&nothing));
        QVERIFY(LabelClipboard::objectsToPaste(&nothing, document).isEmpty());
    }

    void objectsArePositionedOnTheLabel()
    {
        const LabelDocument document = addressLabel();
        const QSizeF canvas = document.canvasSize();
        LabelObject object = ObjectFactory::create(ObjectFactory::Kind::Barcode, document);
        objectBase(object).bounds.setSize(QSizeF(1000, 500));

        ObjectFactory::alignOnLabel(object, document, Qt::AlignLeft | Qt::AlignTop);
        QCOMPARE(objectBase(object).bounds.topLeft(), QPointF(144, 144));
        ObjectFactory::alignOnLabel(object, document, Qt::AlignRight);
        QCOMPARE(objectBase(object).bounds.right(), canvas.width() - 144);
        QCOMPARE(objectBase(object).bounds.top(), 144.0); // vertical untouched
        ObjectFactory::alignOnLabel(object, document, Qt::AlignCenter);
        QCOMPARE(objectBase(object).bounds.center(), QPointF(canvas.width() / 2, canvas.height() / 2));
        ObjectFactory::alignOnLabel(object, document, Qt::AlignBottom);
        QCOMPARE(objectBase(object).bounds.bottom(), canvas.height() - 144);
        QCOMPARE(objectBase(object).bounds.size(), QSizeF(1000, 500));
    }

    void textIsEditedOnTheLabel()
    {
        LabelEditor editor;
        editor.reset(addressLabel()); // one TEXT, "Text"
        LabelView view(&editor);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        const QRectF bounds = objectBase(editor.document().objects.at(0)).bounds;
        QTest::mouseDClick(&view, Qt::LeftButton, {}, (view.labelOrigin() + bounds.center() * view.scale()).toPoint());
        auto *inline_ = view.findChild<InlineTextEditor *>();
        QVERIFY(inline_ && inline_->isVisible());
        QVERIFY(view.isEditingText());

        QTest::keyClicks(inline_, u" on the label"_s);
        QTest::keyClick(inline_, Qt::Key_Return); // a new line, not done yet
        QTest::keyClicks(inline_, u"two"_s);
        QVERIFY(view.isEditingText());
        QTest::keyClick(inline_, Qt::Key_Return, Qt::ControlModifier);
        QVERIFY(!view.isEditingText());
        QCOMPARE(std::get<TextObject>(editor.document().objects.at(0)).plainText(), u"Text on the label\ntwo"_s);
        QCOMPARE(editor.undoStack()->count(), 1);
        editor.undoStack()->undo();
        QCOMPARE(std::get<TextObject>(editor.document().objects.at(0)).plainText(), u"Text"_s);
    }

    void escapeCancelsEditing()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        editor.setSelection(0);
        LabelView view(&editor);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTest::keyClick(&view, Qt::Key_F2);
        auto *inline_ = view.findChild<InlineTextEditor *>();
        QVERIFY(inline_);
        QTest::keyClicks(inline_, u"xyz"_s);
        QTest::keyClick(inline_, Qt::Key_Escape);
        QVERIFY(!view.isEditingText());
        QCOMPARE(std::get<TextObject>(editor.document().objects.at(0)).plainText(), u"Text"_s);
        QCOMPARE(editor.undoStack()->count(), 0);
    }

    void editingKeepsMixedFonts()
    {
        LabelDocument document = addressLabel();
        auto &text = std::get<TextObject>(document.objects[0]);
        LabelFont bold{u"Arial"_s, 14};
        bold.bold = true;
        text.spans = {{u"Bold "_s, bold, Qt::black}, {u"plain"_s, LabelFont{u"Arial"_s, 10}, Qt::red}};

        InlineTextEditor inline_(document.objects.at(0), 0.1);
        inline_.moveCursor(QTextCursor::End);
        QTest::keyClicks(&inline_, u" text"_s);
        inline_.moveCursor(QTextCursor::Start);
        QTest::keyClicks(&inline_, u"Very "_s);
        LabelObject edited = document.objects.at(0);
        QVERIFY(inline_.apply(edited));
        const QList<TextSpan> spans = std::get<TextObject>(edited).spans;
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans.at(0).text, u"Very Bold "_s);
        QVERIFY(spans.at(0).font == bold);
        QCOMPARE(spans.at(1).text, u"plain text"_s);
        QCOMPARE(spans.at(1).font.size, 10.0);
        QCOMPARE(spans.at(1).color, QColor(Qt::red));
    }

    void untouchedTextStaysAsItWas()
    {
        LabelDocument document = addressLabel();
        std::get<TextObject>(document.objects[0]).spans = {{u"A"_s, LabelFont(), Qt::black}, {u"\nB"_s, LabelFont{u"Arial"_s, 20}, Qt::black}};
        InlineTextEditor inline_(document.objects.at(0), 0.1);
        LabelObject object = document.objects.at(0);
        QVERIFY(!inline_.apply(object)); // nothing typed: the runs are not rebuilt
        QVERIFY(object == document.objects.at(0));
    }

    void curvedTextHasOneLine()
    {
        LabelDocument document = addressLabel();
        document.objects = {ObjectFactory::create(ObjectFactory::Kind::CurvedText, document)};
        LabelEditor editor;
        editor.reset(document);
        editor.setSelection(0);
        LabelView view(&editor);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTest::keyClick(&view, Qt::Key_Return);
        auto *inline_ = view.findChild<InlineTextEditor *>();
        QVERIFY(inline_);
        QTest::keyClicks(inline_, u"!"_s);
        QTest::keyClick(inline_, Qt::Key_Return); // finishes
        QVERIFY(!view.isEditingText());
        QCOMPARE(std::get<CircularTextObject>(editor.document().objects.at(0)).text, u"Curved text!"_s);
    }

    void otherObjectsOpenTheirProperties()
    {
        LabelDocument document = addressLabel();
        document.objects = {ObjectFactory::create(ObjectFactory::Kind::Barcode, document)};
        LabelEditor editor;
        editor.reset(document);
        editor.setSelection(0);
        LabelView view(&editor);
        QSignalSpy properties(&view, &LabelView::editObjectRequested);
        QTest::keyClick(&view, Qt::Key_Return);
        QCOMPARE(properties.count(), 1);
        QVERIFY(!view.isEditingText());
    }

    void clickingElsewhereKeepsTheText()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        editor.setSelection(0);
        QWidget window;
        auto *view = new LabelView(&editor, &window);
        auto *other = new QLineEdit(&window);
        other->move(0, 450);
        view->resize(800, 400);
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(view->editText(0));
        auto *inline_ = view->findChild<InlineTextEditor *>();
        QTest::keyClicks(inline_, u"!"_s);
        other->setFocus(Qt::MouseFocusReason);
        QVERIFY(!view->isEditingText());
        QCOMPARE(std::get<TextObject>(editor.document().objects.at(0)).plainText(), u"Text!"_s);
        QVERIFY(other->hasFocus()); // not taken back by the canvas
    }

    void formatChangesWhileEditingAreKept()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        LabelView view(&editor);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(view.editText(0));
        auto *inline_ = view.findChild<InlineTextEditor *>();
        QTest::keyClicks(inline_, u" typed"_s);

        // The Format toolbar makes the object bold meanwhile.
        LabelDocument bold = editor.document();
        std::get<TextObject>(bold.objects[0]).spans[0].font.bold = true;
        editor.apply(bold, u"Bold"_s);
        QVERIFY(view.isEditingText());

        view.finishTextEdit(true);
        const TextObject &text = std::get<TextObject>(editor.document().objects.at(0));
        QCOMPARE(text.plainText(), u"Text typed"_s);
        QVERIFY(text.spans.at(0).font.bold);

        // Deleting the object meanwhile ends editing without a trace.
        QVERIFY(view.editText(0));
        LabelDocument empty = editor.document();
        empty.objects.clear();
        editor.apply(empty, u"Delete"_s);
        QVERIFY(!view.isEditingText());
    }

    void editingFollowsTheObjectWhenReordered()
    {
        LabelDocument document = addressLabel(); // TEXT
        document.objects.append(ObjectFactory::create(ObjectFactory::Kind::Text, document)); // TEXT_1
        std::get<TextObject>(document.objects[1]).spans[0].text = u"Other"_s;
        LabelEditor editor;
        editor.reset(document);
        LabelView view(&editor);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(view.editText(0));
        auto *inline_ = view.findChild<InlineTextEditor *>();
        QTest::keyClicks(inline_, u" edited"_s);

        // "Send to Back" on the other object while typing (from the menu,
        // which does not end editing): TEXT moves from index 0 to 1.
        LabelDocument reordered = editor.document();
        reordered.objects.move(1, 0);
        editor.apply(reordered, u"Send to Back"_s);
        QVERIFY(view.isEditingText());

        view.finishTextEdit(true);
        const auto &objects = editor.document().objects;
        QCOMPARE(objectBase(objects.at(1)).name, u"TEXT"_s);
        QCOMPARE(std::get<TextObject>(objects.at(1)).plainText(), u"Text edited"_s);
        QCOMPARE(std::get<TextObject>(objects.at(0)).plainText(), u"Other"_s);
    }

    void labelsFitWhatThePrinterMeasured()
    {
        // The Altec measured 41.5 mm, which may include up to 3 mm of gap.
        QVERIFY(AltecLabels::fitsLoaded(40, 41.5));
        QVERIFY(AltecLabels::fitsLoaded(41.5, 41.5));
        QVERIFY(AltecLabels::fitsLoaded(42, 41.5));
        QVERIFY(!AltecLabels::fitsLoaded(32, 41.5)); // the DYMO 30334 printed by mistake
        QVERIFY(!AltecLabels::fitsLoaded(50, 41.5));

        // Only by length: the printer's print width is whatever the last job set.
        const QList<PaperSize> labels = {PaperSize::custom(u"A 80 x 40"_s, QSizeF(80, 40)),
                                         PaperSize::custom(u"B 25 x 13"_s, QSizeF(25, 13)),
                                         PaperSize::custom(u"C 100 x 41"_s, QSizeF(100, 41))};
        const QList<PaperSize> fitting = AltecLabels::fitting(labels, 41.5);
        QCOMPARE(fitting.size(), 2);
        QCOMPARE(fitting.at(0).name, u"A 80 x 40"_s);
        QCOMPARE(fitting.at(1).name, u"C 100 x 41"_s);

        QVERIFY(AltecLabels::isAltec(u"ATP-600Pro"_s));
        QVERIFY(AltecLabels::isAltec(u"Altec ATP-600 Pro"_s));
        QVERIFY(AltecLabels::isAltec(u"atp-3000"_s));
        QVERIFY(!AltecLabels::isAltec(u"DYMO LabelWriter 450 Turbo"_s));
        QVERIFY(!AltecLabels::isAltec(u"ATP-Office Zebra"_s)); // not an ATP model

        // Wrap labels from the catalogue take print on their strip only.
        const QList<PaperSize> catalogue = AltecLabels::all();
        QVERIFY(!catalogue.isEmpty());
        {
            const auto wrap = std::find_if(catalogue.cbegin(), catalogue.cend(), [](const PaperSize &paper) {
                return paper.group == u"Wikkellabels"_s && paper.name.contains(u"24 × 57"_s) && !paper.name.contains(u"across"_s);
            });
            QVERIFY(wrap != catalogue.cend());
            // 24 mm across the head, the first 19 mm of 57 along the feed.
            QCOMPARE(qRound(wrap->printArea.width() * Units::MmPerInch / 72.0), 24);
            QCOMPARE(qRound(wrap->printArea.height() * Units::MmPerInch / 72.0), 19);
            QCOMPARE(wrap->printArea.top(), 0.0);
            const int strips = std::count_if(catalogue.cbegin(), catalogue.cend(), [](const PaperSize &paper) {
                return !paper.printArea.isEmpty();
            });
            QCOMPARE(strips, 15);
        }
    }

    void addressLineFontsFollowTheLines()
    {
        LabelDocument document = addressLabel();
        auto address = std::get<AddressObject>(ObjectFactory::create(ObjectFactory::Kind::Address, document));
        QCOMPARE(address.lineFonts.size(), 3); // "Name\nStreet 1\n1234 AB City"
        LabelFont big{u"Arial"_s, 20};
        address.lineFonts[2] = big;

        address.spans = {{u"One\nTwo\nThree\nFour\nFive"_s, LabelFont(), Qt::black}};
        address.fitLineFonts();
        QCOMPARE(address.lineFonts.size(), 5);
        QVERIFY(address.lineFonts.at(4) == big); // new lines take the last line's font

        address.spans = {{u"Only"_s, LabelFont(), Qt::black}};
        address.fitLineFonts();
        QCOMPARE(address.lineFonts.size(), 1);

        // Edited on the label: the fonts follow there too.
        LabelObject object = address;
        InlineTextEditor inline_(object, 0.1);
        inline_.moveCursor(QTextCursor::End);
        QTest::keyClick(&inline_, Qt::Key_Return);
        QTest::keyClicks(&inline_, u"Second"_s);
        QVERIFY(inline_.apply(object));
        QCOMPARE(std::get<AddressObject>(object).lineFonts.size(), 2);
    }

    void dragDataRoundTrips()
    {
        std::unique_ptr<QMimeData> object(DragSupport::objectMimeData(ObjectFactory::Kind::Barcode));
        QCOMPARE(DragSupport::objectKind(object.get()), ObjectFactory::Kind::Barcode);
        QVERIFY(DragSupport::imageFiles(object.get()).isEmpty());

        const QString image = QFINDTESTDATA("data/mixed.label");
        std::unique_ptr<QMimeData> notAnImage(DragSupport::imageMimeData({image}));
        QVERIFY(!DragSupport::canDrop(notAnImage.get()));

        QTemporaryDir dir;
        const QString png = dir.filePath(u"dot.png"_s);
        QImage(4, 4, QImage::Format_RGB32).save(png);
        std::unique_ptr<QMimeData> images(DragSupport::imageMimeData({png}));
        QCOMPARE(DragSupport::imageFiles(images.get()), QStringList{png});
        QVERIFY(!DragSupport::objectKind(images.get()));
    }

    void droppingAnObjectReportsWhere()
    {
        LabelEditor editor;
        editor.reset(addressLabel());
        LabelView view(&editor);
        view.resize(800, 400);
        QSignalSpy dropped(&view, &LabelView::objectDropped);
        std::unique_ptr<QMimeData> data(DragSupport::objectMimeData(ObjectFactory::Kind::Text));
        const QPointF position = view.labelOrigin() + QPointF(100, 50);
        // Qt hands the drop to the widget that accepted the drag entering it.
        QDragEnterEvent enter(position.toPoint(), Qt::CopyAction, data.get(), Qt::LeftButton, {});
        QCoreApplication::sendEvent(&view, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(position, Qt::CopyAction, data.get(), Qt::LeftButton, {});
        QCoreApplication::sendEvent(&view, &drop);
        QCOMPARE(dropped.count(), 1);
        QCOMPARE(dropped.at(0).at(0).value<ObjectFactory::Kind>(), ObjectFactory::Kind::Text);
        const QPointF twips = dropped.at(0).at(1).toPointF();
        QVERIFY(qAbs(twips.x() - 100 / view.scale()) < 0.01);
        QVERIFY(qAbs(twips.y() - 50 / view.scale()) < 0.01);
    }
};

QTEST_MAIN(LabelEditorTest)

#include "labeleditortest.moc"
