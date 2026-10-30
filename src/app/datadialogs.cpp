/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "datadialogs.h"

#include "fields.h"

#include <KLocalizedString>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// The Qt SQL drivers that are installed, with what they connect to.
QList<std::pair<QString, QString>> databaseDrivers()
{
    const QHash<QString, QString> names = {
        {u"QSQLITE"_s, i18n("SQLite file")},
        {u"QODBC"_s, i18n("ODBC (SQL Server, Access, and others)")},
        {u"QPSQL"_s, i18n("PostgreSQL")},
        {u"QMYSQL"_s, i18n("MySQL / MariaDB")},
        {u"QMARIADB"_s, i18n("MariaDB")},
        {u"QIBASE"_s, i18n("Firebird / InterBase")},
        {u"QOCI"_s, i18n("Oracle")},
    };
    QList<std::pair<QString, QString>> drivers;
    for (const QString &driver : QSqlDatabase::drivers()) {
        drivers.append({driver, names.value(driver, driver)});
    }
    return drivers;
}

bool isSpreadsheet(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return suffix == u"xlsx"_s || suffix == u"xlsm"_s || suffix == u"ods"_s;
}

} // namespace

DataSourceDialog::DataSourceDialog(const DataSource &source, const std::optional<DataTable> &table, const QList<int> &selectedRows, QWidget *parent)
    : QDialog(parent)
    , m_table(table)
    , m_initialSelection(selectedRows)
{
    setWindowTitle(i18n("Data Source"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(
        i18n("Print a label for each row. Put fields on the label as <b>{Column}</b>, in texts, addresses, barcodes or an image's file path; "
             "each label gets that row's values."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_kind = new QComboBox(this);
    m_kind->addItem(i18n("A file: CSV, Excel or OpenDocument"), int(DataSource::Kind::File));
    m_kind->addItem(i18n("A database query"), int(DataSource::Kind::Database));
    auto *kindForm = new QFormLayout;
    kindForm->addRow(i18n("Rows from:"), m_kind);
    layout->addLayout(kindForm);

    // A file.
    auto *filePage = new QWidget(this);
    auto *fileForm = new QFormLayout(filePage);
    fileForm->setContentsMargins(0, 0, 0, 0);
    m_file = new QLineEdit(source.fileName, filePage);
    auto *browse = new QPushButton(QIcon::fromTheme(u"document-open"_s), i18n("Choose…"), filePage);
    connect(browse, &QPushButton::clicked, this, &DataSourceDialog::chooseFile);
    connect(m_file, &QLineEdit::editingFinished, this, &DataSourceDialog::updateSheets);
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(browse);
    fileForm->addRow(i18n("File:"), fileRow);
    m_sheet = new QComboBox(filePage);
    fileForm->addRow(i18n("Sheet:"), m_sheet);
    m_delimiter = new QComboBox(filePage);
    m_delimiter->addItem(i18n("Detect"), QString());
    m_delimiter->addItem(i18n("Semicolon ( ; )"), u";"_s);
    m_delimiter->addItem(i18n("Comma ( , )"), u","_s);
    m_delimiter->addItem(i18n("Tab"), u"\t"_s);
    m_delimiter->addItem(i18n("Bar ( | )"), u"|"_s);
    m_delimiter->setCurrentIndex(std::max(0, m_delimiter->findData(source.delimiter.isNull() ? QString() : QString(source.delimiter))));
    fileForm->addRow(i18n("CSV separator:"), m_delimiter);
    m_firstRowNames = new QCheckBox(i18n("The first row names the columns"), filePage);
    m_firstRowNames->setChecked(source.firstRowNames);
    fileForm->addRow(QString(), m_firstRowNames);

    // A database.
    auto *databasePage = new QWidget(this);
    auto *databaseForm = new QFormLayout(databasePage);
    databaseForm->setContentsMargins(0, 0, 0, 0);
    m_driver = new QComboBox(databasePage);
    for (const auto &[driver, name] : databaseDrivers()) {
        m_driver->addItem(name, driver);
    }
    m_driver->setCurrentIndex(std::max(0, m_driver->findData(source.driver)));
    databaseForm->addRow(i18n("Database type:"), m_driver);
    m_database = new QLineEdit(source.database, databasePage);
    m_database->setPlaceholderText(i18n("Database name, SQLite file, or ODBC data source or connection string"));
    databaseForm->addRow(i18n("Database:"), m_database);
    m_host = new QLineEdit(source.host, databasePage);
    m_port = new QSpinBox(databasePage);
    m_port->setRange(0, 65535);
    m_port->setSpecialValueText(i18n("Default"));
    m_port->setValue(source.port);
    auto *hostRow = new QHBoxLayout;
    hostRow->addWidget(m_host, 1);
    hostRow->addWidget(new QLabel(i18n("Port:"), databasePage));
    hostRow->addWidget(m_port);
    databaseForm->addRow(i18n("Server:"), hostRow);
    m_user = new QLineEdit(source.user, databasePage);
    databaseForm->addRow(i18n("User:"), m_user);
    m_password = new QLineEdit(databasePage);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(i18n("Not saved: asked again when printing"));
    databaseForm->addRow(i18n("Password:"), m_password);
    m_query = new QPlainTextEdit(source.query, databasePage);
    m_query->setPlaceholderText(u"SELECT Name, Location, Serial FROM Assets WHERE Active = 1"_s);
    m_query->setMaximumHeight(90);
    databaseForm->addRow(i18n("Query:"), m_query);
    auto *readOnly = new QLabel(i18n("Only a query that reads is run (SELECT or WITH ... SELECT), so printing a label never changes the database."), databasePage);
    readOnly->setWordWrap(true);
    databaseForm->addRow(QString(), readOnly);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(filePage);
    m_pages->addWidget(databasePage);
    layout->addWidget(m_pages);
    connect(m_kind, &QComboBox::currentIndexChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_kind->setCurrentIndex(source.kind == DataSource::Kind::Database ? 1 : 0);
    m_pages->setCurrentIndex(m_kind->currentIndex());

    // The rows, to tick the ones to print.
    auto *loadRow = new QHBoxLayout;
    auto *loadButton = new QPushButton(QIcon::fromTheme(u"view-refresh"_s), i18n("Load Rows"), this);
    connect(loadButton, &QPushButton::clicked, this, &DataSourceDialog::load);
    auto *all = new QPushButton(i18n("Tick All"), this);
    auto *none = new QPushButton(i18n("Tick None"), this);
    connect(all, &QPushButton::clicked, this, [this]() {
        setAllChecked(true);
    });
    connect(none, &QPushButton::clicked, this, [this]() {
        setAllChecked(false);
    });
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    loadRow->addWidget(loadButton);
    loadRow->addWidget(all);
    loadRow->addWidget(none);
    loadRow->addWidget(m_status, 1);
    layout->addLayout(loadRow);
    m_rows = new QTableWidget(this);
    m_rows->setSelectionMode(QAbstractItemView::NoSelection);
    m_rows->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rows->verticalHeader()->setVisible(false);
    layout->addWidget(m_rows, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateSheets();
    m_sheet->setCurrentIndex(std::max(0, m_sheet->findText(source.sheet)));
    showTable();
    resize(760, 620);
}

void DataSourceDialog::chooseFile()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          i18n("Choose Data File"),
                                                          QFileInfo(m_file->text()).absolutePath(),
                                                          i18n("Data files (*.csv *.txt *.xlsx *.xlsm *.ods)") + u";;"_s + i18n("All files (*)"));
    if (!fileName.isEmpty()) {
        m_file->setText(fileName);
        updateSheets();
        load();
    }
}

void DataSourceDialog::updateSheets()
{
    const QString current = m_sheet->currentText();
    m_sheet->clear();
    const bool spreadsheet = isSpreadsheet(m_file->text());
    if (spreadsheet) {
        m_sheet->addItems(DataReader::sheets(m_file->text()));
        m_sheet->setCurrentIndex(std::max(0, m_sheet->findText(current)));
    }
    m_sheet->setEnabled(spreadsheet);
    m_delimiter->setEnabled(!spreadsheet);
}

LabelCreator::DataSource DataSourceDialog::source() const
{
    DataSource source;
    source.kind = DataSource::Kind(m_kind->currentData().toInt());
    if (source.kind == DataSource::Kind::File) {
        source.fileName = m_file->text().trimmed();
        // The first sheet is the default: only another one is named.
        source.sheet = m_sheet->currentIndex() > 0 ? m_sheet->currentText() : QString();
        const QString delimiter = m_delimiter->currentData().toString();
        source.delimiter = delimiter.isEmpty() ? QChar() : delimiter.at(0);
        source.firstRowNames = m_firstRowNames->isChecked();
    } else {
        source.driver = m_driver->currentData().toString();
        source.database = m_database->text().trimmed();
        source.host = m_host->text().trimmed();
        source.port = m_port->value();
        source.user = m_user->text().trimmed();
        source.query = m_query->toPlainText().trimmed();
    }
    return source;
}

void DataSourceDialog::load()
{
    QString error;
    const DataSource chosen = source();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto table = DataReader::read(chosen, m_password->text(), &error);
    QApplication::restoreOverrideCursor();
    if (!table) {
        m_status->setText(error);
        return;
    }
    m_table = table;
    m_tickAll = true; // a new load ticks every row
    showTable();
}

void DataSourceDialog::showTable()
{
    m_rows->clear();
    if (!m_table) {
        m_rows->setRowCount(0);
        m_rows->setColumnCount(0);
        m_status->setText(i18n("Load the rows to see them and choose which to print."));
        return;
    }
    m_rows->setColumnCount(int(m_table->columns.size()) + 1);
    QStringList headers{i18n("Print")};
    for (const QString &column : std::as_const(m_table->columns)) {
        headers << u'{' + column + u'}';
    }
    m_rows->setHorizontalHeaderLabels(headers);
    m_rows->setRowCount(int(m_table->rows.size()));
    for (int row = 0; row < m_table->rows.size(); ++row) {
        auto *tick = new QTableWidgetItem;
        tick->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        tick->setCheckState(m_tickAll || m_initialSelection.contains(row) ? Qt::Checked : Qt::Unchecked);
        m_rows->setItem(row, 0, tick);
        for (int column = 0; column < m_table->columns.size(); ++column) {
            m_rows->setItem(row, column + 1, new QTableWidgetItem(m_table->rows.at(row).value(column)));
        }
    }
    m_rows->resizeColumnsToContents();
    m_status->setText(i18np("1 row; columns: %2", "%1 rows; columns: %2", m_table->rows.size(), m_table->columns.join(u", "_s)));
}

void DataSourceDialog::setAllChecked(bool checked)
{
    for (int row = 0; row < m_rows->rowCount(); ++row) {
        m_rows->item(row, 0)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

std::optional<DataTable> DataSourceDialog::table() const
{
    return m_table;
}

QList<int> DataSourceDialog::selectedRows() const
{
    QList<int> rows;
    for (int row = 0; row < m_rows->rowCount(); ++row) {
        if (m_rows->item(row, 0)->checkState() == Qt::Checked) {
            rows << row;
        }
    }
    return rows;
}

CalculatedFieldsDialog::CalculatedFieldsDialog(const QList<CalculatedField> &fields, const QHash<QString, QString> &sample, QWidget *parent)
    : QDialog(parent)
    , m_sample(sample)
{
    setWindowTitle(i18n("Calculated Fields"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18n("<p>A calculated field is worked out when printing, from other fields, and is used on the label as "
                                  "<b>{Name}</b>, like a column. Each can use the ones above it.</p>"
                                  "<p><b>{Date}</b>, <b>{Time}</b> and <b>{Now}</b> are when printing. <b>{Date+30}</b> or "
                                  "<b>{Installed+1y}</b> moves a date by days, weeks (w), months (m), years (y) or hours (h); <b>{Serial+1}</b> "
                                  "adds to a number. After a <b>|</b>: upper, lower, trim, left:3, right:4, mid:2:3, pad:6, "
                                  "date:dd-MM-yyyy, replace:old:new, default:text. "
                                  "For example <b>{Date+30|date:dd-MM-yyyy}</b> or <b>{Prefix}-{Serial|pad:6}</b>.</p>"),
                             this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({i18n("Name"), i18n("Worked out from"), i18n("Example")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);
    for (const CalculatedField &field : fields) {
        addRow(field);
    }
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item->column() != 2) {
            updateResults();
        }
    });
    updateResults();

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QIcon::fromTheme(u"list-add"_s), i18n("Add"), this);
    auto *remove = new QPushButton(QIcon::fromTheme(u"list-remove"_s), i18n("Remove"), this);
    connect(add, &QPushButton::clicked, this, [this]() {
        addRow({i18n("Field%1", m_table->rowCount() + 1), u"{Date}"_s});
        updateResults();
        m_table->setCurrentCell(m_table->rowCount() - 1, 0);
        m_table->editItem(m_table->item(m_table->rowCount() - 1, 0));
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        if (m_table->currentRow() >= 0) {
            m_table->removeRow(m_table->currentRow());
            updateResults();
        }
    });
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);
    resize(640, 420);
}

void CalculatedFieldsDialog::addRow(const CalculatedField &field)
{
    const QSignalBlocker blocker(m_table);
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(field.name));
    m_table->setItem(row, 1, new QTableWidgetItem(field.expression));
    auto *example = new QTableWidgetItem;
    example->setFlags(Qt::ItemIsEnabled);
    m_table->setItem(row, 2, example);
}

void CalculatedFieldsDialog::updateResults()
{
    const QSignalBlocker blocker(m_table);
    QHash<QString, QString> values = m_sample;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString name = m_table->item(row, 0)->text().trimmed();
        const QString result = Fields::fill(m_table->item(row, 1)->text(), values);
        m_table->item(row, 2)->setText(result);
        if (!name.isEmpty()) {
            values.insert(name, result);
        }
    }
}

QList<CalculatedField> CalculatedFieldsDialog::fields() const
{
    // Fields are written {Name}: names need something, and no braces.
    QList<CalculatedField> fields;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QString name = m_table->item(row, 0)->text().trimmed();
        name.remove(u'{').remove(u'}');
        if (!name.isEmpty()) {
            fields << CalculatedField{name, m_table->item(row, 1)->text()};
        }
    }
    return fields;
}

QuestionsDialog::QuestionsDialog(const QList<PrintQuestion> &questions, QWidget *parent)
    : QDialog(parent)
    , m_questions(questions)
{
    setWindowTitle(i18n("Print-Time Questions"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18n("Questions asked each time the label is printed, such as a batch number. The answer fills <b>{Name}</b> on the label."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *row = new QHBoxLayout;
    auto *listColumn = new QVBoxLayout;
    m_list = new QListWidget(this);
    listColumn->addWidget(m_list, 1);
    auto *add = new QPushButton(QIcon::fromTheme(u"list-add"_s), i18n("Add"), this);
    auto *remove = new QPushButton(QIcon::fromTheme(u"list-remove"_s), i18n("Remove"), this);
    connect(add, &QPushButton::clicked, this, &QuestionsDialog::addQuestion);
    connect(remove, &QPushButton::clicked, this, &QuestionsDialog::removeQuestion);
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(add);
    listButtons->addWidget(remove);
    listColumn->addLayout(listButtons);
    row->addLayout(listColumn, 1);

    m_editor = new QWidget(this);
    auto *form = new QFormLayout(m_editor);
    m_name = new QLineEdit(m_editor);
    m_name->setPlaceholderText(u"Batch"_s);
    form->addRow(i18n("Field name:"), m_name);
    m_prompt = new QLineEdit(m_editor);
    m_prompt->setPlaceholderText(i18n("Batch number"));
    form->addRow(i18n("Question:"), m_prompt);
    m_default = new QLineEdit(m_editor);
    form->addRow(i18n("Default answer:"), m_default);
    m_required = new QCheckBox(i18n("An answer is needed"), m_editor);
    form->addRow(QString(), m_required);
    m_minimum = new QSpinBox(m_editor);
    m_minimum->setRange(0, 1000);
    m_minimum->setSpecialValueText(i18n("Any"));
    m_maximum = new QSpinBox(m_editor);
    m_maximum->setRange(0, 1000);
    m_maximum->setSpecialValueText(i18n("Any"));
    auto *lengths = new QHBoxLayout;
    lengths->addWidget(new QLabel(i18n("at least"), m_editor));
    lengths->addWidget(m_minimum);
    lengths->addWidget(new QLabel(i18n("at most"), m_editor));
    lengths->addWidget(m_maximum);
    form->addRow(i18n("Length:"), lengths);
    m_allowed = new QComboBox(m_editor);
    m_allowed->addItem(i18n("Anything"), int(PrintQuestion::Allowed::Anything));
    m_allowed->addItem(i18n("Digits only"), int(PrintQuestion::Allowed::Digits));
    m_allowed->addItem(i18n("Letters only"), int(PrintQuestion::Allowed::Letters));
    m_allowed->addItem(i18n("Letters and digits"), int(PrintQuestion::Allowed::LettersAndDigits));
    m_allowed->addItem(i18n("A pattern (regular expression)"), int(PrintQuestion::Allowed::Pattern));
    form->addRow(i18n("Allowed:"), m_allowed);
    m_pattern = new QLineEdit(m_editor);
    m_pattern->setPlaceholderText(u"[A-Z]{2}-\\d{4}"_s);
    form->addRow(i18n("Pattern:"), m_pattern);
    connect(m_allowed, &QComboBox::currentIndexChanged, this, [this]() {
        m_pattern->setEnabled(PrintQuestion::Allowed(m_allowed->currentData().toInt()) == PrintQuestion::Allowed::Pattern);
    });
    row->addWidget(m_editor, 2);
    layout->addLayout(row, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        storeQuestion();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    for (const PrintQuestion &question : std::as_const(m_questions)) {
        m_list->addItem(question.name);
    }
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        storeQuestion();
        showQuestion(row);
    });
    m_list->setCurrentRow(m_questions.isEmpty() ? -1 : 0);
    showQuestion(m_list->currentRow());
    resize(680, 400);
}

void QuestionsDialog::showQuestion(int row)
{
    m_current = row;
    m_editor->setEnabled(row >= 0);
    const PrintQuestion question = row >= 0 ? m_questions.at(row) : PrintQuestion();
    m_name->setText(question.name);
    m_prompt->setText(question.prompt);
    m_default->setText(question.defaultValue);
    m_required->setChecked(question.required);
    m_minimum->setValue(question.minimumLength);
    m_maximum->setValue(question.maximumLength);
    m_allowed->setCurrentIndex(std::max(0, m_allowed->findData(int(question.allowed))));
    m_pattern->setText(question.pattern);
    m_pattern->setEnabled(question.allowed == PrintQuestion::Allowed::Pattern);
}

void QuestionsDialog::storeQuestion()
{
    if (m_current < 0 || m_current >= m_questions.size()) {
        return;
    }
    PrintQuestion &question = m_questions[m_current];
    // Fields are written {Name}: no braces in the name.
    question.name = m_name->text().trimmed().remove(u'{').remove(u'}');
    question.prompt = m_prompt->text().trimmed();
    question.defaultValue = m_default->text();
    question.required = m_required->isChecked();
    question.minimumLength = m_minimum->value();
    question.maximumLength = m_maximum->value();
    question.allowed = PrintQuestion::Allowed(m_allowed->currentData().toInt());
    question.pattern = m_pattern->text();
    if (QListWidgetItem *item = m_list->item(m_current)) {
        item->setText(question.name);
    }
}

void QuestionsDialog::addQuestion()
{
    storeQuestion();
    PrintQuestion question;
    question.name = i18nc("default name of a print-time question", "Question%1", m_questions.size() + 1);
    m_questions << question;
    m_list->addItem(question.name);
    m_list->setCurrentRow(int(m_questions.size() - 1));
    m_name->setFocus();
    m_name->selectAll();
}

void QuestionsDialog::removeQuestion()
{
    const int row = m_list->currentRow();
    if (row < 0) {
        return;
    }
    m_current = -1; // nothing to store from the removed one
    m_questions.removeAt(row);
    delete m_list->takeItem(row);
    showQuestion(m_list->currentRow());
}

QList<PrintQuestion> QuestionsDialog::questions() const
{
    QList<PrintQuestion> questions;
    for (const PrintQuestion &question : m_questions) {
        if (!question.name.isEmpty()) {
            questions << question;
        }
    }
    return questions;
}

AnswersDialog::AnswersDialog(const QList<PrintQuestion> &questions, const QHash<QString, QString> &previous, QWidget *parent)
    : QDialog(parent)
    , m_questions(questions)
{
    setWindowTitle(i18n("Print"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    for (const PrintQuestion &question : questions) {
        auto *field = new QLineEdit(previous.value(question.name, question.defaultValue), this);
        if (question.maximumLength > 0) {
            field->setMaxLength(question.maximumLength);
        }
        connect(field, &QLineEdit::textChanged, this, &AnswersDialog::check);
        form->addRow((question.prompt.isEmpty() ? question.name : question.prompt) + (question.required ? u" *"_s : QString()) + u':', field);
        m_fields << field;
    }
    layout->addLayout(form);
    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    layout->addWidget(m_problem);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(i18n("Print"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    check();
}

void AnswersDialog::check()
{
    for (qsizetype i = 0; i < m_questions.size(); ++i) {
        if (const auto problem = Fields::problem(m_questions.at(i), m_fields.at(i)->text())) {
            m_problem->setText(*problem);
            m_ok->setEnabled(false);
            return;
        }
    }
    m_problem->clear();
    m_ok->setEnabled(true);
}

QHash<QString, QString> AnswersDialog::answers() const
{
    QHash<QString, QString> answers;
    for (qsizetype i = 0; i < m_questions.size(); ++i) {
        answers.insert(m_questions.at(i).name, m_fields.at(i)->text());
    }
    return answers;
}

QString askDatabasePassword(const DataSource &source, QWidget *parent, bool *ok)
{
    return QInputDialog::getText(parent,
                                 i18n("Database Password"),
                                 i18n("Password for %1 on %2 (not saved):", source.user.isEmpty() ? i18n("the database") : source.user, source.database),
                                 QLineEdit::Password,
                                 QString(),
                                 ok);
}
