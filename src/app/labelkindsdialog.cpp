/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelkindsdialog.h"

#include "labelkinds.h"
#include "labelrenderer.h"
#include "tapelayout.h"
#include "units.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <memory>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QDoubleSpinBox *millimetres(qreal value, qreal minimum, qreal maximum)
{
    auto *box = new QDoubleSpinBox;
    box->setRange(minimum, maximum);
    box->setDecimals(2);
    box->setSingleStep(0.5);
    box->setSuffix(i18nc("millimetres", " mm"));
    box->setValue(value);
    return box;
}

QSpinBox *number(int value, int minimum, int maximum)
{
    auto *box = new QSpinBox;
    box->setRange(minimum, maximum);
    box->setValue(value);
    return box;
}

// The font every kind asks for: family, size and bold.
struct FontFields {
    QFontComboBox *family = new QFontComboBox;
    QDoubleSpinBox *size = new QDoubleSpinBox;
    QCheckBox *bold = new QCheckBox(i18n("Bold"));

    explicit FontFields(qreal points)
    {
        family->setCurrentFont(QFont(u"Arial"_s));
        size->setRange(4, 72);
        size->setDecimals(1);
        size->setSuffix(i18nc("points", " pt"));
        size->setValue(points);
    }

    void addTo(QFormLayout *form) const
    {
        auto *row = new QHBoxLayout;
        row->addWidget(family, 1);
        row->addWidget(size);
        row->addWidget(bold);
        form->addRow(i18n("Font:"), row);
    }

    LabelFont font() const
    {
        LabelFont font;
        font.family = family->currentFont().family();
        font.size = size->value();
        font.bold = bold->isChecked();
        return font;
    }
};

} // namespace

LabelKindsDialog::LabelKindsDialog(const LabelDocument &base, QWidget *parent)
    : QDialog(parent)
    , m_base(base)
{
    setWindowTitle(i18n("Label of a Kind"));
    m_kinds = new QListWidget;
    m_kinds->setIconSize(QSize(22, 22));
    m_kinds->setMinimumWidth(220);
    m_forms = new QStackedWidget;
    m_preview = new QLabel;
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(110);
    m_preview->setBackgroundRole(QPalette::Mid);
    m_preview->setAutoFillBackground(true);
    m_size = new QLabel;
    m_size->setEnabled(false);

    // Everything the forms ask for re-draws the preview.
    const auto watch = [this](QWidget *form) {
        for (auto *box : form->findChildren<QAbstractSpinBox *>()) {
            if (auto *spin = qobject_cast<QSpinBox *>(box)) {
                connect(spin, &QSpinBox::valueChanged, this, &LabelKindsDialog::updatePreview);
            } else if (auto *real = qobject_cast<QDoubleSpinBox *>(box)) {
                connect(real, &QDoubleSpinBox::valueChanged, this, &LabelKindsDialog::updatePreview);
            }
        }
        for (auto *edit : form->findChildren<QLineEdit *>()) {
            connect(edit, &QLineEdit::textChanged, this, &LabelKindsDialog::updatePreview);
        }
        for (auto *check : form->findChildren<QCheckBox *>()) {
            connect(check, &QCheckBox::toggled, this, &LabelKindsDialog::updatePreview);
        }
        for (auto *combo : form->findChildren<QComboBox *>()) {
            connect(combo, &QComboBox::currentIndexChanged, this, &LabelKindsDialog::updatePreview);
        }
    };

    // Cable wrap and flag.
    {
        auto *form = new QWidget;
        auto *layout = new QFormLayout(form);
        auto *text = new QLineEdit(u"CABLE-01"_s);
        auto *diameter = millimetres(6, 1, 100);
        auto font = std::make_shared<FontFields>(10);
        layout->addRow(i18n("Text:"), text);
        layout->addRow(i18n("Cable diameter:"), diameter);
        font->addTo(layout);
        watch(form);
        addKind(u"draw-circle"_s, i18n("Cable Wrap"), i18n("Around a cable, readable from every side"), form, [this, text, diameter, font]() {
            return LabelKinds::cableWrap(m_base, {text->text(), diameter->value(), font->font()});
        });
    }
    {
        auto *form = new QWidget;
        auto *layout = new QFormLayout(form);
        auto *text = new QLineEdit(u"CABLE-01"_s);
        auto *diameter = millimetres(6, 1, 100);
        auto *flag = millimetres(25, 5, 200);
        auto font = std::make_shared<FontFields>(12);
        layout->addRow(i18n("Text:"), text);
        layout->addRow(i18n("Cable diameter:"), diameter);
        layout->addRow(i18n("Flag length:"), flag);
        font->addTo(layout);
        watch(form);
        addKind(u"flag"_s, i18n("Cable Flag"), i18n("Folded around a cable, the text on both sides"), form, [this, text, diameter, flag, font]() {
            return LabelKinds::cableFlag(m_base, {text->text(), diameter->value(), flag->value(), font->font()});
        });
    }

    // Ports: patch panels, terminal blocks and breakers share a form.
    const auto addPorts = [&](const QString &icon, const QString &name, const QString &description, int count, qreal pitch, const QString &prefix) {
        auto *form = new QWidget;
        auto *layout = new QFormLayout(form);
        auto *countBox = number(count, 1, 200);
        auto *pitchBox = millimetres(pitch, 2, 200);
        auto *first = number(1, -9999, 9999);
        auto *step = number(1, -100, 100);
        auto *prefixEdit = new QLineEdit(prefix);
        auto *digits = number(0, 0, 6);
        digits->setSpecialValueText(i18n("No leading zeros"));
        auto *reversed = new QCheckBox(i18n("Number from the far end"));
        auto *separators = new QCheckBox(i18n("Lines between the cells"));
        separators->setChecked(true);
        auto *arrow = new QComboBox;
        arrow->addItem(i18n("None"), int(LabelKinds::Arrow::None));
        arrow->addItem(i18n("▲ Up, for a label under the ports"), int(LabelKinds::Arrow::Up));
        arrow->addItem(i18n("▼ Down, for a label above the ports"), int(LabelKinds::Arrow::Down));
        auto *lead = millimetres(0, 0, 200);
        auto *tail = millimetres(0, 0, 200);
        auto font = std::make_shared<FontFields>(10);
        layout->addRow(i18n("Number of cells:"), countBox);
        layout->addRow(i18n("Cell width (pitch):"), pitchBox);
        auto *numbering = new QHBoxLayout;
        numbering->addWidget(prefixEdit);
        numbering->addWidget(first);
        numbering->addWidget(new QLabel(i18n("step")));
        numbering->addWidget(step);
        layout->addRow(i18n("Numbers (prefix, first):"), numbering);
        layout->addRow(i18n("Digits:"), digits);
        layout->addRow(QString(), reversed);
        layout->addRow(QString(), separators);
        layout->addRow(i18n("Arrows:"), arrow);
        layout->addRow(i18n("Before the first cell:"), lead);
        layout->addRow(i18n("After the last cell:"), tail);
        font->addTo(layout);
        watch(form);
        addKind(icon, name, description, form, [=, this]() {
            LabelKinds::Ports ports;
            ports.count = countBox->value();
            ports.pitchMm = pitchBox->value();
            ports.first = first->value();
            ports.step = step->value();
            ports.prefix = prefixEdit->text();
            ports.digits = digits->value();
            ports.reversed = reversed->isChecked();
            ports.separators = separators->isChecked();
            ports.arrow = LabelKinds::Arrow(arrow->currentData().toInt());
            ports.leadMm = lead->value();
            ports.tailMm = tail->value();
            ports.font = font->font();
            return LabelKinds::ports(m_base, ports);
        });
    };
    addPorts(u"network-wired"_s, i18n("Patch Panel"), i18n("A row of numbered ports"), 24, 16.5, QString());
    addPorts(u"network-connect"_s, i18n("Terminal Block"), i18n("Terminals at their pitch on a DIN rail"), 12, 6.2, QString());
    addPorts(u"battery"_s, i18n("Breaker Panel"), i18n("Breakers of one module (18 mm) each"), 12, 18, u"F"_s);

    // Rack markers.
    {
        auto *form = new QWidget;
        auto *layout = new QFormLayout(form);
        auto *first = number(1, 0, 99);
        auto *last = number(8, 0, 99);
        auto *unit = millimetres(44.45, 5, 100);
        auto *prefix = new QLineEdit(u"U"_s);
        auto *descending = new QCheckBox(i18n("Highest unit first"));
        auto *boxes = new QCheckBox(i18n("A box around every unit"));
        boxes->setChecked(true);
        auto font = std::make_shared<FontFields>(12);
        auto *units = new QHBoxLayout;
        units->addWidget(first);
        units->addWidget(new QLabel(i18nc("from unit ... to unit", "to")));
        units->addWidget(last);
        layout->addRow(i18n("Units:"), units);
        layout->addRow(i18n("Unit height:"), unit);
        layout->addRow(i18n("Prefix:"), prefix);
        layout->addRow(QString(), descending);
        layout->addRow(QString(), boxes);
        font->addTo(layout);
        auto *hint = new QLabel(i18n("A LabelManager prints up to about 1.4 m in one label: 31 units. Make longer racks in parts."));
        hint->setWordWrap(true);
        hint->setEnabled(false);
        layout->addRow(hint);
        watch(form);
        addKind(u"view-list-details"_s, i18n("Rack Markers"), i18n("Height units along a rack's rail, boxed"), form, [=, this]() {
            LabelKinds::RackMarkers rack;
            rack.firstUnit = first->value();
            rack.lastUnit = last->value();
            rack.unitMm = unit->value();
            rack.prefix = prefix->text();
            rack.descending = descending->isChecked();
            rack.boxes = boxes->isChecked();
            rack.font = font->font();
            return LabelKinds::rackMarkers(m_base, rack);
        });
    }

    auto *top = new QHBoxLayout;
    top->addWidget(m_kinds);
    top->addWidget(m_forms, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top, 1);
    layout->addWidget(m_preview);
    layout->addWidget(m_size);
    layout->addWidget(buttons);

    connect(m_kinds, &QListWidget::currentRowChanged, m_forms, &QStackedWidget::setCurrentIndex);
    connect(m_kinds, &QListWidget::currentRowChanged, this, &LabelKindsDialog::updatePreview);
    m_kinds->setCurrentRow(0);
}

void LabelKindsDialog::addKind(const QString &icon, const QString &name, const QString &description, QWidget *form, std::function<LabelDocument()> make)
{
    auto *item = new QListWidgetItem(QIcon::fromTheme(icon), name, m_kinds);
    item->setToolTip(description);
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *title = new QLabel(u"<b>%1</b><br>%2"_s.arg(name.toHtmlEscaped(), description.toHtmlEscaped()));
    title->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(form);
    layout->addStretch();
    m_forms->addWidget(page);
    m_makers << std::move(make);
}

LabelDocument LabelKindsDialog::label() const
{
    const int row = m_kinds->currentRow();
    LabelDocument document = row >= 0 && row < m_makers.size() ? m_makers.at(row)() : m_base;
    TapeLayout::layout(document);
    return document;
}

void LabelKindsDialog::updatePreview()
{
    const LabelDocument document = label();
    // At a resolution that makes the label as wide as the preview, or as tall.
    const QSizeF canvas = document.canvasSize();
    if (canvas.isEmpty()) {
        return;
    }
    const qreal width = std::max(200, m_preview->width() - 20);
    const qreal height = std::max(80, m_preview->height() - 20);
    const qreal dpi = std::min(width / (canvas.width() / Units::TwipsPerInch), height / (canvas.height() / Units::TwipsPerInch));
    LabelRenderer renderer(LabelRenderer::Mode::Preview);
    m_preview->setPixmap(QPixmap::fromImage(renderer.renderImage(document, std::min<qreal>(dpi, 600))));
    m_size->setText(document.tape ? i18n("On %1 tape, %2 mm long", document.paperName, QString::number(Units::twipsToMm(canvas.width()), 'f', 1))
                                  : i18n("On the label in use, %1 (%2 × %3 mm)",
                                         document.paperName,
                                         QString::number(Units::twipsToMm(canvas.width()), 'f', 1),
                                         QString::number(Units::twipsToMm(canvas.height()), 'f', 1)));
}
