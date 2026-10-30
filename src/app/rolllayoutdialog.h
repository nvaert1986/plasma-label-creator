/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

/**
 * How the label sits on its roll: several labels across, the gap between
 * them, or a continuous roll with the label's length. For printers that
 * take any label size, such as Altec and Zebra (ZPL) printers.
 */
class RollLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * For a label @p widthMm across the print head and @p lengthMm along the
     * feed; @p printerTakesIt says whether the chosen printer can print it.
     */
    RollLayoutDialog(const LabelCreator::RollLayout &roll, qreal widthMm, qreal lengthMm, bool printerTakesIt, QWidget *parent = nullptr);

    LabelCreator::RollLayout roll() const;
    /** The label's length along the feed; changed only for a continuous roll. */
    qreal lengthMm() const;

private:
    void update();

    qreal m_widthMm;
    QSpinBox *m_across;
    QDoubleSpinBox *m_gap;
    QCheckBox *m_continuous;
    QDoubleSpinBox *m_length;
    QLabel *m_summary;
};
