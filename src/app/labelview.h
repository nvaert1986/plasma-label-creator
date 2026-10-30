/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"
#include "objectfactory.h"

#include <QPointer>
#include <QWidget>

#include <optional>

class InlineTextEditor;
class LabelEditor;

/**
 * The label canvas: draws the label as it will print and lets the user
 * select, move and resize objects. Ctrl or Shift and a click adds an object
 * to the selection or takes it out; dragging over empty space selects what
 * the band encloses. Groups select as one; locked objects do not move.
 * Drags change a working copy of the document and are applied to the editor
 * as one undo step on release.
 */
class LabelView : public QWidget
{
    Q_OBJECT

public:
    explicit LabelView(LabelEditor *editor, QWidget *parent = nullptr);

    /** Zoom factor: 1 shows the label at its real size. */
    qreal zoom() const;
    void setZoom(qreal zoom);
    void zoomIn();
    void zoomOut();
    /** Fits the label into the view, and keeps fitting as the view resizes. */
    void zoomToFit();

    QSize sizeHint() const override;

    /** Where the label's top-left corner is drawn, in widget pixels. */
    QPointF labelOrigin() const;
    /** Pixels per twip at the current zoom. */
    qreal scale() const;
    /** The label, and the selected object if any, in widget pixels. */
    QRectF labelRect() const;
    QRectF selectionRect() const;

    /**
     * Starts editing the text of object @p index on the label, if it is a
     * text, address or curved text object; false otherwise.
     */
    bool editText(int index);
    bool isEditingText() const;
    /** Ends editing on the label, keeping the text if @p accept. */
    void finishTextEdit(bool accept);

    /**
     * The part of the label that takes print, in twips on the canvas; the
     * rest is shaded and the part outlined. Empty for the whole label.
     */
    void setPrintArea(const QRectF &area);
    QRectF printArea() const;

    /** Dots over the label at every grid point. */
    void setGridVisible(bool visible);
    bool isGridVisible() const;
    /** Moving and resizing put edges on the grid; holding Alt places freely. */
    void setSnapToGrid(bool snap);
    bool snapsToGrid() const;
    /** The grid's spacing in twips. */
    void setGridSpacing(qreal twips);
    qreal gridSpacing() const;
    /** @p value on the nearest multiple of @p spacing. */
    static qreal snapped(qreal value, qreal spacing);

Q_SIGNALS:
    void editObjectRequested(int index);
    void contextMenuRequested(const QPoint &globalPos);
    void zoomChanged(qreal zoom);
    /** A new object of @p kind was dropped with its centre at @p twips. */
    void objectDropped(ObjectFactory::Kind kind, const QPointF &twips);
    /** Image files were dropped with the first one's centre at @p twips. */
    void imagesDropped(const QStringList &fileNames, const QPointF &twips);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum Handle {
        NoHandle = -1,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
    };

    const LabelCreator::LabelDocument &shownDocument() const;
    QPointF toTwips(const QPointF &widgetPos) const;
    QRectF toWidget(const QRectF &twips) const;
    QPolygonF toWidget(const QPolygonF &twips) const;
    QList<QRectF> handleRects(const QRectF &bounds) const;
    Handle handleAt(const QPointF &pos) const;
    int objectAt(const QPointF &pos) const;
    void updateFitZoom();
    /** Grows the widget beyond its viewport when zoomed in, so it scrolls. */
    void updateMinimumSize();
    void moveSelection(const QPointF &delta, const QString &description);
    /** Text objects are edited on the label, others in their properties. */
    void editObject(int index);
    void updateTextEditorGeometry();

    LabelEditor *m_editor;
    qreal m_zoom = 1.0;
    bool m_fit = false; // labels show at their real size until fitted

    // An ongoing drag: the document as it was, the working copy and where
    // the mouse went down.
    std::optional<LabelCreator::LabelDocument> m_dragDocument;
    LabelCreator::LabelDocument m_dragStart;
    QRectF m_dragStartBounds;
    int m_dragIndex = -1; // the object dragged; the selection may change meanwhile
    // Everything a move drags along, with where it started.
    QList<int> m_dragIndices;
    QList<QRectF> m_dragStartAll;
    QPointF m_pressTwips;
    Handle m_dragHandle = NoHandle;
    bool m_dragMoved = false;

    // An ongoing rubber band, in widget pixels, and what was selected before
    // it when it adds to the selection.
    std::optional<QRectF> m_rubberBand;
    QPointF m_rubberStart;
    QList<int> m_rubberBase;

    QRectF m_printArea;
    bool m_gridVisible = false;
    bool m_snapToGrid = false;
    qreal m_gridSpacing = 113.385826771653543; // twips, 2 mm
    QPointer<InlineTextEditor> m_textEditor;
    int m_textEditIndex = -1;
    // The object's name, to find it again when objects are reordered meanwhile.
    QString m_textEditName;

    // Where something dragged in from outside would land, in widget pixels.
    std::optional<QPointF> m_dropPos;
};
