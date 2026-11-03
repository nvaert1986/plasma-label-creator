/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow_p.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;
using MainWindowPrivate::labelFileFilter;
using MainWindowPrivate::settings;

// Printing, print preview, export, and checking the label fits the printer.

bool MainWindow::labelFitsPrinter() const
{
    return LabelFit::check(m_editor->document(), m_printBar->paperSizes(), m_printBar->acceptsCustomSizes(), m_printBar->loadedLabelLengthMm())
        == LabelFit::Problem::None;
}

bool MainWindow::confirmLabelFitsPrinter()
{
    const LabelDocument &document = m_editor->document();
    const QSizeF size = document.paperSize();
    const qreal widthMm = Units::twipsToMm(size.width());
    const qreal lengthMm = Units::twipsToMm(size.height());
    const auto loaded = m_printBar->loadedLabelLengthMm();
    QString reason;
    switch (LabelFit::check(document, m_printBar->paperSizes(), m_printBar->acceptsCustomSizes(), loaded)) {
    case LabelFit::Problem::NotOffered:
        reason = i18n("This label is a %1 (%2 × %3 mm), which is not one of the label types of %4.",
                      document.paperName,
                      QString::number(widthMm, 'f', 1),
                      QString::number(lengthMm, 'f', 1),
                      m_printBar->printerName());
        break;
    case LabelFit::Problem::WrongLength:
        reason = i18n("This label is %1 mm long, but the labels in %2 are %3 mm long.",
                      QString::number(lengthMm, 'f', 1),
                      m_printBar->printerName(),
                      QString::number(*loaded, 'f', 1));
        break;
    case LabelFit::Problem::RollNotSupported:
        reason = i18n("This label is laid out for several labels across or a continuous roll (Layout > Roll Layout), which %1 does not print: "
                      "it takes one die-cut label at a time.",
                      m_printBar->printerName());
        break;
    case LabelFit::Problem::None:
        break;
    }
    if (!reason.isEmpty()
        && KMessageBox::warningContinueCancel(this,
                                              reason + u"\n\n"_s + i18n("Choose the label type of the loaded labels in the Labels tab first, or print anyway."),
                                              i18n("Different Labels"),
                                              KGuiItem(i18n("Print Anyway"), u"document-print"_s))
            != KMessageBox::Continue) {
        return false;
    }
    return confirmPrinterReady();
}

bool MainWindow::confirmPrinterReady()
{
    // What the queue says now, and a network printer said last.
    if (!m_settings.warnNotReady) {
        return true;
    }
    m_printBar->refreshStatus();
    const QStringList problems = m_printBar->printerProblems();
    if (problems.isEmpty()) {
        return true;
    }
    return KMessageBox::warningContinueCancel(this,
                                              i18n("%1 reports: %2.\n\nThe labels may wait in the queue until this is solved.",
                                                   m_printBar->printerName(),
                                                   problems.join(u", "_s)),
                                              i18n("Printer Not Ready"),
                                              KGuiItem(i18n("Print Anyway"), u"document-print"_s))
        == KMessageBox::Continue;
}

void MainWindow::configure()
{
    SettingsDialog dialog(m_settings, this);
    connect(&dialog, &SettingsDialog::applied, this, [this](const AppSettings &settings) {
        settings.save();
        applySettings(AppSettings::load()); // as saved: settings fixed by an administrator stay
    });
    dialog.exec();
}

void MainWindow::applySettings(const AppSettings &settings)
{
    const bool lockedBefore = m_settings.lockPrintOnly;
    m_settings = settings;
    // Designing
    ObjectFactory::setTextFont(settings.textFont);
    m_view->setNudgeSteps(Units::mmToTwips(settings.nudgeMm), Units::mmToTwips(settings.largeNudgeMm));
    m_view->setTurnStep(settings.turnStep);
    const QString step = QLocale().toString(settings.turnStep);
    actionCollection()->action(u"object_turn_left"_s)->setText(i18nc("degrees", "Turn Left %1°", step));
    actionCollection()->action(u"object_turn_right"_s)->setText(i18nc("degrees", "Turn Right %1°", step));
    RulerWidget::setInches(settings.inches);
    for (RulerWidget *ruler : findChildren<RulerWidget *>()) {
        ruler->update();
    }
    // Printing
    m_printBar->setStatusPolling(settings.askPrinters, settings.statusSeconds);
    AltecLabels::setRollDefaults(settings.acrossGapMm, settings.continuousLengthMm);
    m_printBar->reloadPaperSizes();
    // History
    m_editor->setVersions(settings.versionsKept, settings.versionsNextToLabel);
    m_recentLayouts.setMaxLayouts(settings.recentLayoutsKept);
    // Locked into print-only mode now; unlocked, the switch is back.
    if (settings.lockPrintOnly && !lockedBefore && !m_printOnly) {
        setPrintOnly(true);
    }
    keepEditingOff();
    if (!settings.lockPrintOnly && m_printOnly) {
        m_printOnlyAction->setEnabled(true);
    }
}

void MainWindow::showPrinterStatus()
{
    PrinterStatusDialog dialog(this);
    dialog.exec();
    m_printBar->refreshStatus();
}

LabelPrinter::Options MainWindow::printOptions(const PrintRun::Run &run, const PrintSettings &printing) const
{
    LabelPrinter::Options options;
    // The printer's own calibration, in its directions, in points.
    options.offset = m_printBar->printOffsetMm() * 72.0 / Units::MmPerInch;
    options.copies = run.copies;
    options.headerLabels = run.headerLabels;
    options.trailerLabels = run.trailerLabels;
    options.printQuality = m_printBar->graphicsQuality() ? u"Graphics"_s : u"Text"_s;
    // The label's own density, darkness and speed, where it has them.
    options.printDensity = printing.density;
    options.darkness = printing.darkness;
    options.speed = printing.speed;
    options.jobTitle = m_editor->fileName().isEmpty() ? i18n("Label") : QFileInfo(m_editor->fileName()).fileName();
    return options;
}

std::optional<PrintRun::Run> MainWindow::labelsToPrint()
{
    finishEditing();
    if (m_printBar->printerName().isEmpty()) {
        KMessageBox::error(this, i18n("No printer was found. Connect a label printer or add one in the system's printer settings first."));
        return std::nullopt;
    }
    if (!confirmLabelFitsPrinter()) {
        return std::nullopt;
    }
    const LabelDocument document = m_editor->document();

    // The questions first: their answers go on every label of the run.
    QHash<QString, QString> answers;
    if (m_printOnly) {
        // Print-only mode has the form at hand: its answers, once they do.
        if (const auto problem = m_printOnlyPanel->problem()) {
            KMessageBox::error(this, *problem);
            return std::nullopt;
        }
        answers = m_printOnlyPanel->answers();
        m_answers = answers;
    } else if (!document.questions.isEmpty()) {
        // Drop-downs and lookups may come from the data source.
        ensureDataLoaded(true);
        AnswersDialog dialog(document.questions, m_answers, m_dataTable, this);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        answers = dialog.answers();
        m_answers = answers;
    }

    PrintRun::Request request;
    request.document = document;
    request.labelFile = m_editor->fileName();
    request.pictureFolder = pictureFolder(document);
    request.answers = answers;
    request.copies = m_printBar->copies();
    m_printedRows.clear();
    if (m_printBar->printRows()) {
        // One label per ticked row, with the row's values and the answers.
        if (!ensureDataLoaded()) {
            return std::nullopt;
        }
        if (m_dataRows.isEmpty()) {
            KMessageBox::error(this, i18n("No rows are ticked. Choose them under Data > Data Source."));
            return std::nullopt;
        }
        for (const int row : std::as_const(m_dataRows)) {
            m_printedRows << m_dataTable->values(row);
        }
        request.rows = m_printedRows;
    } else if (m_printBar->printAddresses()) {
        // One label per checked address, each with the address filled in.
        const bool hasAddress = std::any_of(document.objects.cbegin(), document.objects.cend(), [](const LabelObject &object) {
            return std::holds_alternative<AddressObject>(object);
        });
        if (!hasAddress) {
            KMessageBox::error(this, i18n("To print the checked addresses, the label needs an address object."));
            return std::nullopt;
        }
        for (const QString &address : m_sidebar->addressBook()->checkedAddresses()) {
            LabelDocument label = document;
            for (LabelObject &object : label.objects) {
                if (auto *target = std::get_if<AddressObject>(&object)) {
                    const TextSpan first = target->spans.isEmpty() ? TextSpan{QString(), LabelFont(), Qt::black} : target->spans.constFirst();
                    target->spans = {{address, first.font, first.color}};
                    target->fitLineFonts();
                    break;
                }
            }
            request.prepared << filled(label, answers);
        }
    }
    QString error;
    auto run = PrintRun::make(request, &error);
    if (!run) {
        KMessageBox::error(this, error);
        return std::nullopt;
    }
    return run;
}

void MainWindow::print()
{
    if (const auto run = labelsToPrint()) {
        printLabels(*run);
    }
}

bool MainWindow::useSharedCounters(QList<LabelDocument> &labels,
                                   std::vector<std::unique_ptr<SharedCounter>> *locks,
                                   QHash<QString, int> *values,
                                   int count,
                                   QSet<QString> *byAddon)
{
    // The label's shared counters, read from their files (locked first when
    // printing); every label of the run that shares one gets its value.
    QHash<QString, int> read;
    Plugin *addon = currentAddon();
    for (const LabelObject &object : m_editor->document().objects) {
        const auto *counter = std::get_if<CounterObject>(&object);
        if (!counter || counter->sharedFile.isEmpty() || read.contains(counter->sharedFile)) {
            continue;
        }
        // An add-on's label may count on with its add-on (a team's counter
        // on a server, say): values are taken there, never twice.
        if (addon) {
            QString error;
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const auto value = addon->takeCounter(currentOrigin(), counter->sharedFile, counter->current, locks ? count : 0, counter->increment, &error);
            QApplication::restoreOverrideCursor();
            if (value) {
                read.insert(counter->sharedFile, *value);
                if (byAddon) {
                    byAddon->insert(counter->sharedFile);
                }
                continue;
            }
            if (!error.isEmpty()) {
                KMessageBox::error(this, i18n("The shared counter %1 could not be reached:\n%2", counter->sharedFile, error));
                return false;
            }
        }
        auto shared = std::make_unique<SharedCounter>(counter->sharedFile);
        // Another computer may be printing: it is waited for a while.
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool locked = !locks || shared->lock(m_settings.counterWaitSeconds * 1000);
        QApplication::restoreOverrideCursor();
        if (!locked) {
            KMessageBox::error(this, shared->errorString());
            return false;
        }
        const auto value = shared->read(counter->current);
        if (!value) {
            KMessageBox::error(this, shared->errorString());
            return false;
        }
        read.insert(counter->sharedFile, *value);
        if (locks) {
            locks->push_back(std::move(shared));
        }
    }
    for (LabelDocument &label : labels) {
        for (LabelObject &object : label.objects) {
            if (auto *counter = std::get_if<CounterObject>(&object); counter && read.contains(counter->sharedFile)) {
                counter->current = read.value(counter->sharedFile);
            }
        }
    }
    if (values) {
        *values = read;
    }
    return true;
}

void MainWindow::printLabels(const PrintRun::Run &prepared)
{
    // Shared counters stay locked from reading their files until the next
    // values are written, so two computers never print the same numbers.
    PrintRun::Run run = prepared;
    std::vector<std::unique_ptr<SharedCounter>> locks;
    QHash<QString, int> sharedValues;
    QSet<QString> byAddon;
    // As LabelPrinter counts them: every copy, without header and trailer labels.
    const int counted = int(std::max<qsizetype>(0, run.labels.size() - run.headerLabels - run.trailerLabels)) * std::max(1, run.copies);
    if (!useSharedCounters(run.labels, &locks, &sharedValues, counted, &byAddon)) {
        return;
    }
    const QString printerName = m_printBar->printerName();
    LabelPrinter printer(printerName, m_printBar->printerDriver());

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const int job = printer.print(run.labels, printOptions(run, m_editor->document().printing));
    QApplication::restoreOverrideCursor();

    if (job == 0) {
        KMessageBox::error(this, printer.errorString());
        return;
    }
    QHash<int, int> sharedNext;
    QStringList written;
    const QList<LabelObject> &objects = m_editor->document().objects;
    for (int i = 0; i < objects.size(); ++i) {
        const auto *counter = std::get_if<CounterObject>(&objects.at(i));
        if (!counter || counter->sharedFile.isEmpty()) {
            continue;
        }
        const int next = sharedValues.value(counter->sharedFile, counter->current) + printer.printedLabels() * counter->increment;
        sharedNext.insert(i, next);
        // The add-on moved its counters on when they were taken.
        if (written.contains(counter->sharedFile) || byAddon.contains(counter->sharedFile)) {
            continue;
        }
        written << counter->sharedFile;
        SharedCounter file(counter->sharedFile);
        if (!file.write(next)) {
            KMessageBox::error(this, i18n("The labels were printed, but the next counter value could not be saved:\n%1", file.errorString()));
        }
    }
    // Kept as it was printed, before counters move on.
    QString recentError;
    if (!m_recentLayouts.add(m_editor->document(), m_editor->fileName(), &recentError)) {
        qWarning("Could not keep the label in the recently used layouts: %s", qPrintable(recentError));
    }
    PrintRecord record;
    record.when = QDateTime::currentDateTime();
    record.source = m_editor->fileName();
    record.title = !record.source.isEmpty() ? QFileInfo(record.source).fileName()
        : !m_reprintTitle.isEmpty()         ? m_reprintTitle
                                            : m_editor->document().paperName;
    record.printer = printerName;
    record.labels = printer.printedLabels();
    record.job = job;
    for (const PrintQuestion &question : m_editor->document().questions) {
        record.answers.insert(question.name, m_answers.value(question.name));
    }
    record.rows = m_printedRows;
    if (m_settings.historyKept > 0) {
        PrintHistory history;
        history.setMaxRecords(m_settings.historyKept);
        QString historyError;
        if (!history.add(record, m_editor->document(), &historyError)) {
            qWarning("Could not write the print history: %s", qPrintable(historyError));
        }
    }
    // An add-on's label is kept in its history too (a team's, say).
    if (Plugin *addon = currentAddon()) {
        PluginPrint print;
        print.origin = currentOrigin();
        print.content = LabelWriter::toByteArray(m_editor->document());
        print.title = record.title;
        print.printer = record.printer;
        print.labels = record.labels;
        print.answers = record.answers;
        print.rows = record.rows;
        addon->printed(print);
    }
    advanceCounters(printer.printedLabels(), sharedNext);
    // In print-only mode the label keeps its counters itself: nobody will
    // save it by hand.
    if (m_printOnly && m_settings.saveCounters && m_editor->isModified() && !m_editor->fileName().isEmpty()) {
        QString saveError;
        if (!m_editor->save(m_editor->fileName(), &saveError, false)) {
            KMessageBox::error(this, i18n("The counters could not be saved in the label:\n%1", saveError));
        } else {
            // Who only prints may not change the add-on's label: that is no reason to stop.
            addonSaved(false, true);
        }
    }
    // What was printed is the best guess of the labels in the printer.
    settings().writeEntry("Paper", m_editor->document().paperName);
    QString message = i18np("Sent one label to %2 as job %3.", "Sent %1 labels to %2 as job %3.", printer.printedLabels(), printerName, job);
    if (!printer.warnings().isEmpty()) {
        message += u' ' + i18np("Note: %2", "%1 notes, first: %2", printer.warnings().size(), printer.warnings().constFirst());
    }
    statusBar()->showMessage(message, 10000);
}

void MainWindow::printPreview()
{
    const auto run = labelsToPrint();
    if (!run) {
        return;
    }
    // With the shared counters' values as they are now; printing reads
    // them again, locked.
    QList<LabelDocument> previewed = run->labels;
    if (!useSharedCounters(previewed, nullptr, nullptr)) {
        return;
    }
    LabelPrinter printer(m_printBar->printerName(), m_printBar->printerDriver());
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<QImage> pages = printer.pages(previewed, printOptions(*run, m_editor->document().printing));
    QApplication::restoreOverrideCursor();
    if (pages.isEmpty()) {
        KMessageBox::error(this, printer.errorString());
        return;
    }
    // A landscape label is turned on the paper; shown as on the canvas.
    const bool turned = m_editor->document().orientation == PaperOrientation::Landscape && !m_editor->document().tape;
    PrintPreviewDialog dialog(pages, printer.resolution(), m_printBar->printerName(), printer.warnings(), this, turned);
    // What was previewed, without asking the questions again.
    if (dialog.exec() == QDialog::Accepted) {
        printLabels(*run);
    }
}

void MainWindow::exportLabel()
{
    finishEditing();
    // Named after the label, in the folder and format used last.
    const QString base = m_editor->fileName().isEmpty() ? i18n("Label") : QFileInfo(m_editor->fileName()).completeBaseName();
    const QString labelFolder = m_editor->fileName().isEmpty() ? QDir::homePath() : QFileInfo(m_editor->fileName()).absolutePath();
    QString folder = settings().readEntry("ExportFolder", labelFolder);
    if (!QFileInfo(folder).isDir()) {
        folder = labelFolder; // removed since, or on a drive not mounted now
    }
    const QString suffix = settings().readEntry("ExportFormat", QString()) == u"png"_s ? u"png"_s : u"pdf"_s;
    ExportDialog dialog(QDir(folder).filePath(base + u'.' + suffix), settings().readEntry("ExportOutline", false), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString fileName = dialog.fileName();
    const bool png = dialog.png();
    settings().writeEntry("ExportFolder", QFileInfo(fileName).absolutePath());
    settings().writeEntry("ExportFormat", png ? u"png"_s : u"pdf"_s);
    settings().writeEntry("ExportOutline", dialog.outline());

    // As on the canvas, tape laid out to its length; nothing that only shows
    // on screen, and the label's outline only when asked for.
    LabelDocument document = m_editor->document();
    TapeLayout::layout(document);
    LabelRenderer renderer(LabelRenderer::Mode::Print);
    renderer.setOutline(dialog.outline());
    qreal dpi = 300;
    if (!m_printBar->printerName().isEmpty()) {
        LabelPrinter printer(m_printBar->printerName(), m_printBar->printerDriver());
        if (printer.load()) {
            dpi = printer.resolution();
        }
    }
    renderer.setPrinterResolution(dpi);

    bool saved = false;
    if (png) {
        const QImage image = renderer.renderImage(document, dpi);
        saved = image.save(fileName, "PNG");
    } else {
        saved = renderer.writePdf(document, fileName, base);
    }
    if (!saved) {
        KMessageBox::error(this, i18n("Could not write %1.", fileName));
        return;
    }
    statusBar()->showMessage(i18n("Exported to %1.", fileName), 10000);
}

void MainWindow::editPrintOffset()
{
    // Asked for as seen on screen, for the label as it is laid out; kept
    // in the printer's own directions, so it holds for every label.
    const PaperOrientation orientation = m_editor->document().orientation;
    const QPointF shown = LabelPrinter::canvasShift(orientation, m_printBar->printOffsetMm());
    QDialog dialog(this);
    dialog.setWindowTitle(i18n("Print Offset of %1", m_printBar->printerName()));
    auto *form = new QFormLayout(&dialog);
    auto *intro = new QLabel(i18n("Print a label and measure how far everything must move to land where it is on the screen. "
                                  "The offset is kept for this printer and applies to all labels."));
    intro->setWordWrap(true);
    form->addRow(intro);
    const auto millimetreBox = [](qreal value) {
        auto *box = new QDoubleSpinBox;
        box->setRange(-20, 20);
        box->setDecimals(1);
        box->setSingleStep(0.1);
        box->setSuffix(i18nc("millimetres", " mm"));
        box->setValue(value);
        return box;
    };
    auto *right = millimetreBox(shown.x());
    auto *down = millimetreBox(shown.y());
    form->addRow(i18n("Move right by:"), right);
    form->addRow(i18n("Move down by:"), down);
    auto *hint = new QLabel(i18n("Negative values move left and up. The label itself does not move, so what comes within this distance of its edges may be cut off."));
    hint->setWordWrap(true);
    hint->setEnabled(false);
    form->addRow(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, &dialog, [right, down]() {
        right->setValue(0);
        down->setValue(0);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        m_printBar->setPrintOffsetMm(LabelPrinter::paperShift(orientation, QPointF(right->value(), down->value())));
    }
}
