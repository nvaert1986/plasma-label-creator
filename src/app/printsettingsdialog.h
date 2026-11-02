/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

/**
 * Layout > Print Settings: how the printer prints this label (DYMO's
 * density, a ZPL printer's darkness and speed) and the header and trailer
 * labels printed once around a run.
 */
class PrintSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /** For the label in @p labelFile (empty when not saved): other labels next to it are named without a folder. */
    PrintSettingsDialog(const LabelCreator::LabelDocument &document, const QString &labelFile, QWidget *parent = nullptr);

    LabelCreator::PrintSettings printing() const;
    QString headerLabel() const;
    QString trailerLabel() const;

private:
    QLineEdit *labelChooser(const QString &name);
    QString named(const QString &fileName) const;

    QString m_labelFile;
    QComboBox *m_density;
    QSpinBox *m_darkness;
    QSpinBox *m_speed;
    QLineEdit *m_header;
    QLineEdit *m_trailer;
};
