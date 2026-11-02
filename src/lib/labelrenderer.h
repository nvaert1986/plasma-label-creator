/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDateTime>
#include <QImage>
#include <QStringList>

class QPainter;

namespace LabelCreator
{

/**
 * Draws a LabelDocument. All drawing happens in twips on the label canvas;
 * paint() expects the painter to be set up that way, renderImage() does it
 * for a given resolution.
 */
class LabelRenderer
{
public:
    enum class Mode {
        /** Antialiased, with the paper outline; for the screen. */
        Preview,
        /** No antialiasing and no outline; what goes to a thermal printer. */
        Print,
    };

    explicit LabelRenderer(Mode mode = Mode::Preview);

    Mode mode() const;

    /**
     * Resolution of the printer the label is meant for, 300 dpi by default.
     * Bar widths are whole printer dots, so a preview shows what will print.
     */
    qreal printerResolution() const;
    void setPrinterResolution(qreal dpi);

    /** The moment date and time objects show; invalid means now. */
    void setDateTime(const QDateTime &when);
    /** Which label of a print run this is: counters advance by this many steps. */
    void setCounterStep(int step);
    /**
     * In Print mode, also draws the label's outline (its die-cut shape, such
     * as rounded corners) as a thin grey line, with nothing outside it: a
     * proof of the label as it will look. For exports, never for printing.
     */
    void setOutline(bool outline);

    /**
     * @p object alone, on a transparent picture of its box at @p dpi: what a
     * program that cannot draw it is given instead.
     */
    QImage objectPicture(const LabelObject &object, qreal dpi);
    /** The outline of a polygon, star or arrow filling @p box. */
    static QPainterPath shapePath(const ShapeObject &object, const QRectF &box);

    /**
     * @p document with text that grows to fit grown, and the objects
     * anchored to it moved along, as it is painted; as it is without either.
     */
    LabelDocument grown(const LabelDocument &document) const;

    /** Paints the label with the painter's coordinates in twips. */
    void paint(QPainter *painter, const LabelDocument &document);

    /** Renders the whole label at @p dpi. */
    QImage renderImage(const LabelDocument &document, qreal dpi);

    /**
     * Writes the label as a one-page PDF of its own size, as on the canvas,
     * in vectors (text stays text). False if the file cannot be written.
     */
    bool writePdf(const LabelDocument &document, const QString &fileName, const QString &title = {});

    /** Problems found while drawing the last label, such as invalid barcode data. */
    QStringList warnings() const;

    /**
     * How long @p object wants to be in a tape cell that gives it @p across
     * twips the other way: along the canvas's x axis if @p horizontal, else
     * along y. Text as its fit mode makes it, images and 2D barcodes in
     * proportion, 1D barcodes at their bar widths.
     */
    qreal naturalLength(const LabelObject &object, qreal across, bool horizontal = true) const;

private:
    void paintObject(QPainter *painter, const LabelObject &object);
    void paintText(QPainter *painter, const TextObject &object, const QRectF &box);
    void paintBarcode(QPainter *painter, const BarcodeObject &object, const QRectF &box);
    void paintImage(QPainter *painter, const ImageObject &object, const QRectF &box);
    void paintImageBorder(QPainter *painter, const ImageObject &object, const QRectF &box);
    void paintShape(QPainter *painter, const ShapeObject &object, const QRectF &box);
    void paintCircularText(QPainter *painter, const CircularTextObject &object, const QRectF &box);
    void paintPlaceholder(QPainter *painter, const QRectF &box, const QString &text);
    QFont printFont(QFont font) const;

    Mode m_mode;
    qreal m_printerResolution = 300;
    QDateTime m_dateTime;
    int m_counterStep = 0;
    bool m_outline = false; // a proof, for exports
    QStringList m_warnings;
};

} // namespace LabelCreator
