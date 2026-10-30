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
    case LabelFit::Problem::None:
        break;
    }
    if (reason.isEmpty()) {
        return true;
    }
    return KMessageBox::warningContinueCancel(this,
                                              reason + u"\n\n"_s + i18n("Choose the label type of the loaded labels in the Labels tab first, or print anyway."),
                                              i18n("Different Labels"),
                                              KGuiItem(i18n("Print Anyway"), u"document-print"_s))
        == KMessageBox::Continue;
}

LabelPrinter::Options MainWindow::printOptions() const
{
    LabelPrinter::Options options;
    // The printer's own calibration, in its directions, in points.
    options.offset = m_printBar->printOffsetMm() * 72.0 / Units::MmPerInch;
    options.copies = m_printBar->copies();
    options.printQuality = m_printBar->graphicsQuality() ? u"Graphics"_s : u"Text"_s;
    options.jobTitle = m_editor->fileName().isEmpty() ? i18n("Label") : QFileInfo(m_editor->fileName()).fileName();
    return options;
}

std::optional<QList<LabelDocument>> MainWindow::labelsToPrint()
{
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
    if (!document.questions.isEmpty()) {
        AnswersDialog dialog(document.questions, m_answers, this);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        answers = dialog.answers();
        m_answers = answers;
    }

    QList<LabelDocument> labels;
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
            QHash<QString, QString> values = m_dataTable->values(row);
            values.insert(answers);
            labels << filled(document, values);
        }
        return labels;
    }
    if (!m_printBar->printAddresses()) {
        return QList<LabelDocument>{filled(document, answers)};
    }
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
        labels << filled(label, answers);
    }
    return labels;
}

void MainWindow::print()
{
    if (const auto labels = labelsToPrint()) {
        printLabels(*labels);
    }
}

bool MainWindow::useSharedCounters(QList<LabelDocument> &labels, std::vector<std::unique_ptr<SharedCounter>> *locks)
{
    QHash<QString, int> values; // per file
    const LabelDocument &document = m_editor->document();
    for (int i = 0; i < document.objects.size(); ++i) {
        const auto *counter = std::get_if<CounterObject>(&document.objects.at(i));
        if (!counter || counter->sharedFile.isEmpty()) {
            continue;
        }
        if (!values.contains(counter->sharedFile)) {
            auto shared = std::make_unique<SharedCounter>(counter->sharedFile);
            if (locks && !shared->lock()) {
                KMessageBox::error(this, shared->errorString());
                return false;
            }
            const auto value = shared->read(counter->current);
            if (!value) {
                KMessageBox::error(this, shared->errorString());
                return false;
            }
            values.insert(counter->sharedFile, *value);
            if (locks) {
                locks->push_back(std::move(shared));
            }
        }
        for (LabelDocument &label : labels) {
            if (auto *printed = std::get_if<CounterObject>(&label.objects[i])) {
                printed->current = values.value(counter->sharedFile);
            }
        }
    }
    return true;
}

void MainWindow::printLabels(const QList<LabelDocument> &prepared)
{
    // Shared counters stay locked from reading their files until the next
    // values are written, so two computers never print the same numbers.
    QList<LabelDocument> labels = prepared;
    std::vector<std::unique_ptr<SharedCounter>> locks;
    if (!useSharedCounters(labels, &locks)) {
        return;
    }
    const QString printerName = m_printBar->printerName();
    LabelPrinter printer(printerName, m_printBar->printerDriver());

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const int job = printer.print(labels, printOptions());
    QApplication::restoreOverrideCursor();

    if (job == 0) {
        KMessageBox::error(this, printer.errorString());
        return;
    }
    QHash<int, int> sharedNext;
    QStringList written;
    for (int i = 0; i < labels.constFirst().objects.size(); ++i) {
        const auto *counter = std::get_if<CounterObject>(&labels.constFirst().objects.at(i));
        if (!counter || counter->sharedFile.isEmpty()) {
            continue;
        }
        const int next = counter->current + printer.printedLabels() * counter->increment;
        sharedNext.insert(i, next);
        if (written.contains(counter->sharedFile)) {
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
    advanceCounters(printer.printedLabels(), sharedNext);
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
    const auto labels = labelsToPrint();
    if (!labels) {
        return;
    }
    // With the shared counters' values as they are now; printing reads
    // them again, locked.
    QList<LabelDocument> previewed = *labels;
    if (!useSharedCounters(previewed, nullptr)) {
        return;
    }
    LabelPrinter printer(m_printBar->printerName(), m_printBar->printerDriver());
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<QImage> pages = printer.pages(previewed, printOptions());
    QApplication::restoreOverrideCursor();
    if (pages.isEmpty()) {
        KMessageBox::error(this, printer.errorString());
        return;
    }
    PrintPreviewDialog dialog(pages, printer.resolution(), m_printBar->printerName(), printer.warnings(), this);
    // What was previewed, without asking the questions again.
    if (dialog.exec() == QDialog::Accepted) {
        printLabels(*labels);
    }
}

void MainWindow::exportLabel()
{
    const QString pdfFilter = i18n("PDF document (*.pdf)");
    const QString pngFilter = i18n("PNG image at the printer's resolution (*.png)");
    QString filter = pdfFilter;
    const QString base = m_editor->fileName().isEmpty() ? i18n("Label") : QFileInfo(m_editor->fileName()).completeBaseName();
    QString fileName = QFileDialog::getSaveFileName(this, i18n("Export Label"), base + u".pdf"_s, pdfFilter + u";;"_s + pngFilter, &filter);
    if (fileName.isEmpty()) {
        return;
    }
    const bool png = filter == pngFilter || fileName.endsWith(u".png"_s, Qt::CaseInsensitive);
    if (QFileInfo(fileName).suffix().isEmpty()) {
        fileName += png ? u".png"_s : u".pdf"_s;
    }

    // As on the canvas, tape laid out to its length; no outline, nothing
    // that only shows on screen.
    LabelDocument document = m_editor->document();
    TapeLayout::layout(document);
    LabelRenderer renderer(LabelRenderer::Mode::Print);
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
