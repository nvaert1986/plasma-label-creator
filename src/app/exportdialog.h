/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

/**
 * File > Export: where to, as a PDF (vectors, the label's size) or a PNG
 * (the printer's dots), and whether to show the label's outline, as a proof.
 */
class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * Starts at @p fileName; @p outline ticked as the user had it. A name
     * typed without a folder goes in @p fileName's folder.
     */
    ExportDialog(const QString &fileName, bool outline, QWidget *parent = nullptr);

    /** The file to write: absolute, with the format's extension unless it has it. */
    QString fileName() const;
    bool png() const;
    bool outline() const;

private:
    void browse();
    /** The file name's extension follows the format chosen. */
    void followFormat();
    /** The format follows an extension typed. */
    void followFileName();
    void accept() override;

    QLineEdit *m_file;
    QRadioButton *m_pdf;
    QRadioButton *m_png;
    QCheckBox *m_outline;
    QPushButton *m_export;
    QString m_folder;
};
