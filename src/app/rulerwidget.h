/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

class LabelView;

/**
 * A ruler in centimetres with millimetre ticks along the canvas, as in DLS.
 * It follows the canvas's zoom and scrolling, shades the label and marks
 * the selected object.
 */
class RulerWidget : public QWidget
{
    Q_OBJECT

public:
    RulerWidget(Qt::Orientation orientation, LabelView *view, QWidget *parent = nullptr);

    static constexpr int Thickness = 22;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Qt::Orientation m_orientation;
    LabelView *m_view;
};
