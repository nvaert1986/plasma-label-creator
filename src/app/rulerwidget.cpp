/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rulerwidget.h"

#include "labelview.h"
#include "units.h"

#include <KLocalizedString>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

using namespace LabelCreator;

namespace
{
bool imperialRulers = false;
}

void RulerWidget::setInches(bool inches)
{
    imperialRulers = inches;
}

bool RulerWidget::inches()
{
    return imperialRulers;
}

RulerWidget::RulerWidget(Qt::Orientation orientation, LabelView *view, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
    , m_view(view)
{
    // Repaint whenever the canvas does: zooming, scrolling, editing.
    m_view->installEventFilter(this);
    if (orientation == Qt::Horizontal) {
        setFixedHeight(Thickness);
    } else {
        setFixedWidth(Thickness);
    }
    setToolTip(i18n("Drag onto the label for a guideline; drag a guideline back here to remove it"));
}

void RulerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_view->beginNewGuide(m_orientation);
        m_view->dragGuideTo(event->globalPosition());
    }
}

void RulerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        m_view->dragGuideTo(event->globalPosition());
    }
}

void RulerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_view->endGuideAt(event->globalPosition());
    }
}

QSize RulerWidget::sizeHint() const
{
    return QSize(Thickness, Thickness);
}

bool RulerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view && (event->type() == QEvent::Paint || event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        update();
    }
    return QWidget::eventFilter(watched, event);
}

void RulerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const bool horizontal = m_orientation == Qt::Horizontal;
    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Window));

    // The canvas's coordinates in ours, along the ruler.
    const QPoint offset = m_view->mapTo(window(), QPoint(0, 0)) - mapTo(window(), QPoint(0, 0));
    const auto along = [&](const QPointF &point) {
        return horizontal ? point.x() + offset.x() : point.y() + offset.y();
    };
    const QRectF label = m_view->labelRect();
    const qreal start = along(label.topLeft());
    const qreal end = along(label.bottomRight());
    const qreal length = horizontal ? width() : height();

    // The label lighter, the selected object highlighted.
    const auto band = [&](qreal from, qreal to, const QColor &color) {
        painter.fillRect(horizontal ? QRectF(from, 0, to - from, height()) : QRectF(0, from, width(), to - from), color);
    };
    band(start, end, pal.color(QPalette::Base));
    const QRectF selection = m_view->selectionRect();
    if (!selection.isEmpty()) {
        QColor highlight = pal.color(QPalette::Highlight);
        highlight.setAlpha(70);
        band(along(selection.topLeft()), along(selection.bottomRight()), highlight);
    }

    // Ticks from the label's corner: every millimetre, numbered per
    // centimetre, or every eighth of an inch, numbered per inch.
    const bool imperial = inches();
    const qreal pixelsPerTick = m_view->scale() * (imperial ? Units::TwipsPerInch / 8 : Units::mmToTwips(1));
    if (pixelsPerTick <= 0) {
        return;
    }
    const int perNumber = imperial ? 8 : 10;
    // Too dense to tell apart: every fifth millimetre, or every half inch.
    const int step = pixelsPerTick >= 2.5 ? 1 : (imperial ? 4 : 5);
    QFont font = painter.font();
    font.setPixelSize(9);
    painter.setFont(font);
    painter.setPen(pal.color(QPalette::WindowText));
    const int first = int(std::floor(-start / pixelsPerTick));
    const int last = int(std::ceil((length - start) / pixelsPerTick));
    for (int tick = first - first % step; tick <= last; tick += step) {
        const qreal position = start + tick * pixelsPerTick;
        const bool numbered = tick % perNumber == 0;
        const bool middle = tick % (perNumber / 2) == 0;
        const bool quarter = imperial && tick % 2 == 0;
        const int size = numbered ? Thickness - 2 : (middle ? Thickness / 2 : (quarter ? Thickness / 3 : Thickness / 4));
        if (horizontal) {
            painter.drawLine(QPointF(position, height() - size), QPointF(position, height()));
        } else {
            painter.drawLine(QPointF(width() - size, position), QPointF(width(), position));
        }
        if (numbered) {
            const QString number = QString::number(tick / perNumber);
            if (horizontal) {
                painter.drawText(QPointF(position + 2, 10), number);
            } else {
                painter.drawText(QPointF(2, position + 10), number);
            }
        }
    }
    painter.setPen(pal.color(QPalette::Mid));
    if (horizontal) {
        painter.drawLine(0, height() - 1, width(), height() - 1);
    } else {
        painter.drawLine(width() - 1, 0, width() - 1, height());
    }
}
