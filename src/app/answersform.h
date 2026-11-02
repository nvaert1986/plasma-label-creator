/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "datareader.h"
#include "labeldocument.h"

#include <QHash>
#include <QWidget>

#include <optional>

/**
 * The form for a label's print-time questions: a field per question, of its
 * kind (text, drop-down, number, date, check box), with their rules. A text
 * question can look its answer up in the data source; the row found fills
 * the other questions named after its columns.
 */
class AnswersForm : public QWidget
{
    Q_OBJECT

public:
    /**
     * @p previous: answers given before, shown in place of the defaults;
     * @p data: the label's data source, for drop-downs and lookups.
     */
    AnswersForm(const QList<LabelCreator::PrintQuestion> &questions,
                const QHash<QString, QString> &previous,
                const std::optional<DataTable> &data,
                QWidget *parent = nullptr);

    /** The answers, by question name, written as each question says. */
    QHash<QString, QString> answers() const;
    /** What keeps the answers from printing; nullopt when they will do. */
    std::optional<QString> problem() const;
    /** Fills question @p name's field with @p value, as typing it would. */
    void setAnswer(const QString &name, const QString &value);

Q_SIGNALS:
    void changed();

private:
    QString answer(int index) const;
    void lookUp(int index);

    QList<LabelCreator::PrintQuestion> m_questions;
    QList<QWidget *> m_fields;
    std::optional<DataTable> m_data;
};
