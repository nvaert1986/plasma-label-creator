/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelview.h"

#include "labeleditor.h"
#include "arrange.h"
#include "dragsupport.h"
#include "inlinetexteditor.h"
#include "labelrenderer.h"
#include "units.h"

#include <KLocalizedString>

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QWheelEvent>

#include <QtMath>

#include <algorithm>
#include <cmath>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int Margin = 32; // pixels around the label
constexpr qreal HandleSize = 7; // pixels
constexpr qreal MinimumSize = 72; // twips, 1/20 inch
constexpr qreal MinimumZoom = 0.25;
constexpr qreal MaximumZoom = 8.0;
constexpr qreal ZoomStep = 1.25;
constexpr qreal NudgeStep = 20; // twips, one point
constexpr qreal LargeNudgeStep = 144; // twips, 1/10 inch

} // namespace

LabelView::LabelView(LabelEditor *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAcceptDrops(true);
    connect(m_editor, &LabelEditor::documentChanged, this, [this]() {
        // Something else (undo by keyboard, say) changed the label during a
        // drag: the drag's copy is out of date, so the drag ends.
        m_dragDocument.reset();
        // Changed meanwhile (the Format toolbar, say): editing goes on in the
        // new fonts, unless the object is gone or no longer matches.
        if (m_textEditor) {
            // Brought to front or sent to back meanwhile: follow it by name.
            const auto &objects = m_editor->document().objects;
            m_textEditIndex = -1;
            for (qsizetype i = 0; i < objects.size(); ++i) {
                if (objectBase(objects.at(i)).name == m_textEditName) {
                    m_textEditIndex = int(i);
                    break;
                }
            }
            if (m_textEditIndex < 0 || !InlineTextEditor::canEdit(objects.at(m_textEditIndex)) || !m_textEditor->updateFonts(objects.at(m_textEditIndex))) {
                finishTextEdit(false);
            } else {
                updateTextEditorGeometry(); // moved or resized meanwhile
            }
        }
        updateFitZoom();
        updateMinimumSize();
        update();
    });
    connect(m_editor, &LabelEditor::selectionChanged, this, qOverload<>(&QWidget::update));
    // The editor's fonts are sized for the zoom it started at.
    connect(this, &LabelView::zoomChanged, this, [this]() {
        finishTextEdit(true);
    });
}

void LabelView::setPrintArea(const QRectF &area)
{
    if (m_printArea != area) {
        m_printArea = area;
        update();
    }
}

QRectF LabelView::printArea() const
{
    return m_printArea;
}

void LabelView::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

bool LabelView::isGridVisible() const
{
    return m_gridVisible;
}

void LabelView::setSnapToGrid(bool snap)
{
    m_snapToGrid = snap;
}

bool LabelView::snapsToGrid() const
{
    return m_snapToGrid;
}

void LabelView::setGridSpacing(qreal twips)
{
    m_gridSpacing = std::max<qreal>(1, twips);
    update();
}

qreal LabelView::gridSpacing() const
{
    return m_gridSpacing;
}

qreal LabelView::snapped(qreal value, qreal spacing)
{
    return spacing > 0 ? std::round(value / spacing) * spacing : value;
}

bool LabelView::isEditingText() const
{
    return m_textEditor;
}

bool LabelView::editText(int index)
{
    finishTextEdit(true);
    const auto &objects = m_editor->document().objects;
    if (index < 0 || index >= objects.size() || !InlineTextEditor::canEdit(objects.at(index))) {
        return false;
    }
    m_editor->setSelection(index);
    m_textEditIndex = index;
    m_textEditName = objectBase(objects.at(index)).name;
    m_textEditor = new InlineTextEditor(objects.at(index), scale(), this);
    connect(m_textEditor, &InlineTextEditor::finished, this, &LabelView::finishTextEdit);
    updateTextEditorGeometry();
    m_textEditor->show();
    m_textEditor->setFocus();
    return true;
}

void LabelView::updateTextEditorGeometry()
{
    if (!m_textEditor || m_textEditIndex < 0 || m_textEditIndex >= m_editor->document().objects.size()) {
        return;
    }
    // Over the object, but large enough to type in when the object is small.
    QRect area = toWidget(objectBase(m_editor->document().objects.at(m_textEditIndex)).bounds).toAlignedRect();
    const int frame = 2 * m_textEditor->frameWidth();
    area.setWidth(std::max(area.width(), 80));
    area.setHeight(std::max(area.height(), qCeil(m_textEditor->document()->size().height()) + frame));
    m_textEditor->setGeometry(area);
}

void LabelView::finishTextEdit(bool accept)
{
    if (!m_textEditor) {
        return;
    }
    InlineTextEditor *editor = m_textEditor;
    m_textEditor = nullptr;
    // Ended from the keyboard, the canvas takes the focus back; ended by
    // clicking elsewhere, it stays there.
    if (editor->hasFocus()) {
        setFocus();
    }
    editor->hide();
    editor->deleteLater();

    const auto &objects = m_editor->document().objects;
    if (accept && m_textEditIndex >= 0 && m_textEditIndex < objects.size()) {
        LabelDocument document = m_editor->document();
        if (editor->apply(document.objects[m_textEditIndex])) {
            m_editor->apply(document, i18n("Edit Text"), m_textEditIndex);
        }
    }
}

void LabelView::editObject(int index)
{
    if (!editText(index)) {
        Q_EMIT editObjectRequested(index);
    }
}

qreal LabelView::zoom() const
{
    return m_zoom;
}

void LabelView::setZoom(qreal zoom)
{
    m_fit = false;
    zoom = std::clamp(zoom, MinimumZoom, MaximumZoom);
    if (!qFuzzyCompare(m_zoom, zoom)) {
        m_zoom = zoom;
        Q_EMIT zoomChanged(m_zoom);
    }
    // Also when leaving fit mode at the same zoom: the canvas may need to scroll.
    updateMinimumSize();
    update();
}

void LabelView::updateMinimumSize()
{
    if (m_fit) {
        setMinimumSize(0, 0);
        return;
    }
    const QSizeF canvas = m_editor->document().canvasSize() * scale();
    setMinimumSize(qCeil(canvas.width()) + 2 * Margin, qCeil(canvas.height()) + 2 * Margin);
}

void LabelView::zoomIn()
{
    setZoom(m_zoom * ZoomStep);
}

void LabelView::zoomOut()
{
    setZoom(m_zoom / ZoomStep);
}

void LabelView::zoomToFit()
{
    m_fit = true;
    updateMinimumSize();
    updateFitZoom();
    update();
}

QSize LabelView::sizeHint() const
{
    return QSize(640, 360);
}

void LabelView::updateFitZoom()
{
    if (!m_fit) {
        return;
    }
    const QSizeF canvas = m_editor->document().canvasSize();
    if (canvas.isEmpty()) {
        return;
    }
    const qreal realSize = logicalDpiX() / Units::TwipsPerInch;
    const qreal zoom = std::min((width() - 2 * Margin) / (canvas.width() * realSize), (height() - 2 * Margin) / (canvas.height() * realSize));
    const qreal clamped = std::clamp(zoom, MinimumZoom, MaximumZoom);
    if (!qFuzzyCompare(m_zoom, clamped)) {
        m_zoom = clamped;
        Q_EMIT zoomChanged(m_zoom);
    }
}

const LabelDocument &LabelView::shownDocument() const
{
    return m_dragDocument ? *m_dragDocument : m_editor->document();
}

qreal LabelView::scale() const
{
    return m_zoom * logicalDpiX() / Units::TwipsPerInch;
}

QPointF LabelView::labelOrigin() const
{
    const QSizeF canvas = shownDocument().canvasSize() * scale();
    return QPointF(std::max<qreal>(Margin, (width() - canvas.width()) / 2.0), std::max<qreal>(Margin, (height() - canvas.height()) / 2.0));
}

QRectF LabelView::labelRect() const
{
    return toWidget(QRectF(QPointF(0, 0), shownDocument().canvasSize()));
}

QRectF LabelView::selectionRect() const
{
    const int selection = m_editor->selection();
    if (selection < 0 || selection >= shownDocument().objects.size()) {
        return {};
    }
    return toWidget(objectBase(shownDocument().objects.at(selection)).bounds);
}

QPointF LabelView::toTwips(const QPointF &widgetPos) const
{
    return (widgetPos - labelOrigin()) / scale();
}

QRectF LabelView::toWidget(const QRectF &twips) const
{
    return QRectF(labelOrigin() + twips.topLeft() * scale(), twips.size() * scale());
}

QPolygonF LabelView::toWidget(const QPolygonF &twips) const
{
    QPolygonF widget;
    for (const QPointF &point : twips) {
        widget << labelOrigin() + point * scale();
    }
    return widget;
}

QList<QRectF> LabelView::handleRects(const QRectF &bounds) const
{
    const QRectF r = toWidget(bounds);
    const QList<QPointF> points = {
        r.topLeft(),
        QPointF(r.center().x(), r.top()),
        r.topRight(),
        QPointF(r.right(), r.center().y()),
        r.bottomRight(),
        QPointF(r.center().x(), r.bottom()),
        r.bottomLeft(),
        QPointF(r.left(), r.center().y()),
    };
    QList<QRectF> rects;
    for (const QPointF &point : points) {
        rects << QRectF(point - QPointF(HandleSize / 2, HandleSize / 2), QSizeF(HandleSize, HandleSize));
    }
    return rects;
}

LabelView::Handle LabelView::handleAt(const QPointF &pos) const
{
    const int selection = m_editor->selection();
    // On tape, cells place and size the objects. Only a single object that
    // is not locked or at an angle (whose sides are not the box's) has handles.
    if (selection < 0 || shownDocument().tape || m_editor->selectedObjects().size() != 1 || shownDocument().isLocked(selection)
        || objectBase(shownDocument().objects.at(selection)).angle != 0) {
        return NoHandle;
    }
    const QList<QRectF> rects = handleRects(objectBase(shownDocument().objects.at(selection)).bounds);
    for (int i = 0; i < rects.size(); ++i) {
        if (rects.at(i).adjusted(-2, -2, 2, 2).contains(pos)) {
            return Handle(i);
        }
    }
    return NoHandle;
}

int LabelView::objectAt(const QPointF &pos) const
{
    const QPointF twips = toTwips(pos);
    const auto &objects = shownDocument().objects;
    // Topmost first; thin lines get some slack so they can be grabbed.
    const qreal slack = 3 / scale();
    for (qsizetype i = objects.size() - 1; i >= 0; --i) {
        if (!shownDocument().isSelectable(int(i))) {
            continue;
        }
        const ObjectBase &base = objectBase(objects.at(i));
        const bool hit = base.angle == 0 ? base.bounds.adjusted(-slack, -slack, slack, slack).contains(twips)
                                         : objectOutline(base).containsPoint(twips, Qt::OddEvenFill);
        if (hit) {
            return int(i);
        }
    }
    return -1;
}

void LabelView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Mid));

    const LabelDocument &document = shownDocument();
    painter.save();
    painter.translate(labelOrigin());
    painter.scale(scale(), scale());
    LabelRenderer renderer(LabelRenderer::Mode::Preview);
    renderer.paint(&painter, document);
    painter.restore();

    // A dot at every grid point; at small zoom levels only every few, so the
    // dots stay apart.
    if (m_gridVisible) {
        qreal step = m_gridSpacing;
        while (step * scale() < 6) {
            step *= 2;
        }
        const QSizeF canvas = document.canvasSize();
        QPolygonF points;
        const int columns = int(canvas.width() / step);
        const int rows = int(canvas.height() / step);
        for (int row = 0; row <= rows; ++row) {
            for (int column = 0; column <= columns; ++column) {
                points << labelOrigin() + QPointF(column * step, row * step) * scale();
            }
        }
        painter.setPen(QPen(QColor(0, 0, 0, 90), 1.5));
        painter.drawPoints(points);
    }

    // Only part of the label takes print (a wrap label's strip): shade the
    // rest, outline the part.
    if (!m_printArea.isEmpty()) {
        QPainterPath outside;
        outside.addRect(labelRect());
        QPainterPath inside;
        inside.addRect(toWidget(m_printArea));
        painter.fillPath(outside.subtracted(inside), QColor(128, 128, 128, 70));
        painter.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(toWidget(m_printArea));
    }

    // Faint frames show where every object is, including empty ones.
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 50), 0, Qt::DotLine));
    for (int i = 0; i < document.objects.size(); ++i) {
        if (document.isShown(i)) {
            painter.drawPolygon(toWidget(objectOutline(objectBase(document.objects.at(i)))));
        }
    }

    // Every selected object framed; a selected group also as a whole, locked
    // objects with a padlock. Handles only on a single object.
    const QColor highlight = palette().color(QPalette::Highlight);
    const QList<int> selected = m_editor->selectedObjects();
    QList<int> groupsDrawn;
    for (const int index : selected) {
        if (index >= document.objects.size()) {
            continue;
        }
        const ObjectBase &base = objectBase(document.objects.at(index));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(highlight, index == m_editor->selection() ? 1.5 : 1));
        painter.drawPolygon(toWidget(objectOutline(base)));
        if (Arrange::isGrouped(document, index) && !groupsDrawn.contains(*base.groupId)) {
            groupsDrawn << *base.groupId;
            QRectF around;
            for (const int member : Arrange::withGroups(document, {index})) {
                const QRectF bounds = objectOutline(objectBase(document.objects.at(member))).boundingRect();
                around = around.isNull() ? bounds : around.united(bounds);
            }
            painter.setPen(QPen(highlight, 1, Qt::DashLine));
            painter.drawRect(toWidget(around).adjusted(-3, -3, 3, 3));
        }
        if (base.locked) {
            const QRectF frame = toWidget(objectOutline(base)).boundingRect();
            const QRectF padlock(frame.right() - 14, frame.top() + 2, 12, 12);
            const QIcon icon = QIcon::fromTheme(u"object-locked"_s);
            if (!icon.isNull()) {
                icon.paint(&painter, padlock.toRect());
            } else {
                // Without an icon theme: a shackle over a body.
                painter.setPen(QPen(highlight, 1.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawArc(QRectF(padlock.left() + 3, padlock.top(), 6, 8), 0, 180 * 16);
                painter.setBrush(highlight);
                painter.drawRect(QRectF(padlock.left() + 2, padlock.top() + 5, 8, 6));
            }
        }
    }
    if (selected.size() == 1 && !document.tape && !document.isLocked(selected.constFirst()) && objectBase(document.objects.at(selected.constFirst())).angle == 0) {
        painter.setPen(QPen(highlight, 1));
        painter.setBrush(palette().color(QPalette::Base));
        for (const QRectF &handle : handleRects(objectBase(document.objects.at(selected.constFirst())).bounds)) {
            painter.drawRect(handle);
        }
    }

    if (m_rubberBand) {
        QColor fill = highlight;
        fill.setAlpha(40);
        painter.setBrush(fill);
        painter.setPen(QPen(highlight, 1, Qt::DashLine));
        painter.drawRect(*m_rubberBand);
    }

    // Cross hairs over the label where a dragged object would be centred.
    if (m_dropPos) {
        const QRectF label = labelRect();
        painter.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DashLine));
        painter.drawLine(QPointF(label.left(), m_dropPos->y()), QPointF(label.right(), m_dropPos->y()));
        painter.drawLine(QPointF(m_dropPos->x(), label.top()), QPointF(m_dropPos->x(), label.bottom()));
    }
}

void LabelView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateFitZoom();
    updateTextEditorGeometry();
}

void LabelView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    setFocus();
    const LabelDocument &document = m_editor->document();
    const bool adding = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    m_dragHandle = adding ? NoHandle : handleAt(event->position());
    if (m_dragHandle == NoHandle) {
        const int index = objectAt(event->position());
        if (index < 0) {
            // Empty space: a rubber band selects what it encloses.
            m_rubberStart = event->position();
            m_rubberBand = QRectF(m_rubberStart, m_rubberStart);
            m_rubberBase = adding ? m_editor->selectedObjects() : QList<int>();
            if (!adding) {
                m_editor->setSelection(-1);
            }
            return;
        }
        const QList<int> clicked = Arrange::withGroups(document, {index});
        QList<int> selection = m_editor->selectedObjects();
        if (adding) {
            if (m_editor->isSelected(index)) {
                for (const int member : clicked) {
                    selection.removeAll(member);
                }
                m_editor->setSelectedObjects(selection);
            } else {
                m_editor->setSelectedObjects(selection + clicked, index);
            }
            return;
        }
        // A click on a selected object keeps the selection, so all of it can be dragged.
        m_editor->setSelectedObjects(m_editor->isSelected(index) ? selection : clicked, index);
    }
    const int selection = m_editor->selection();
    if (selection < 0 || document.tape) {
        return;
    }
    // A move takes along everything selected that is not locked; a resize
    // only the object of the handle.
    m_dragIndices.clear();
    m_dragStartAll.clear();
    const QList<int> dragged = m_dragHandle == NoHandle ? m_editor->selectedObjects() : QList<int>{selection};
    for (const int index : dragged) {
        if (!document.isLocked(index)) {
            m_dragIndices << index;
            m_dragStartAll << objectBase(document.objects.at(index)).bounds;
        }
    }
    if (m_dragIndices.isEmpty()) {
        return;
    }
    m_dragStart = document;
    m_dragDocument = m_dragStart;
    // The object under the mouse leads, unless it is locked.
    m_dragIndex = m_dragIndices.contains(selection) ? selection : m_dragIndices.constFirst();
    m_dragStartBounds = objectBase(m_dragStart.objects.at(m_dragIndex)).bounds;
    m_pressTwips = toTwips(event->position());
    m_dragMoved = false;
}

void LabelView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_rubberBand) {
        m_rubberBand = QRectF(m_rubberStart, event->position()).normalized();
        const QRectF band(toTwips(m_rubberBand->topLeft()), toTwips(m_rubberBand->bottomRight()));
        QList<int> enclosed;
        const auto &objects = m_editor->document().objects;
        for (int i = 0; i < objects.size(); ++i) {
            if (m_editor->document().isSelectable(i) && band.contains(objectOutline(objectBase(objects.at(i))).boundingRect())) {
                enclosed << i;
            }
        }
        m_editor->setSelectedObjects(m_rubberBase + Arrange::withGroups(m_editor->document(), enclosed));
        update();
        return;
    }
    if (!m_dragDocument) {
        const Handle handle = handleAt(event->position());
        if (handle == TopLeft || handle == BottomRight) {
            setCursor(Qt::SizeFDiagCursor);
        } else if (handle == TopRight || handle == BottomLeft) {
            setCursor(Qt::SizeBDiagCursor);
        } else if (handle == Top || handle == Bottom) {
            setCursor(Qt::SizeVerCursor);
        } else if (handle == Left || handle == Right) {
            setCursor(Qt::SizeHorCursor);
        } else {
            const int index = objectAt(event->position());
            const bool movable = index >= 0 && !m_editor->document().tape && !m_editor->document().isLocked(index);
            setCursor(movable ? Qt::SizeAllCursor : Qt::ArrowCursor);
        }
        return;
    }

    const QPointF delta = toTwips(event->position()) - m_pressTwips;
    if (!m_dragMoved && (delta * scale()).manhattanLength() < 3) {
        return;
    }
    m_dragMoved = true;

    QRectF bounds = m_dragStartBounds;
    switch (m_dragHandle) {
    case NoHandle:
        bounds.translate(delta);
        break;
    case TopLeft:
        bounds.setTopLeft(bounds.topLeft() + delta);
        break;
    case Top:
        bounds.setTop(bounds.top() + delta.y());
        break;
    case TopRight:
        bounds.setTopRight(bounds.topRight() + delta);
        break;
    case Right:
        bounds.setRight(bounds.right() + delta.x());
        break;
    case BottomRight:
        bounds.setBottomRight(bounds.bottomRight() + delta);
        break;
    case Bottom:
        bounds.setBottom(bounds.bottom() + delta.y());
        break;
    case BottomLeft:
        bounds.setBottomLeft(bounds.bottomLeft() + delta);
        break;
    case Left:
        bounds.setLeft(bounds.left() + delta.x());
        break;
    }
    // Edges that move go onto the grid; Alt places freely.
    if (m_snapToGrid && !(event->modifiers() & Qt::AltModifier)) {
        const auto snap = [this](qreal value) {
            return snapped(value, m_gridSpacing);
        };
        if (m_dragHandle == NoHandle) {
            bounds.moveTopLeft(QPointF(snap(bounds.left()), snap(bounds.top())));
        } else {
            if (m_dragHandle == TopLeft || m_dragHandle == Left || m_dragHandle == BottomLeft) {
                bounds.setLeft(snap(bounds.left()));
            }
            if (m_dragHandle == TopRight || m_dragHandle == Right || m_dragHandle == BottomRight) {
                bounds.setRight(snap(bounds.right()));
            }
            if (m_dragHandle == TopLeft || m_dragHandle == Top || m_dragHandle == TopRight) {
                bounds.setTop(snap(bounds.top()));
            }
            if (m_dragHandle == BottomLeft || m_dragHandle == Bottom || m_dragHandle == BottomRight) {
                bounds.setBottom(snap(bounds.bottom()));
            }
        }
    }
    bounds = bounds.normalized();
    // Objects already thinner than the minimum, such as lines, keep their
    // thickness instead of jumping to it.
    bounds.setWidth(std::max(bounds.width(), std::min(MinimumSize, m_dragStartBounds.width())));
    bounds.setHeight(std::max(bounds.height(), std::min(MinimumSize, m_dragStartBounds.height())));

    if (m_dragHandle == NoHandle) {
        // Everything moves as far as the object under the mouse.
        const QPointF moved = bounds.topLeft() - m_dragStartBounds.topLeft();
        for (qsizetype i = 0; i < m_dragIndices.size(); ++i) {
            objectBase(m_dragDocument->objects[m_dragIndices.at(i)]).bounds = m_dragStartAll.at(i).translated(moved);
        }
    } else {
        objectBase(m_dragDocument->objects[m_dragIndex]).bounds = bounds;
    }
    update();
}

void LabelView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (m_rubberBand) {
        m_rubberBand.reset();
        update();
        return;
    }
    if (!m_dragDocument) {
        return;
    }
    const LabelDocument result = *m_dragDocument;
    m_dragDocument.reset();
    if (m_dragMoved) {
        const QString description = m_dragHandle != NoHandle ? i18n("Resize Object") : (m_dragIndices.size() > 1 ? i18n("Move Objects") : i18n("Move Object"));
        m_editor->apply(result, description, m_editor->selectedObjects());
    }
    update();
}

void LabelView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int index = objectAt(event->position());
    if (index >= 0) {
        m_editor->setSelectedObjects(Arrange::withGroups(m_editor->document(), {index}), index);
        editObject(index);
    }
}

void LabelView::moveSelection(const QPointF &delta, const QString &description)
{
    if (m_editor->document().tape) {
        return;
    }
    LabelDocument document = m_editor->document();
    bool moved = false;
    for (const int index : m_editor->selectedObjects()) {
        if (!document.isLocked(index)) {
            objectBase(document.objects[index]).bounds.translate(delta);
            moved = true;
        }
    }
    if (moved) {
        m_editor->apply(document, description);
    }
}

void LabelView::keyPressEvent(QKeyEvent *event)
{
    const qreal step = event->modifiers() & Qt::ShiftModifier ? LargeNudgeStep : NudgeStep;
    switch (event->key()) {
    case Qt::Key_Left:
        moveSelection(QPointF(-step, 0), i18n("Move Object"));
        break;
    case Qt::Key_Right:
        moveSelection(QPointF(step, 0), i18n("Move Object"));
        break;
    case Qt::Key_Up:
        moveSelection(QPointF(0, -step), i18n("Move Object"));
        break;
    case Qt::Key_Down:
        moveSelection(QPointF(0, step), i18n("Move Object"));
        break;
    case Qt::Key_Escape:
        // Also ends a rubber band, which would select again on the next move.
        if (m_rubberBand) {
            m_rubberBand.reset();
            update();
        }
        m_editor->setSelection(-1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
        if (m_editor->selection() >= 0) {
            editObject(m_editor->selection());
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void LabelView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else if (event->angleDelta().y() < 0) {
            zoomOut();
        }
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void LabelView::contextMenuEvent(QContextMenuEvent *event)
{
    // On an object outside the selection, the menu is for that object (and its group).
    const int index = objectAt(event->pos());
    if (index >= 0 && !m_editor->isSelected(index)) {
        m_editor->setSelectedObjects(Arrange::withGroups(m_editor->document(), {index}), index);
    } else if (index >= 0) {
        m_editor->setSelectedObjects(m_editor->selectedObjects(), index);
    }
    Q_EMIT contextMenuRequested(event->globalPos());
}

void LabelView::dragEnterEvent(QDragEnterEvent *event)
{
    if (DragSupport::canDrop(event->mimeData())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        m_dropPos = event->position();
        update();
    }
}

void LabelView::dragMoveEvent(QDragMoveEvent *event)
{
    event->setDropAction(Qt::CopyAction);
    event->accept();
    m_dropPos = event->position();
    update();
}

void LabelView::dragLeaveEvent(QDragLeaveEvent *)
{
    m_dropPos.reset();
    update();
}

void LabelView::dropEvent(QDropEvent *event)
{
    m_dropPos.reset();
    update();
    const QPointF twips = toTwips(event->position());
    if (const auto kind = DragSupport::objectKind(event->mimeData())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        setFocus();
        Q_EMIT objectDropped(*kind, twips);
        return;
    }
    const QStringList files = DragSupport::imageFiles(event->mimeData());
    if (!files.isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        setFocus();
        Q_EMIT imagesDropped(files, twips);
    }
}
