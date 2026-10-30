/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layersdialog.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

enum Column {
    Name,
    Visible,
    Printed,
    Locked,
};

constexpr int OriginalNameRole = Qt::UserRole;

QTableWidgetItem *tick(bool checked)
{
    auto *item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

} // namespace

LayersDialog::LayersDialog(const QList<Layer> &layers, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Layers"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18n("Objects on a layer show, print and can be edited as the layer says. Objects not on a layer are on the label "
                                  "itself, which always shows and prints. A layer that does not print suits a picture of the product to design on."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({i18n("Layer"), i18n("Show"), i18n("Print"), i18n("Locked")});
    m_table->horizontalHeader()->setSectionResizeMode(Name, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);
    for (const Layer &layer : layers) {
        addRow(layer, layer.name);
        m_originalNames << layer.name;
    }

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QIcon::fromTheme(u"list-add"_s), i18n("Add Layer"), this);
    auto *remove = new QPushButton(QIcon::fromTheme(u"list-remove"_s), i18n("Remove Layer"), this);
    remove->setToolTip(i18n("Its objects move to the label itself"));
    connect(add, &QPushButton::clicked, this, &LayersDialog::addLayer);
    connect(remove, &QPushButton::clicked, this, &LayersDialog::removeLayer);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);
    resize(480, 320);
}

void LayersDialog::addRow(const Layer &layer, const QString &originalName)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *name = new QTableWidgetItem(layer.name);
    name->setData(OriginalNameRole, originalName);
    m_table->setItem(row, Name, name);
    m_table->setItem(row, Visible, tick(layer.visible));
    m_table->setItem(row, Printed, tick(layer.printed));
    m_table->setItem(row, Locked, tick(layer.locked));
}

void LayersDialog::addLayer()
{
    // A name of its own: Layer 1, Layer 2...
    QStringList names;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        names << m_table->item(row, Name)->text();
    }
    int number = 1;
    while (names.contains(i18n("Layer %1", number))) {
        ++number;
    }
    Layer layer;
    layer.name = i18n("Layer %1", number);
    addRow(layer, QString());
    m_table->setCurrentCell(m_table->rowCount() - 1, Name);
    m_table->editItem(m_table->item(m_table->rowCount() - 1, Name));
}

void LayersDialog::removeLayer()
{
    if (m_table->currentRow() >= 0) {
        m_table->removeRow(m_table->currentRow());
    }
}

QList<Layer> LayersDialog::layers() const
{
    // Names are what objects refer to: blank and repeated ones are left out.
    QList<Layer> layers;
    QStringList names;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        Layer layer;
        layer.name = m_table->item(row, Name)->text().trimmed();
        if (layer.name.isEmpty() || names.contains(layer.name)) {
            continue;
        }
        layer.visible = m_table->item(row, Visible)->checkState() == Qt::Checked;
        layer.printed = m_table->item(row, Printed)->checkState() == Qt::Checked;
        layer.locked = m_table->item(row, Locked)->checkState() == Qt::Checked;
        names << layer.name;
        layers << layer;
    }
    return layers;
}

QHash<QString, QString> LayersDialog::renamed() const
{
    const QList<Layer> kept = layers();
    QHash<QString, QString> names;
    for (const QString &original : m_originalNames) {
        names.insert(original, QString());
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString original = m_table->item(row, Name)->data(OriginalNameRole).toString();
        const QString now = m_table->item(row, Name)->text().trimmed();
        const bool stays = std::any_of(kept.cbegin(), kept.cend(), [&now](const Layer &layer) {
            return layer.name == now;
        });
        if (!original.isEmpty() && stays) {
            names.insert(original, now);
        }
    }
    return names;
}
