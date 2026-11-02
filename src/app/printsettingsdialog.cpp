/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printsettingsdialog.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

PrintSettingsDialog::PrintSettingsDialog(const LabelDocument &document, const QString &labelFile, QWidget *parent)
    : QDialog(parent)
    , m_labelFile(labelFile)
{
    setWindowTitle(i18n("Print Settings"));
    auto *layout = new QVBoxLayout(this);

    // How the printer prints this label: each printer uses what it has.
    auto *printer = new QGroupBox(i18n("How the Printer Prints This Label"));
    auto *printerForm = new QFormLayout(printer);
    m_density = new QComboBox;
    m_density->addItem(i18n("The printer's setting"), QString());
    m_density->addItem(i18nc("print density", "Light"), u"Light"_s);
    m_density->addItem(i18nc("print density", "Medium"), u"Medium"_s);
    m_density->addItem(i18nc("print density", "Normal"), u"Normal"_s);
    m_density->addItem(i18nc("print density", "Dark"), u"Dark"_s);
    m_density->setCurrentIndex(std::max(0, m_density->findData(document.printing.density)));
    m_darkness = new QSpinBox;
    m_darkness->setRange(0, 30);
    m_darkness->setSpecialValueText(i18n("The printer's setting"));
    m_darkness->setValue(document.printing.darkness);
    m_darkness->setToolTip(i18n("1 is the lightest, 30 the darkest. Darker for thick barcodes and some ribbons, lighter when print bleeds."));
    m_speed = new QSpinBox;
    m_speed->setRange(0, 12);
    m_speed->setSpecialValueText(i18n("The printer's setting"));
    m_speed->setSuffix(i18nc("inches per second", " in/s"));
    m_speed->setValue(document.printing.speed);
    m_speed->setToolTip(i18n("Slower prints sharper, for small barcodes and text."));
    printerForm->addRow(i18n("DYMO print density:"), m_density);
    printerForm->addRow(i18n("Darkness (Altec, Zebra):"), m_darkness);
    printerForm->addRow(i18n("Speed (Altec, Zebra):"), m_speed);
    layout->addWidget(printer);

    // Printed once around the labels of a run.
    auto *run = new QGroupBox(i18n("Around a Print Run"));
    auto *runForm = new QFormLayout(run);
    m_header = labelChooser(document.headerLabel);
    m_trailer = labelChooser(document.trailerLabel);
    runForm->addRow(i18n("Header label:"), m_header->parentWidget());
    runForm->addRow(i18n("Trailer label:"), m_trailer->parentWidget());
    auto *note = new QLabel(i18n("Printed once before or after the labels, filled with the answers and {Count}, the number of labels in between. "
                                 "They must be for the same label type. A label next to this one is named without its folder."));
    note->setWordWrap(true);
    note->setEnabled(false);
    runForm->addRow(note);
    layout->addWidget(run);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(560, sizeHint().height());
}

QLineEdit *PrintSettingsDialog::labelChooser(const QString &name)
{
    auto *row = new QWidget(this);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    auto *edit = new QLineEdit(name, row);
    edit->setPlaceholderText(i18n("None"));
    edit->setClearButtonEnabled(true);
    auto *browse = new QPushButton(QIcon::fromTheme(u"document-open"_s), i18n("Browse…"), row);
    connect(browse, &QPushButton::clicked, this, [this, edit]() {
        const QString folder = m_labelFile.isEmpty() ? QDir::homePath() : QFileInfo(m_labelFile).absolutePath();
        const QString chosen = QFileDialog::getOpenFileName(this, i18n("Choose a Label"), folder, i18n("Label files (*.label)"));
        if (!chosen.isEmpty()) {
            edit->setText(named(chosen));
        }
    });
    rowLayout->addWidget(edit, 1);
    rowLayout->addWidget(browse);
    return edit;
}

QString PrintSettingsDialog::named(const QString &fileName) const
{
    // Next to this label: its name only, so the two can move together.
    if (!m_labelFile.isEmpty() && QFileInfo(fileName).absolutePath() == QFileInfo(m_labelFile).absolutePath()) {
        return QFileInfo(fileName).fileName();
    }
    return fileName;
}

PrintSettings PrintSettingsDialog::printing() const
{
    PrintSettings printing;
    printing.density = m_density->currentData().toString();
    printing.darkness = m_darkness->value();
    printing.speed = m_speed->value();
    return printing;
}

QString PrintSettingsDialog::headerLabel() const
{
    return m_header->text().trimmed();
}

QString PrintSettingsDialog::trailerLabel() const
{
    return m_trailer->text().trimmed();
}
