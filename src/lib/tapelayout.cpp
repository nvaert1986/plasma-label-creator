/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "tapelayout.h"

#include "labelrenderer.h"
#include "units.h"

#include <QRegularExpression>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace TapeLayout
{

namespace
{

// From DYMO's LabelManager PPDs (lm450.ppd): each tape's page width and the
// part of it the print head reaches (*ImageableArea of the Continuous
// sizes), in points, and the tape code the driver expects.
const QList<Tape> &table()
{
    static const QList<Tape> tapes = {
        {6, 18, 2.8, 14.8, 0},
        {9, 26, 2.8, 22.8, 1},
        {12, 35, 2.8, 31.6, 2},
        {19, 55, 7.2, 47.2, 3},
        {24, 68, 8.4, 59.6, 4},
    };
    return tapes;
}

// Length used for cells whose content has no natural length (an empty cell).
constexpr qreal EmptyCellLength = 720; // twips, 1/2 inch

// The printable band across the tape, from and to, in canvas twips along
// the canvas axis across the tape (y for landscape labels, x for portrait).
std::pair<qreal, qreal> band(const LabelDocument &document)
{
    const auto millimetres = millimetresOf(document.id).value_or(millimetresOf(document.paperName).value_or(12));
    const Tape tape = TapeLayout::tape(millimetres).value_or(*TapeLayout::tape(12));
    const qreal width = document.tape->width;
    // In paper terms from the tape's edge, scaled to the width the document has.
    const qreal scale = width / Units::pointsToTwips(tape.width);
    const qreal from = Units::pointsToTwips(tape.printFrom) * scale;
    const qreal to = Units::pointsToTwips(tape.printTo) * scale;
    // Landscape labels are turned: paper x is canvas y counted from the far edge.
    return document.orientation == PaperOrientation::Landscape ? std::pair(width - to, width - from) : std::pair(from, to);
}

class Layout
{
public:
    explicit Layout(LabelDocument &document)
        : m_document(document)
        , m_alongX(document.orientation == PaperOrientation::Landscape)
    {
    }

    // The length @p cell wants along @p direction (canvas x or y), given the
    // room it has across that direction.
    qreal natural(const TapeCell &cell, bool horizontal, qreal across) const
    {
        if (cell.lengthMode == CellLengthMode::Fixed) {
            return cell.length;
        }
        if (!cell.subcells.isEmpty()) {
            const bool childrenHorizontal = cell.subcellsOrientation == Qt::Horizontal;
            if (childrenHorizontal == horizontal) {
                qreal sum = 0;
                for (const TapeCell &child : cell.subcells) {
                    sum += child.lengthMode == CellLengthMode::Star ? child.length : natural(child, horizontal, across);
                }
                return sum;
            }
            // Stacked the other way: as long as the longest, each getting a share across.
            const qreal share = across / cell.subcells.size();
            qreal longest = 0;
            for (const TapeCell &child : cell.subcells) {
                longest = std::max(longest, natural(child, horizontal, share));
            }
            return longest;
        }
        if (cell.object < 0 || cell.object >= m_document.objects.size()) {
            return cell.length > 0 ? cell.length : EmptyCellLength;
        }
        const QMarginsF margin = cell.objectMargin;
        const qreal marginAlong = horizontal ? margin.left() + margin.right() : margin.top() + margin.bottom();
        const qreal marginAcross = horizontal ? margin.top() + margin.bottom() : margin.left() + margin.right();
        return m_renderer.naturalLength(m_document.objects.at(cell.object), std::max<qreal>(1, across - marginAcross), horizontal) + marginAlong;
    }

    // Puts @p cell in @p rect, and its children and object inside it.
    void place(TapeCell &cell, const QRectF &rect)
    {
        cell.bounds = rect;
        if (cell.object >= 0 && cell.object < m_document.objects.size()) {
            objectBase(m_document.objects[cell.object]).bounds = rect.marginsRemoved(cell.objectMargin);
        }
        if (cell.subcells.isEmpty()) {
            return;
        }
        const bool horizontal = cell.subcellsOrientation == Qt::Horizontal;
        const qreal room = horizontal ? rect.width() : rect.height();
        const qreal across = horizontal ? rect.height() : rect.width();

        // Fixed and Auto first; Star cells share what is left. Across the
        // tape there is no length to grow into: Auto cells share the band.
        const bool alongTape = horizontal == m_alongX;
        const auto shares = [alongTape](const TapeCell &child) {
            return child.lengthMode == CellLengthMode::Star || (!alongTape && child.lengthMode == CellLengthMode::Auto);
        };
        QList<qreal> lengths;
        qreal used = 0;
        int stars = 0;
        for (const TapeCell &child : std::as_const(cell.subcells)) {
            if (shares(child)) {
                lengths << 0;
                ++stars;
            } else {
                lengths << natural(child, horizontal, across);
                used += lengths.constLast();
            }
        }
        const qreal share = stars > 0 ? std::max<qreal>(0, room - used) / stars : 0;
        qreal position = horizontal ? rect.left() : rect.top();
        for (qsizetype i = 0; i < cell.subcells.size(); ++i) {
            TapeCell &child = cell.subcells[i];
            const qreal length = shares(child) ? share : lengths.at(i);
            if (child.lengthMode != CellLengthMode::Fixed) {
                child.length = length; // saved as DLS does, laid out
            }
            place(child, horizontal ? QRectF(position, rect.top(), length, rect.height()) : QRectF(rect.left(), position, rect.width(), length));
            position += length;
        }
    }

private:
    LabelDocument &m_document;
    bool m_alongX; // along the tape is the canvas's x axis (landscape labels)
    LabelRenderer m_renderer{LabelRenderer::Mode::Preview};
};

} // namespace

std::optional<Tape> tape(int millimetres)
{
    for (const Tape &candidate : table()) {
        if (candidate.millimetres == millimetres) {
            return candidate;
        }
    }
    return std::nullopt;
}

QList<Tape> tapes()
{
    return table();
}

std::optional<int> millimetresOf(const QString &idOrPaperName)
{
    static const QRegularExpression width(u"(\\d+)\\s*mm"_s, QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = width.match(idOrPaperName);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    return std::nullopt;
}

LabelDocument newLabel(int millimetres)
{
    const Tape tape = TapeLayout::tape(millimetres).value_or(*TapeLayout::tape(12));
    LabelDocument document;
    document.orientation = PaperOrientation::Landscape;
    document.id = u"Tape%1mm"_s.arg(tape.millimetres);
    document.paperName = u"%1mm"_s.arg(tape.millimetres);
    TapeLabel label;
    label.width = Units::pointsToTwips(tape.width);
    label.lengthMode = CellLengthMode::Auto;
    label.root.subcellsOrientation = Qt::Horizontal;
    document.tape = label;
    layout(document);
    return document;
}

QRectF printableBand(const LabelDocument &document)
{
    if (!document.tape) {
        return {};
    }
    const auto [from, to] = band(document);
    const qreal length = document.tape->length;
    return document.orientation == PaperOrientation::Landscape ? QRectF(0, from, length, to - from) : QRectF(from, 0, to - from, length);
}

void layout(LabelDocument &document)
{
    if (!document.tape) {
        return;
    }
    TapeLabel &tape = *document.tape;
    // Along the tape is the canvas's x axis on landscape labels, y on portrait ones.
    const bool alongX = document.orientation == PaperOrientation::Landscape;
    const auto [from, to] = band(document);
    Layout engine(document);

    // How long the label is: its content, unless fixed.
    if (tape.lengthMode != CellLengthMode::Fixed) {
        TapeCell root = tape.root;
        root.lengthMode = CellLengthMode::Auto;
        tape.length = std::max<qreal>(EmptyCellLength, engine.natural(root, alongX, to - from));
    }
    tape.root.length = tape.length;
    engine.place(tape.root, printableBand(document));

    // The paper as it feeds, for drawing the tape's outline.
    document.paperOutline = QPainterPath();
    document.paperOutline.addRect(QRectF(QPointF(0, 0), document.paperSize()));
}

} // namespace TapeLayout

} // namespace LabelCreator
