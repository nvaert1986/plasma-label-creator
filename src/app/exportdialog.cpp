/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "exportdialog.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QCheckBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

using namespace Qt::Literals::StringLiterals;

ExportDialog::ExportDialog(const QString &fileName, bool outline, QWidget *parent)
    : QDialog(parent)
    , m_folder(QFileInfo(fileName).absolutePath())
{
    setWindowTitle(i18n("Export Label"));
    auto *form = new QFormLayout(this);
    m_file = new QLineEdit(fileName, this);
    auto *browse = new QPushButton(QIcon::fromTheme(u"document-save-as"_s), i18n("Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &ExportDialog::browse);
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(browse);
    form->addRow(i18n("File:"), fileRow);

    m_pdf = new QRadioButton(i18n("PDF: vectors, the label's own size"), this);
    m_png = new QRadioButton(i18n("PNG: the printer's dots, at its resolution"), this);
    (fileName.endsWith(u".png"_s, Qt::CaseInsensitive) ? m_png : m_pdf)->setChecked(true);
    connect(m_pdf, &QRadioButton::toggled, this, &ExportDialog::followFormat);
    form->addRow(i18n("As:"), m_pdf);
    form->addRow(QString(), m_png);

    m_outline = new QCheckBox(i18n("Show the label's outline"), this);
    m_outline->setChecked(outline);
    m_outline->setToolTip(i18n("Draws the label's shape, such as rounded corners, as a thin grey line, with nothing outside it: "
                               "a proof of how the label will look. Leave it off for printing the file elsewhere."));
    form->addRow(QString(), m_outline);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_export = buttons->addButton(i18n("Export"), QDialogButtonBox::AcceptRole);
    m_export->setIcon(QIcon::fromTheme(u"document-export"_s));
    m_export->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &ExportDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_file, &QLineEdit::textEdited, this, &ExportDialog::followFileName);
    connect(m_file, &QLineEdit::textChanged, this, [this]() {
        m_export->setEnabled(!m_file->text().trimmed().isEmpty());
    });
    m_export->setEnabled(!fileName.trimmed().isEmpty());
    form->addRow(buttons);
    resize(560, sizeHint().height());
}

QString ExportDialog::fileName() const
{
    QString name = m_file->text().trimmed();
    if (name == u"~"_s || name.startsWith(u"~/"_s)) {
        name = QDir::homePath() + name.mid(1);
    }
    name = QDir(m_folder).absoluteFilePath(name);
    // "Kabel 2.5" has no extension yet, whatever QFileInfo says.
    const QString suffix = QFileInfo(name).suffix().toLower();
    if (suffix != u"pdf"_s && suffix != u"png"_s) {
        name += png() ? u".png"_s : u".pdf"_s;
    }
    return name;
}

bool ExportDialog::png() const
{
    return m_png->isChecked();
}

bool ExportDialog::outline() const
{
    return m_outline->isChecked();
}

void ExportDialog::browse()
{
    const QString filter = png() ? i18n("PNG images (*.png)") : i18n("PDF documents (*.pdf)");
    // Asking about overwriting is done once, on Export.
    const QString chosen = QFileDialog::getSaveFileName(this, i18n("Export Label"), fileName(), filter, nullptr, QFileDialog::DontConfirmOverwrite);
    if (!chosen.isEmpty()) {
        m_file->setText(chosen);
        (chosen.endsWith(u".png"_s, Qt::CaseInsensitive) ? m_png : m_pdf)->setChecked(true);
    }
}

void ExportDialog::followFormat()
{
    const QFileInfo file(m_file->text().trimmed());
    const QString suffix = file.suffix().toLower();
    if (suffix == u"pdf"_s || suffix == u"png"_s) {
        const QString name = m_file->text().trimmed();
        m_file->setText(name.left(name.size() - suffix.size()) + (png() ? u"png"_s : u"pdf"_s));
    }
}

void ExportDialog::followFileName()
{
    const QString suffix = QFileInfo(m_file->text().trimmed()).suffix().toLower();
    if (suffix == u"pdf"_s || suffix == u"png"_s) {
        // The name is right already: no rewriting it while it is typed.
        const QSignalBlocker blocker(m_pdf);
        (suffix == u"png"_s ? m_png : m_pdf)->setChecked(true);
    }
}

void ExportDialog::accept()
{
    if (QFileInfo(fileName()).isDir()) {
        KMessageBox::error(this, i18n("%1 is a folder. Type a name for the file.", fileName()), i18n("Export Label"));
        return;
    }
    if (QFileInfo::exists(fileName())
        && KMessageBox::warningContinueCancel(this,
                                              i18n("%1 already exists. Replace it?", QFileInfo(fileName()).fileName()),
                                              i18n("Export Label"),
                                              KStandardGuiItem::overwrite())
            != KMessageBox::Continue) {
        return;
    }
    QDialog::accept();
}
