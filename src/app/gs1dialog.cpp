/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gs1dialog.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

enum Column {
    Identifier,
    Value,
    Check,
};

// "01" from "01  GTIN", or from "01" typed in.
QString aiOf(const QComboBox *combo)
{
    return combo->currentText().section(u' ', 0, 0).trimmed();
}

// With its check digit added when only that is missing.
Gs1::Element completed(Gs1::Element element)
{
    const auto id = Gs1::identifier(element.ai);
    const QString &value = element.value;
    const bool digits = !value.isEmpty() && std::all_of(value.cbegin(), value.cend(), [](QChar c) {
        return c.isDigit();
    });
    if (id && id->checkDigit && digits && value.size() == id->maximumLength - 1) {
        element.value += Gs1::checkDigit(value);
    }
    return element;
}

} // namespace

Gs1Dialog::Gs1Dialog(const QString &data, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Build GS1 Data"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18n("Choose application identifiers and give their values. A check digit left out is added; "
                                  "a value may use fields, such as {Batch}, which are checked when printing."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({i18n("Application identifier"), i18n("Value"), QString()});
    m_table->horizontalHeader()->setSectionResizeMode(Identifier, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(Value, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(Check, QHeaderView::Stretch);
    m_table->setColumnWidth(Identifier, 280);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QIcon::fromTheme(u"list-add"_s), i18n("Add"), this);
    auto *remove = new QPushButton(QIcon::fromTheme(u"list-remove"_s), i18n("Remove"), this);
    connect(add, &QPushButton::clicked, this, [this]() {
        // GTIN first, then batch, expiry and serial, as labels usually go.
        static const QStringList usual = {u"01"_s, u"10"_s, u"17"_s, u"21"_s};
        QString next = u"10"_s;
        for (const QString &ai : usual) {
            const QList<Gs1::Element> present = elements();
            if (std::none_of(present.cbegin(), present.cend(), [&ai](const Gs1::Element &e) {
                    return e.ai == ai;
                })) {
                next = ai;
                break;
            }
        }
        addElement({next, QString()});
        m_table->setCurrentCell(m_table->rowCount() - 1, Value);
        if (QWidget *editor = m_table->cellWidget(m_table->rowCount() - 1, Value)) {
            editor->setFocus();
        }
    });
    connect(remove, &QPushButton::clicked, this, &Gs1Dialog::removeElement);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_result = new QLabel(this);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setWordWrap(true);
    layout->addWidget(m_result);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = box->button(QDialogButtonBox::Ok);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);

    const auto parsed = Gs1::parse(data);
    for (const Gs1::Element &element : parsed.value_or(QList<Gs1::Element>())) {
        addElement(element);
    }
    if (m_table->rowCount() == 0) {
        addElement({u"01"_s, QString()});
    }
    // Data typed without brackets is not lost: it shows, to rebuild.
    if (!parsed && !data.trimmed().isEmpty()) {
        m_result->setText(i18n("The data was not written with [AI] brackets: %1", data.toHtmlEscaped()));
    }
    update();
    resize(760, 360);
}

void Gs1Dialog::addElement(const Gs1::Element &element)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *ai = new QComboBox(m_table);
    ai->setEditable(true); // AIs this program does not list can be typed
    for (const Gs1::Identifier &id : Gs1::identifiers()) {
        ai->addItem(id.ai + u"  "_s + id.title, id.ai);
    }
    const int known = ai->findData(element.ai);
    if (known >= 0) {
        ai->setCurrentIndex(known);
    } else {
        ai->setCurrentText(element.ai);
    }
    auto *value = new QLineEdit(element.value, m_table);
    connect(ai, &QComboBox::currentTextChanged, this, &Gs1Dialog::update);
    connect(value, &QLineEdit::textChanged, this, &Gs1Dialog::update);
    m_table->setCellWidget(row, Identifier, ai);
    m_table->setCellWidget(row, Value, value);
    auto *check = new QTableWidgetItem;
    check->setFlags(Qt::ItemIsEnabled);
    m_table->setItem(row, Check, check);
    update();
}

void Gs1Dialog::removeElement()
{
    if (m_table->currentRow() >= 0) {
        m_table->removeRow(m_table->currentRow());
        update();
    }
}

QList<Gs1::Element> Gs1Dialog::elements() const
{
    QList<Gs1::Element> elements;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *ai = qobject_cast<QComboBox *>(m_table->cellWidget(row, Identifier));
        const auto *value = qobject_cast<QLineEdit *>(m_table->cellWidget(row, Value));
        if (ai && value) {
            elements.append(completed({aiOf(ai), value->text().trimmed()}));
        }
    }
    return elements;
}

QString Gs1Dialog::data() const
{
    return Gs1::compose(elements());
}

void Gs1Dialog::update()
{
    bool right = m_table->rowCount() > 0;
    const QList<Gs1::Element> all = elements();
    for (int row = 0; row < all.size(); ++row) {
        QTableWidgetItem *check = m_table->item(row, Check);
        if (!check) {
            continue;
        }
        const auto problem = Gs1::problem(all.at(row));
        const auto id = Gs1::identifier(all.at(row).ai);
        if (problem) {
            check->setText(*problem);
            check->setIcon(QIcon::fromTheme(u"dialog-error"_s));
            right = false;
        } else {
            const auto *typed = qobject_cast<QLineEdit *>(m_table->cellWidget(row, Value));
            const bool added = typed && typed->text().trimmed() != all.at(row).value;
            check->setText(added ? i18n("%1; check digit %2 added", id->format(), all.at(row).value.back())
                                 : (id ? id->format() : i18n("Not listed: only its characters are checked")));
            check->setIcon(QIcon::fromTheme(u"dialog-ok"_s));
        }
        check->setToolTip(check->text());
    }
    m_ok->setEnabled(right);
    if (right) {
        m_result->setText(i18n("Data: %1", data().toHtmlEscaped()));
    }
}
