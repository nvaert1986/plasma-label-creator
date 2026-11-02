/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "answersform.h"

#include "fields.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLocale>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// A date answer as the question writes it: its format, else the short one.
QString writeDate(const PrintQuestion &question, const QDate &date)
{
    return question.dateFormat.isEmpty() ? QLocale().toString(date, QLocale::ShortFormat) : QLocale().toString(date, question.dateFormat);
}

QDate readDate(const PrintQuestion &question, const QString &text)
{
    return question.dateFormat.isEmpty() ? QLocale().toDate(text, QLocale::ShortFormat) : QLocale().toDate(text, question.dateFormat);
}

QString checkedText(const PrintQuestion &question)
{
    return question.checkedText.isEmpty() ? i18nc("answer of a ticked check box", "Yes") : question.checkedText;
}

QString uncheckedText(const PrintQuestion &question)
{
    return question.uncheckedText.isEmpty() ? i18nc("answer of a check box not ticked", "No") : question.uncheckedText;
}

} // namespace

AnswersForm::AnswersForm(const QList<PrintQuestion> &questions, const QHash<QString, QString> &previous, const std::optional<DataTable> &data, QWidget *parent)
    : QWidget(parent)
    , m_questions(questions)
    , m_data(data)
{
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < questions.size(); ++i) {
        const PrintQuestion &question = questions.at(i);
        const QString start = previous.value(question.name, question.defaultValue);
        const QString label = (question.prompt.isEmpty() ? question.name : question.prompt) + (question.required ? u" *"_s : QString());
        QWidget *field = nullptr;
        switch (question.kind) {
        case PrintQuestion::Kind::Text: {
            auto *edit = new QLineEdit(start, this);
            if (question.maximumLength > 0) {
                edit->setMaxLength(question.maximumLength);
            }
            connect(edit, &QLineEdit::textChanged, this, &AnswersForm::changed);
            if (!question.lookupColumn.isEmpty()) {
                edit->setToolTip(i18n("Looked up in the column %1 of the data; the row found fills the other fields.", question.lookupColumn));
                connect(edit, &QLineEdit::editingFinished, this, [this, i]() {
                    lookUp(i);
                });
            }
            field = edit;
            break;
        }
        case PrintQuestion::Kind::Choice: {
            auto *box = new QComboBox(this);
            if (!question.required) {
                box->addItem(QString());
            }
            QStringList choices = question.choices;
            if (!question.choicesColumn.isEmpty() && m_data) {
                const qsizetype column = m_data->columns.indexOf(question.choicesColumn);
                for (const QStringList &row : std::as_const(m_data->rows)) {
                    const QString value = row.value(column);
                    if (column >= 0 && !value.isEmpty() && !choices.contains(value)) {
                        choices << value;
                    }
                }
            }
            box->addItems(choices);
            box->setCurrentIndex(std::max(0, box->findText(start)));
            connect(box, &QComboBox::currentIndexChanged, this, &AnswersForm::changed);
            field = box;
            break;
        }
        case PrintQuestion::Kind::Number: {
            auto *spin = new QDoubleSpinBox(this);
            spin->setDecimals(question.decimals);
            const bool limited = question.minimum != 0 || question.maximum != 0;
            spin->setRange(limited ? question.minimum : -1e9, limited ? question.maximum : 1e9);
            spin->setValue(QLocale().toDouble(start));
            connect(spin, &QDoubleSpinBox::valueChanged, this, &AnswersForm::changed);
            field = spin;
            break;
        }
        case PrintQuestion::Kind::Date: {
            auto *date = new QDateEdit(this);
            date->setCalendarPopup(true);
            date->setDisplayFormat(question.dateFormat.isEmpty() ? QLocale().dateFormat(QLocale::ShortFormat) : question.dateFormat);
            const QDate given = readDate(question, start);
            date->setDate(given.isValid() ? given : QDate::currentDate()); // today unless set
            connect(date, &QDateEdit::dateChanged, this, &AnswersForm::changed);
            field = date;
            break;
        }
        case PrintQuestion::Kind::Check: {
            auto *check = new QCheckBox(this);
            check->setChecked(start == checkedText(question));
            connect(check, &QCheckBox::toggled, this, &AnswersForm::changed);
            field = check;
            break;
        }
        }
        form->addRow(label + u':', field);
        m_fields << field;
    }
}

QString AnswersForm::answer(int index) const
{
    const PrintQuestion &question = m_questions.at(index);
    QWidget *field = m_fields.at(index);
    switch (question.kind) {
    case PrintQuestion::Kind::Text:
        return static_cast<QLineEdit *>(field)->text();
    case PrintQuestion::Kind::Choice:
        return static_cast<QComboBox *>(field)->currentText();
    case PrintQuestion::Kind::Number:
        return QLocale().toString(static_cast<QDoubleSpinBox *>(field)->value(), 'f', question.decimals);
    case PrintQuestion::Kind::Date:
        return writeDate(question, static_cast<QDateEdit *>(field)->date());
    case PrintQuestion::Kind::Check:
        return static_cast<QCheckBox *>(field)->isChecked() ? checkedText(question) : uncheckedText(question);
    }
    return {};
}

QHash<QString, QString> AnswersForm::answers() const
{
    QHash<QString, QString> answers;
    for (int i = 0; i < m_questions.size(); ++i) {
        answers.insert(m_questions.at(i).name, answer(i));
    }
    return answers;
}

std::optional<QString> AnswersForm::problem() const
{
    for (int i = 0; i < m_questions.size(); ++i) {
        if (const auto problem = Fields::problem(m_questions.at(i), answer(i))) {
            return problem;
        }
    }
    return std::nullopt;
}

void AnswersForm::setAnswer(const QString &name, const QString &value)
{
    for (int i = 0; i < m_questions.size(); ++i) {
        if (m_questions.at(i).name != name) {
            continue;
        }
        const PrintQuestion &question = m_questions.at(i);
        QWidget *field = m_fields.at(i);
        switch (question.kind) {
        case PrintQuestion::Kind::Text:
            static_cast<QLineEdit *>(field)->setText(value);
            break;
        case PrintQuestion::Kind::Choice: {
            auto *box = static_cast<QComboBox *>(field);
            if (box->findText(value) < 0) {
                box->addItem(value); // from the data: offered as well
            }
            box->setCurrentIndex(box->findText(value));
            break;
        }
        case PrintQuestion::Kind::Number:
            static_cast<QDoubleSpinBox *>(field)->setValue(QLocale().toDouble(value));
            break;
        case PrintQuestion::Kind::Date: {
            const QDate date = readDate(question, value);
            if (date.isValid()) {
                static_cast<QDateEdit *>(field)->setDate(date);
            }
            break;
        }
        case PrintQuestion::Kind::Check:
            static_cast<QCheckBox *>(field)->setChecked(value == checkedText(question));
            break;
        }
    }
}

void AnswersForm::lookUp(int index)
{
    const PrintQuestion &question = m_questions.at(index);
    if (!m_data || question.lookupColumn.isEmpty()) {
        return;
    }
    const qsizetype column = m_data->columns.indexOf(question.lookupColumn);
    const QString key = answer(index).trimmed();
    if (column < 0 || key.isEmpty()) {
        return;
    }
    for (qsizetype row = 0; row < m_data->rows.size(); ++row) {
        if (m_data->rows.at(row).value(column).trimmed().compare(key, Qt::CaseInsensitive) != 0) {
            continue;
        }
        // The other questions named after a column get its value.
        const QHash<QString, QString> values = m_data->values(row);
        for (const PrintQuestion &other : std::as_const(m_questions)) {
            if (other.name != question.name && values.contains(other.name)) {
                setAnswer(other.name, values.value(other.name));
            }
        }
        return;
    }
}
