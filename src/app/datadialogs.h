/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "datareader.h"
#include "labeldocument.h"

#include <QDialog>
#include <QHash>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

/**
 * Chooses where the rows come from (a file or a database query), shows them,
 * and lets the user tick which to print.
 */
class DataSourceDialog : public QDialog
{
    Q_OBJECT

public:
    DataSourceDialog(const LabelCreator::DataSource &source, const std::optional<DataTable> &table, const QList<int> &selectedRows, QWidget *parent = nullptr);

    LabelCreator::DataSource source() const;
    /** The rows as last loaded; nullopt if nothing loaded. */
    std::optional<DataTable> table() const;
    /** The ticked rows, by index into table(). */
    QList<int> selectedRows() const;

private:
    void chooseFile();
    void updateSheets();
    void load();
    void showTable();
    void setAllChecked(bool checked);

    QComboBox *m_kind;
    QStackedWidget *m_pages;
    QLineEdit *m_file;
    QComboBox *m_sheet;
    QComboBox *m_delimiter;
    QCheckBox *m_firstRowNames;
    QComboBox *m_driver;
    QLineEdit *m_database;
    QLineEdit *m_host;
    QSpinBox *m_port;
    QLineEdit *m_user;
    QLineEdit *m_password;
    QPlainTextEdit *m_query;
    QLabel *m_status;
    QTableWidget *m_rows;
    std::optional<DataTable> m_table;
    QList<int> m_initialSelection;
    bool m_tickAll = false; // after a new load, every row is ticked
};

/** Edits the questions asked when printing, each with its input rules. */
class QuestionsDialog : public QDialog
{
    Q_OBJECT

public:
    QuestionsDialog(const QList<LabelCreator::PrintQuestion> &questions, QWidget *parent = nullptr);

    QList<LabelCreator::PrintQuestion> questions() const;

private:
    void showQuestion(int row);
    void storeQuestion();
    void addQuestion();
    void removeQuestion();

    QList<LabelCreator::PrintQuestion> m_questions;
    int m_current = -1;
    QListWidget *m_list;
    QWidget *m_editor;
    QLineEdit *m_name;
    QLineEdit *m_prompt;
    QLineEdit *m_default;
    QCheckBox *m_required;
    QSpinBox *m_minimum;
    QSpinBox *m_maximum;
    QComboBox *m_allowed;
    QLineEdit *m_pattern;
};

/** Asks the questions when printing; OK only once every answer passes its rules. */
class AnswersDialog : public QDialog
{
    Q_OBJECT

public:
    /** @p previous: answers given before, offered again in place of the defaults. */
    AnswersDialog(const QList<LabelCreator::PrintQuestion> &questions, const QHash<QString, QString> &previous, QWidget *parent = nullptr);

    /** The answers, by question name. */
    QHash<QString, QString> answers() const;

private:
    void check();

    QList<LabelCreator::PrintQuestion> m_questions;
    QList<QLineEdit *> m_fields;
    QLabel *m_problem;
    QPushButton *m_ok;
};

/** Asks for a database's password, which is never kept. */
QString askDatabasePassword(const LabelCreator::DataSource &source, QWidget *parent, bool *ok);
