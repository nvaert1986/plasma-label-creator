/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow_p.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;
using MainWindowPrivate::labelFileFilter;
using MainWindowPrivate::settings;

// The Data menu: data source, fields, print-time questions.

void MainWindow::setupDataActions()
{
    KActionCollection *actions = actionCollection();
    QAction *source = actions->addAction(u"data_source"_s, this, &MainWindow::editDataSource);
    source->setText(i18n("&Data Source…"));
    source->setIcon(QIcon::fromTheme(u"table"_s, QIcon::fromTheme(u"x-office-spreadsheet"_s)));
    QAction *questions = actions->addAction(u"data_questions"_s, this, &MainWindow::editQuestions);
    questions->setText(i18n("Print-Time &Questions…"));
    questions->setIcon(QIcon::fromTheme(u"help-contents"_s));
    QAction *remove = actions->addAction(u"data_remove"_s, this, [this]() {
        LabelDocument document = m_editor->document();
        document.dataSource = DataSource();
        m_editor->apply(document, i18n("Remove Data Source"));
    });
    remove->setText(i18n("&Remove Data Source"));
    remove->setIcon(QIcon::fromTheme(u"edit-delete"_s));

    // Lists the fields when opened: the data's columns and the questions.
    auto *field = new KActionMenu(QIcon::fromTheme(u"insert-text"_s), i18n("Insert &Field"), this);
    field->setPopupMode(QToolButton::InstantPopup);
    actions->addAction(u"data_insert_field"_s, field);
    connect(field->menu(), &QMenu::aboutToShow, this, [this, field]() {
        QMenu *menu = field->menu();
        menu->clear();
        const QStringList fields = availableFields();
        for (const QString &name : fields) {
            menu->addAction(u'{' + name + u'}', this, [this, name]() {
                insertField(name);
            });
        }
        if (fields.isEmpty()) {
            menu->addAction(i18n("No fields yet: choose a data source or add a question"))->setEnabled(false);
        }
    });
}

QStringList MainWindow::availableFields()
{
    QStringList fields;
    ensureDataLoaded(true);
    if (m_dataTable) {
        fields << m_dataTable->columns;
    }
    for (const PrintQuestion &question : m_editor->document().questions) {
        if (!fields.contains(question.name)) {
            fields << question.name;
        }
    }
    return fields;
}

void MainWindow::insertField(const QString &name)
{
    const QString field = u'{' + name + u'}';
    const int index = m_editor->selection();
    if (index < 0) {
        LabelDocument document = m_editor->document();
        LabelObject object = ObjectFactory::create(ObjectFactory::Kind::Text, document);
        std::get_if<TextObject>(&object)->spans.first().text = field;
        insert(object);
        return;
    }
    LabelDocument document = m_editor->document();
    bool placed = true;
    std::visit(
        [&](auto &o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                if (o.spans.isEmpty()) {
                    o.spans = {{field, LabelFont(), Qt::black}};
                } else {
                    o.spans.last().text += field;
                }
                if constexpr (std::is_same_v<T, AddressObject>) {
                    o.fitLineFonts();
                }
            } else if constexpr (std::is_same_v<T, BarcodeObject> || std::is_same_v<T, CircularTextObject>) {
                o.text += field;
            } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject>) {
                o.preText += field;
            } else if constexpr (std::is_same_v<T, ImageObject>) {
                // A picture per row: the field is (part of) its file path.
                o.imageLocation = o.imageLocation.value_or(QString()) + field;
            } else {
                placed = false;
            }
        },
        document.objects[index]);
    if (placed) {
        m_editor->apply(document, i18n("Insert Field"));
    }
}

bool MainWindow::ensureDataLoaded(bool quiet)
{
    const DataSource &source = m_editor->document().dataSource;
    if (source.kind == DataSource::Kind::None) {
        return false;
    }
    if (m_dataTable && m_dataTableSource == source) {
        return true;
    }
    QString password = m_databasePassword;
    if (source.kind == DataSource::Kind::Database && !source.user.isEmpty() && password.isEmpty()) {
        if (quiet) {
            return false;
        }
        bool ok = false;
        password = askDatabasePassword(source, this, &ok);
        if (!ok) {
            return false;
        }
    }
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto table = DataReader::read(source, password, &error);
    QApplication::restoreOverrideCursor();
    if (!table) {
        if (!quiet) {
            KMessageBox::error(this, i18n("The data could not be read:\n%1", error));
        }
        return false;
    }
    if (source.kind == DataSource::Kind::Database) {
        m_databasePassword = password;
    }
    m_dataTable = table;
    m_dataTableSource = source;
    m_dataRows.clear();
    for (int row = 0; row < table->rows.size(); ++row) {
        m_dataRows << row;
    }
    updateDataRows();
    return true;
}

void MainWindow::updateDataRows()
{
    const DataSource &source = m_editor->document().dataSource;
    if (m_dataTable && m_dataTableSource != source) {
        m_dataTable.reset();
        m_dataRows.clear();
    }
    m_printBar->setDataRows(source.kind != DataSource::Kind::None, m_dataTable ? int(m_dataRows.size()) : -1);
}

void MainWindow::editDataSource()
{
    ensureDataLoaded(true);
    DataSourceDialog dialog(m_editor->document().dataSource, m_dataTable, m_dataRows, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const DataSource source = dialog.source();
    if (dialog.table()) {
        m_dataTable = dialog.table();
        m_dataTableSource = source;
        m_dataRows = dialog.selectedRows();
    }
    if (source != m_editor->document().dataSource) {
        LabelDocument document = m_editor->document();
        document.dataSource = source;
        m_editor->apply(document, i18n("Change Data Source"));
    }
    updateDataRows();
    m_printBar->choosePrintRows();
}

void MainWindow::editQuestions()
{
    QuestionsDialog dialog(m_editor->document().questions, this);
    if (dialog.exec() != QDialog::Accepted || dialog.questions() == m_editor->document().questions) {
        return;
    }
    LabelDocument document = m_editor->document();
    document.questions = dialog.questions();
    m_editor->apply(document, i18n("Change Print-Time Questions"));
}

LabelDocument MainWindow::filled(const LabelDocument &document, const QHash<QString, QString> &values) const
{
    LabelDocument result = Fields::fill(document, values);
    // Relative picture paths: next to the data file, else next to the label.
    const DataSource &source = document.dataSource;
    const QString base = source.kind == DataSource::Kind::File ? QFileInfo(source.fileName).absolutePath()
        : m_editor->fileName().isEmpty()                       ? QDir::currentPath()
                                                               : QFileInfo(m_editor->fileName()).absolutePath();
    for (LabelObject &object : result.objects) {
        if (auto *image = std::get_if<ImageObject>(&object); image && image->imageLocation && !image->imageLocation->isEmpty()) {
            if (QFileInfo(*image->imageLocation).isRelative()) {
                image->imageLocation = QDir(base).absoluteFilePath(*image->imageLocation);
            }
        }
    }
    return result;
}
