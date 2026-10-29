/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rulerwidget.h"

#include "labelview.h"
#include "units.h"

#include <QEvent>
#include <QPainter>

#include <cmath>

using namespace LabelCreator;

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

    // Ticks every millimetre from the label's corner, numbered per centimetre.
    const qreal pixelsPerMm = m_view->scale() * Units::mmToTwips(1);
    if (pixelsPerMm <= 0) {
        return;
    }
    const int step = pixelsPerMm < 2.5 ? 5 : 1; // too dense to tell apart otherwise
    QFont font = painter.font();
    font.setPixelSize(9);
    painter.setFont(font);
    painter.setPen(pal.color(QPalette::WindowText));
    const int first = int(std::floor(-start / pixelsPerMm));
    const int last = int(std::ceil((length - start) / pixelsPerMm));
    for (int mm = first - first % step; mm <= last; mm += step) {
        const qreal position = start + mm * pixelsPerMm;
        const int tick = mm % 10 == 0 ? Thickness - 2 : (mm % 5 == 0 ? Thickness / 2 : Thickness / 4);
        if (horizontal) {
            painter.drawLine(QPointF(position, height() - tick), QPointF(position, height()));
        } else {
            painter.drawLine(QPointF(width() - tick, position), QPointF(width(), position));
        }
        if (mm % 10 == 0) {
            const QString number = QString::number(mm / 10);
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
