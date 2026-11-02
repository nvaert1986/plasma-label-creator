/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow_p.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;
using MainWindowPrivate::labelFileFilter;
using MainWindowPrivate::settings;

// Print-only mode: a folder of labels, a form and the print bar; the label
// itself cannot be edited.

namespace
{

// The toolbars of the designer, hidden in print-only mode.
const QStringList DesignerToolBars = {u"mainToolBar"_s, u"insertToolBar"_s, u"formatToolBar"_s, u"arrangeToolBar"_s};

// What stays available in print-only mode: printing, opening, zooming,
// help and settings.
bool allowedInPrintOnly(const QString &name)
{
    static const QStringList allowed = {u"file_print"_s,
                                        u"file_print_preview"_s,
                                        u"file_print_only"_s,
                                        u"file_open"_s,
                                        u"file_open_recent"_s,
                                        u"file_export"_s,
                                        u"file_quit"_s,
                                        u"layout_recent"_s,
                                        u"data_choose_rows"_s,
                                        u"file_print_history"_s,
                                        u"file_printer_status"_s,
                                        u"view_zoom_in"_s,
                                        u"view_zoom_out"_s,
                                        u"view_fit_to_page"_s,
                                        u"view_actual_size"_s};
    return allowed.contains(name) || name.startsWith(u"help_"_s) || name.startsWith(u"options_"_s) || name.startsWith(u"switch_application_language"_s);
}

} // namespace

void MainWindow::setupPrintOnlyActions()
{
    KActionCollection *actions = actionCollection();
    m_printOnlyAction = new KToggleAction(QIcon::fromTheme(u"document-print-direct"_s), i18n("Print-Only Mode"), this);
    m_printOnlyAction->setToolTip(i18n("Choose a label from a folder, fill in its form and print, without editing it"));
    actions->addAction(u"file_print_only"_s, m_printOnlyAction);
    connect(m_printOnlyAction, &QAction::triggered, this, &MainWindow::setPrintOnly);

    QAction *versions = actions->addAction(u"file_versions"_s, this, &MainWindow::showVersions);
    versions->setText(i18n("&Versions…"));
    versions->setIcon(QIcon::fromTheme(u"document-revert"_s));
    versions->setToolTip(i18n("Earlier versions of this label, kept each time it was saved over"));
    QAction *history = actions->addAction(u"file_print_history"_s, this, &MainWindow::showPrintHistory);
    history->setText(i18n("Print &History…"));
    history->setIcon(QIcon::fromTheme(u"view-history"_s));
    history->setToolTip(i18n("What was printed, when and where; open a label as printed or print it again"));

    QAction *status = actions->addAction(u"file_printer_status"_s, this, &MainWindow::showPrinterStatus);
    status->setText(i18n("Printer &Status…"));
    status->setIcon(QIcon::fromTheme(u"printer"_s));
    status->setToolTip(i18n("How every label printer is: its queue, jobs waiting, and what network printers say"));
    connect(m_printBar, &PrintBar::printerStatusRequested, this, &MainWindow::showPrinterStatus);

    QAction *rows = actions->addAction(u"data_choose_rows"_s, this, &MainWindow::chooseRows);
    rows->setText(i18n("Choose Rows…"));
    rows->setIcon(QIcon::fromTheme(u"view-list-details"_s));
    rows->setToolTip(i18n("Tick the rows of the label's data to print"));

    connect(m_printOnlyPanel, &PrintOnlyPanel::labelChosen, this, [this](const QString &fileName) {
        if (fileName != m_editor->fileName() && maybeSave()) {
            openFile(fileName);
        }
    });
    connect(m_printOnlyPanel, &PrintOnlyPanel::folderChosen, this, [](const QString &folder) {
        settings().writeEntry("PrintOnlyFolder", folder);
    });
    // Another label: its own form.
    connect(m_editor, &LabelEditor::fileNameChanged, this, [this]() {
        if (m_printOnly) {
            updatePrintOnlyForm();
        }
    });
    // What prints, as the answers change and counters move on.
    connect(m_printOnlyPanel, &PrintOnlyPanel::answersChanged, this, &MainWindow::updatePrintOnlyPreview);
    connect(m_editor, &LabelEditor::documentChanged, this, [this]() {
        if (m_printOnly) {
            updatePrintOnlyPreview();
        }
    });
}

bool MainWindow::isPrintOnly() const
{
    return m_printOnly;
}

void MainWindow::setPrintOnlyFolder(const QString &folder)
{
    m_printOnlyPanel->setFolder(folder);
    settings().writeEntry("PrintOnlyFolder", folder);
}

void MainWindow::setPrintOnly(bool printOnly)
{
    if (printOnly && !m_printOnly) {
        // Which designer toolbars were showing, to show them again after,
        // also when the app is closed in print-only mode.
        QStringList shown;
        for (const QString &name : DesignerToolBars) {
            if (toolBar(name)->isVisible()) {
                shown << name;
            }
        }
        settings().writeEntry("DesignerToolBars", shown);
    }
    m_printOnly = printOnly;
    settings().writeEntry("PrintOnly", printOnly);
    {
        const QSignalBlocker blocker(m_printOnlyAction);
        m_printOnlyAction->setChecked(printOnly);
    }
    m_view->setReadOnly(printOnly);
    // The address book is hidden: its checked addresses do not print either.
    m_printBar->setCheckedAddresses(printOnly ? 0 : int(m_sidebar->addressBook()->checkedAddresses().size()));
    m_sidebar->setVisible(!printOnly);
    m_printOnlyPanel->setVisible(printOnly);
    // The panel shown gets the side's room; the label the rest.
    const int side = 340;
    m_splitter->setSizes(printOnly ? QList<int>{0, side, std::max(side, width() - side)} : QList<int>{side - 40, 0, std::max(side, width() - side)});
    const QStringList shown = settings().readEntry("DesignerToolBars", DesignerToolBars);
    for (const QString &name : DesignerToolBars) {
        toolBar(name)->setVisible(!printOnly && shown.contains(name));
    }
    toolBar(u"printOnlyToolBar"_s)->setVisible(printOnly);

    if (printOnly) {
        if (m_printOnlyPanel->folder().isEmpty()) {
            const QString folder = m_editor->fileName().isEmpty() ? settings().readEntry("PrintOnlyFolder", QDir::homePath())
                                                                  : QFileInfo(m_editor->fileName()).absolutePath();
            m_printOnlyPanel->setFolder(folder);
        }
        m_printOnlyPanel->setCurrentLabel(m_editor->fileName());
        updatePrintOnlyForm();
        keepEditingOff();
    } else {
        // Everything the label's state does not decide is back on.
        for (QAction *action : actionCollection()->actions()) {
            action->setEnabled(true);
        }
        actionCollection()->action(u"edit_undo"_s)->setEnabled(m_editor->undoStack()->canUndo());
        actionCollection()->action(u"edit_redo"_s)->setEnabled(m_editor->undoStack()->canRedo());
        m_paste->setEnabled(LabelClipboard::canPaste(QApplication::clipboard()->mimeData()));
        actionCollection()->action(u"label_custom_size"_s)->setEnabled(m_printBar->acceptsCustomSizes());
        updateActions();
        updateFormatActions();
    }
    updateCaption();
}

void MainWindow::keepEditingOff()
{
    if (!m_printOnly) {
        return;
    }
    for (QAction *action : actionCollection()->actions()) {
        if (!allowedInPrintOnly(action->objectName())) {
            action->setEnabled(false);
        }
    }
    // The angle box is a widget of its own.
    m_angle->setEnabled(false);
}

void MainWindow::updatePrintOnlyForm()
{
    ensureDataLoaded(true);
    m_printOnlyPanel->setCurrentLabel(m_editor->fileName());
    m_printOnlyPanel->setQuestions(m_editor->document().questions, m_answers, m_dataTable);
    actionCollection()->action(u"data_choose_rows"_s)->setEnabled(m_editor->document().dataSource.kind != DataSource::Kind::None);
    updatePrintOnlyPreview();
}

void MainWindow::updatePrintOnlyPreview()
{
    // The answers, and the first row to print when printing from data.
    QHash<QString, QString> values;
    if (m_printBar->printRows() && m_dataTable && !m_dataRows.isEmpty()) {
        values = m_dataTable->values(m_dataRows.constFirst());
    }
    values.insert(m_printOnlyPanel->answers());
    m_view->setShownDocument(filled(m_editor->document(), values));
}

void MainWindow::chooseRows()
{
    const DataSource &source = m_editor->document().dataSource;
    if (source.kind == DataSource::Kind::None) {
        KMessageBox::information(this, i18n("This label prints without data: it has no data source."));
        return;
    }
    if (!ensureDataLoaded()) {
        return;
    }
    DataSourceDialog dialog(source, m_dataTable, m_dataRows, this);
    dialog.setRowsOnly(true);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (dialog.table()) {
        m_dataTable = dialog.table();
        m_dataTableSource = source;
    }
    m_dataRows = dialog.selectedRows();
    updateDataRows();
    m_printBar->choosePrintRows();
    updatePrintOnlyPreview();
}

void MainWindow::showVersions()
{
    const QString fileName = m_editor->fileName();
    if (fileName.isEmpty()) {
        KMessageBox::information(this, i18n("Versions are kept each time a label is saved over. This label has not been saved yet."));
        return;
    }
    VersionsDialog dialog(fileName, LabelVersions().versions(fileName), this);
    if (dialog.exec() != QDialog::Accepted || dialog.chosenVersion().isEmpty()) {
        return;
    }
    LabelReader reader;
    const auto version = reader.readFile(dialog.chosenVersion());
    if (!version) {
        KMessageBox::error(this, i18n("The version could not be read: %1", reader.errorString()));
        return;
    }
    // On the canvas as a change: undo brings back what was there.
    m_editor->apply(*version, i18n("Restore Version"), -1);
}

void MainWindow::showPrintHistory()
{
    PrintHistory history;
    PrintHistoryDialog dialog(history.records(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const PrintRecord record = dialog.chosenRecord();
    switch (dialog.action()) {
    case PrintHistoryDialog::Action::None:
        return;
    case PrintHistoryDialog::Action::Clear:
        history.clear();
        return;
    case PrintHistoryDialog::Action::Open: {
        if (!maybeSave()) {
            return;
        }
        QString error;
        const auto label = history.label(record, &error);
        if (!label) {
            KMessageBox::error(this, i18n("The label as printed could not be read: %1", error));
            return;
        }
        // As printed; saved where the user wants it.
        m_editor->reset(*label);
        m_answers.insert(record.answers);
        statusBar()->showMessage(i18n("The label %1 as printed on %2.", record.title, QLocale().toString(record.when, QLocale::ShortFormat)), 10000);
        return;
    }
    case PrintHistoryDialog::Action::PrintAgain:
        printAgain(record);
        return;
    }
}

void MainWindow::printAgain(const PrintRecord &record)
{
    if (!maybeSave()) {
        return;
    }
    QString error;
    const auto label = PrintHistory().label(record, &error);
    if (!label) {
        KMessageBox::error(this, i18n("The label as printed could not be read: %1", error));
        return;
    }
    // The same labels: each data row printed then, with the answers given then.
    QList<LabelDocument> labels;
    if (record.rows.isEmpty()) {
        labels << filled(*label, record.answers);
    }
    for (QHash<QString, QString> values : record.rows) {
        values.insert(record.answers);
        labels << filled(*label, values);
    }
    const QString printer = m_printBar->printerName();
    if (KMessageBox::questionTwoActions(this,
                                        i18np("Print the label %2 again on %3?", "Print the %1 labels of %2 again on %3?", labels.size(), record.title, printer),
                                        i18n("Print Again"),
                                        KGuiItem(i18n("Print"), u"document-print"_s),
                                        KStandardGuiItem::cancel())
        != KMessageBox::PrimaryAction) {
        return;
    }
    m_editor->reset(*label);
    m_answers.insert(record.answers);
    m_printedRows = record.rows;
    // The checks of any print: the label suits the printer, the printer is ready.
    if (!confirmLabelFitsPrinter()) {
        return;
    }
    m_reprintTitle = record.title;
    printLabels(labels);
    m_reprintTitle.clear();
}
