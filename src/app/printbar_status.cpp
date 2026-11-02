/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printbar.h"

#include <KColorScheme>
#include <KLocalizedString>

#include <QLabel>
#include <QThread>
#include <QTimer>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

// The print bar's printer status: the CUPS queue, and what a network ZPL
// printer says about itself and the labels loaded.

void PrintBar::setStatusPolling(bool askPrinters, int seconds)
{
    m_askPrinters = askPrinters;
    m_statusTimer->setInterval(std::max(5, seconds) * 1000);
}

void PrintBar::refreshStatus()
{
    const QString name = printerName();
    m_queueStatus = name.isEmpty() ? QueueStatus() : LabelPrinter::queueStatus(name);
    const PrinterInfo *printer = currentPrinter();
    if (m_askPrinters && printer && printer->speaksZpl() && Zpl::networkAddress(printer->deviceUri)) {
        if (m_detecting.isEmpty()) {
            detectLabels(); // shows the status when the printer answers
        }
        return;
    }
    updatePrinterStatus(QString());
}

void PrintBar::detectLabels()
{
    const PrinterInfo *printer = currentPrinter();
    const auto address = printer && printer->speaksZpl() ? Zpl::networkAddress(printer->deviceUri) : std::nullopt;
    if (!address) {
        m_zplInfo.reset();
        m_zplPrinter.clear();
        m_detecting.clear();
        updatePrinterStatus(QString());
        return;
    }
    const QString name = printer->name;
    m_detecting = name;
    if (!m_zplInfo) {
        updatePrinterStatus(QString()); // "asking"
    }

    // The query blocks for up to a few seconds; it runs in its own thread
    // and reports back here, if this queue is still the one selected.
    auto result = std::make_shared<std::optional<ZplPrinterInfo>>();
    auto error = std::make_shared<QString>();
    QThread *thread = QThread::create([address, result, error]() {
        *result = Zpl::query(address->first, address->second, 3000, error.get());
    });
    m_detectThreads.removeAll(nullptr);
    m_detectThreads << thread;
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, name, result, error]() {
        if (printerName() != name) {
            return;
        }
        // The label types follow the labels loaded: only when those changed,
        // or the printer started or stopped answering. A printer that keeps
        // not answering changes nothing.
        const bool answered = m_zplInfo && m_zplPrinter == name;
        const QSizeF before = answered ? m_zplInfo->labelSizeMm() : QSizeF();
        m_detecting.clear();
        m_zplInfo = *result;
        m_zplPrinter = name;
        m_zplError = *error;
        updatePrinterStatus(m_zplError);
        if (m_zplInfo.has_value() != answered || (m_zplInfo && m_zplInfo->labelSizeMm() != before)) {
            reloadPaperSizes();
        }
    });
    thread->start();
}

QStringList PrintBar::printerProblems() const
{
    QStringList problems = m_queueStatus.problems();
    if (m_zplInfo && m_zplPrinter == printerName()) {
        problems += m_zplInfo->problems();
    }
    return problems;
}

void PrintBar::updatePrinterStatus(const QString &error)
{
    // Problems first, then what the printer says, then jobs waiting.
    QStringList lines;
    const QStringList problems = printerProblems();
    if (!problems.isEmpty()) {
        lines << problems.join(u", "_s);
    }
    const PrinterInfo *printer = currentPrinter();
    // A network printer not asked on its own has nothing to say until asked.
    const bool network = printer && printer->speaksZpl() && Zpl::networkAddress(printer->deviceUri)
        && (m_askPrinters || m_zplInfo || !m_detecting.isEmpty());
    QString tooltip;
    if (network && !m_zplInfo && !m_detecting.isEmpty()) {
        lines << i18n("Asking the printer which labels are loaded…");
    } else if (network && !m_zplInfo) {
        lines << i18n("The printer did not answer: %1", error.isEmpty() ? m_zplError : error);
    } else if (network && m_zplInfo->problems().isEmpty()) {
        const ZplPrinterInfo &info = *m_zplInfo;
        const QString labels = info.continuous() ? i18n("continuous labels")
                                                 : i18nc("label length", "labels %1 mm long", QString::number(info.labelSizeMm().height(), 'f', 1));
        lines << i18nc("model, dpi, labels", "%1, %2 dpi, %3, ready", info.model, qRound(info.dpi()), labels);
    }
    if (network && m_zplInfo) {
        const ZplPrinterInfo &info = *m_zplInfo;
        tooltip = i18n("Firmware %1, %2, %3", info.firmware, info.thermalTransfer ? i18n("thermal transfer") : i18n("direct thermal"), info.printMode().toLower());
    }
    if (m_queueStatus.activeJobs > 0) {
        lines << i18np("1 job waiting in the queue", "%1 jobs waiting in the queue", m_queueStatus.activeJobs);
    }
    QPalette palette = m_printerStatus->palette();
    KColorScheme::adjustForeground(palette, problems.isEmpty() ? KColorScheme::NormalText : KColorScheme::NegativeText, QPalette::WindowText);
    m_printerStatus->setPalette(palette);
    m_printerStatus->setForegroundRole(problems.isEmpty() && !m_zplInfo && network ? QPalette::PlaceholderText : QPalette::WindowText);
    m_printerStatus->setText(lines.join(u'\n'));
    m_printerStatus->setToolTip(tooltip);
    m_printerStatus->setVisible(!lines.isEmpty());
}

bool PrintBar::isDetecting() const
{
    return !m_detecting.isEmpty() && m_detecting == printerName();
}

std::optional<qreal> PrintBar::loadedLabelLengthMm() const
{
    if (!m_zplInfo || m_zplPrinter != printerName() || m_zplInfo->continuous() || m_zplInfo->labelLengthDots <= 0) {
        return std::nullopt;
    }
    return m_zplInfo->labelSizeMm().height();
}
