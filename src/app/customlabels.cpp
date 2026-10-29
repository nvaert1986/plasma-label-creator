/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "customlabels.h"

#include "units.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// One group per size, named after it: width and length in millimetres, and
// the printable area if only part of the label takes print (across, along,
// width, length, in millimetres, in the printer's directions).
KConfigGroup settings()
{
    return KSharedConfig::openConfig()->group(u"Custom Label Sizes"_s);
}

QSizeF millimetres(const PaperSize &paper)
{
    return paper.size * Units::MmPerInch / 72.0;
}

} // namespace

namespace CustomLabels
{

QList<PaperSize> all()
{
    QList<PaperSize> list;
    const KConfigGroup group = settings();
    for (const QString &name : group.groupList()) {
        const KConfigGroup entry = group.group(name);
        const QSizeF mm(entry.readEntry("Width", 0.0), entry.readEntry("Length", 0.0));
        if (mm.isEmpty()) {
            continue;
        }
        PaperSize paper = PaperSize::custom(name, mm);
        paper.group = i18n("My label sizes");
        const QList<qreal> area = entry.readEntry("PrintArea", QList<qreal>());
        if (area.size() == 4) {
            paper.printArea = QRectF(area.at(0), area.at(1), area.at(2), area.at(3)).intersected(QRectF(QPointF(0, 0), mm));
            paper.printArea = QRectF(paper.printArea.topLeft() * 72.0 / Units::MmPerInch, paper.printArea.size() * 72.0 / Units::MmPerInch);
        }
        list << paper;
    }
    return list;
}

void add(const PaperSize &paper)
{
    KConfigGroup entry = settings().group(paper.name);
    const QSizeF mm = millimetres(paper);
    entry.writeEntry("Width", mm.width());
    entry.writeEntry("Length", mm.height());
    if (paper.printArea.isEmpty()) {
        entry.deleteEntry("PrintArea");
    } else {
        const QRectF area(paper.printArea.topLeft() * Units::MmPerInch / 72.0, paper.printArea.size() * Units::MmPerInch / 72.0);
        entry.writeEntry("PrintArea", QList<qreal>{area.x(), area.y(), area.width(), area.height()});
    }
    entry.sync();
}

void remove(const QString &name)
{
    KConfigGroup group = settings();
    group.deleteGroup(name);
    group.sync();
}

std::optional<PaperSize> find(const QString &name)
{
    for (const PaperSize &paper : all()) {
        if (paper.name == name) {
            return paper;
        }
    }
    return std::nullopt;
}

std::optional<PaperSize> create(QWidget *parent, const std::optional<PaperSize> &existing)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? i18n("Edit Label Size") : i18n("Custom Label Size"));
    auto *form = new QFormLayout(&dialog);
    auto *intro = new QLabel(i18n("For labels that are in no list. Measure one label without the gap to the next one."));
    intro->setWordWrap(true);
    form->addRow(intro);
    auto *name = new QLineEdit(existing ? existing->name : QString());
    name->setPlaceholderText(i18n("For example: Cable wrap label 41 × 41.5 mm"));
    const auto millimetreBox = [](qreal value, qreal minimum = 3) {
        auto *box = new QDoubleSpinBox;
        box->setRange(minimum, 1000);
        box->setDecimals(1);
        box->setSingleStep(0.5);
        box->setSuffix(i18nc("millimetres", " mm"));
        box->setValue(value);
        return box;
    };
    const QSizeF size = existing ? millimetres(*existing) : QSizeF(50, 25);
    auto *width = millimetreBox(size.width());
    auto *length = millimetreBox(size.height());
    form->addRow(i18n("Name:"), name);
    form->addRow(i18n("Width, across the roll:"), width);
    form->addRow(i18n("Length, along the roll:"), length);

    // The printable area, as the label is laid out on screen: turned
    // (landscape) when it is longer than wide, as new labels are.
    auto *partly = new QCheckBox(i18n("Only part of the label takes print (a wrap label's strip, say)"));
    auto *left = millimetreBox(0, 0);
    auto *top = millimetreBox(0, 0);
    auto *areaWidth = millimetreBox(10, 1);
    auto *areaHeight = millimetreBox(10, 1);
    const auto orientation = [&]() {
        return length->value() > width->value() ? PaperOrientation::Landscape : PaperOrientation::Portrait;
    };
    if (existing && !existing->printArea.isEmpty()) {
        partly->setChecked(true);
        const QRectF paperMm(existing->printArea.topLeft() * Units::MmPerInch / 72.0, existing->printArea.size() * Units::MmPerInch / 72.0);
        const QRectF shown = LabelPrinter::canvasRect(orientation(), size, paperMm);
        left->setValue(shown.x());
        top->setValue(shown.y());
        areaWidth->setValue(shown.width());
        areaHeight->setValue(shown.height());
    }
    auto *area = new QWidget;
    auto *areaForm = new QFormLayout(area);
    areaForm->setContentsMargins(0, 0, 0, 0);
    auto *areaHint = new QLabel(i18n("As the label shows on screen: from its top-left corner."));
    areaHint->setEnabled(false);
    areaForm->addRow(areaHint);
    areaForm->addRow(i18n("From the left:"), left);
    areaForm->addRow(i18n("From the top:"), top);
    areaForm->addRow(i18n("Width:"), areaWidth);
    areaForm->addRow(i18n("Height:"), areaHeight);
    area->setEnabled(partly->isChecked());
    QObject::connect(partly, &QCheckBox::toggled, area, &QWidget::setEnabled);
    form->addRow(partly);
    form->addRow(area);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    const auto updateOk = [ok, name]() {
        ok->setEnabled(!name->text().trimmed().isEmpty());
    };
    QObject::connect(name, &QLineEdit::textChanged, &dialog, updateOk);
    updateOk();
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const QSizeF mm(width->value(), length->value());
    PaperSize paper = PaperSize::custom(name->text().trimmed(), mm);
    paper.group = i18n("My label sizes");
    if (partly->isChecked()) {
        const QRectF shown(left->value(), top->value(), areaWidth->value(), areaHeight->value());
        const QRectF paperMm = LabelPrinter::paperRect(orientation(), mm, shown).intersected(QRectF(QPointF(0, 0), mm));
        paper.printArea = QRectF(paperMm.topLeft() * 72.0 / Units::MmPerInch, paperMm.size() * 72.0 / Units::MmPerInch);
    }
    if (existing && existing->name != paper.name) {
        remove(existing->name);
    }
    add(paper);
    return paper;
}

} // namespace CustomLabels
