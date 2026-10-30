/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rolllayoutdialog.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

RollLayoutDialog::RollLayoutDialog(const RollLayout &roll, qreal widthMm, qreal lengthMm, bool printerTakesIt, QWidget *parent)
    : QDialog(parent)
    , m_widthMm(widthMm)
{
    setWindowTitle(i18n("Roll Layout"));
    auto *form = new QFormLayout(this);
    auto *intro = new QLabel(i18n("Rolls with several labels side by side print a row at a time, with the next labels (data rows, copies, "
                                  "counters going on) next to each other. A continuous roll has no gaps: each label takes the length "
                                  "set here."),
                             this);
    intro->setWordWrap(true);
    form->addRow(intro);
    if (!printerTakesIt) {
        auto *warning = new QLabel(i18n("The chosen printer prints one die-cut label at a time. Labels across and continuous rolls need a "
                                        "printer that takes any label size, such as an Altec ATP or a Zebra."),
                                   this);
        warning->setWordWrap(true);
        warning->setStyleSheet(u"color: palette(highlight)"_s);
        form->addRow(warning);
    }

    m_across = new QSpinBox(this);
    m_across->setRange(1, 10);
    m_across->setValue(std::max(1, roll.across));
    m_gap = new QDoubleSpinBox(this);
    m_gap->setRange(0, 50);
    m_gap->setDecimals(1);
    m_gap->setSingleStep(0.5);
    m_gap->setSuffix(i18nc("millimetres", " mm"));
    m_gap->setValue(roll.gapMm);
    m_gap->setToolTip(i18n("From the right edge of one label to the left edge of the next, on the roll"));
    m_continuous = new QCheckBox(i18n("Continuous roll, without gaps between labels"), this);
    m_continuous->setChecked(roll.continuous);
    m_length = new QDoubleSpinBox(this);
    m_length->setRange(5, 1000);
    m_length->setDecimals(1);
    m_length->setSuffix(i18nc("millimetres", " mm"));
    m_length->setValue(lengthMm);
    m_summary = new QLabel(this);
    m_summary->setEnabled(false);
    m_summary->setWordWrap(true);
    form->addRow(i18n("Labels across:"), m_across);
    form->addRow(i18n("Gap between them:"), m_gap);
    form->addRow(QString(), m_continuous);
    form->addRow(i18n("Label length:"), m_length);
    form->addRow(m_summary);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);

    connect(m_across, &QSpinBox::valueChanged, this, &RollLayoutDialog::update);
    connect(m_gap, &QDoubleSpinBox::valueChanged, this, &RollLayoutDialog::update);
    connect(m_continuous, &QCheckBox::toggled, this, &RollLayoutDialog::update);
    connect(m_length, &QDoubleSpinBox::valueChanged, this, &RollLayoutDialog::update);
    update();
}

void RollLayoutDialog::update()
{
    m_gap->setEnabled(m_across->value() > 1);
    m_length->setEnabled(m_continuous->isChecked());
    const qreal width = m_across->value() * m_widthMm + (m_across->value() - 1) * m_gap->value();
    m_summary->setText(i18n("Printed %1 mm wide and %2 mm long per row.", QLocale().toString(width, 'f', 1), QLocale().toString(m_length->value(), 'f', 1)));
}

RollLayout RollLayoutDialog::roll() const
{
    RollLayout roll;
    roll.across = m_across->value();
    roll.gapMm = roll.across > 1 ? m_gap->value() : 0;
    roll.continuous = m_continuous->isChecked();
    return roll;
}

qreal RollLayoutDialog::lengthMm() const
{
    return m_length->value();
}
