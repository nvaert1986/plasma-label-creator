/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QImage>

class QCheckBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSlider;

/**
 * Shows the pages a print sends, dot for dot: what the printer gets, in
 * black and white; a landscape label turned back to look as on the canvas,
 * or as it feeds. Steps through the labels of
 * a print run, where counters and addresses differ.
 */
class PrintPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    /** @p pages at @p dpi, for @p printerName; @p notes are the renderer's warnings. */
    /**
     * @p turned: the label lies across the feed on the canvas (a landscape
     * label), so the pages are shown turned back to look as there, unless
     * asked to show them as they feed.
     */
    PrintPreviewDialog(const QList<QImage> &pages,
                       qreal dpi,
                       const QString &printerName,
                       const QStringList &notes,
                       QWidget *parent = nullptr,
                       bool turned = false);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void showPage(int index);
    void updateZoom();

    QList<QImage> m_pages;
    qreal m_dpi;
    int m_index = 0;
    QLabel *m_image = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_position = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_next = nullptr;
    QSlider *m_zoom = nullptr;
    QLabel *m_zoomText = nullptr;
    QCheckBox *m_asFed = nullptr; // nullptr when the label is not turned
};
