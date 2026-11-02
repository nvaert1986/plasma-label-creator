/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printerstatusdialog.h"

#include <KColorScheme>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

enum Column {
    Printer,
    Queue,
    Jobs,
    Says,
};

QString stateText(const QueueStatus &queue)
{
    switch (queue.state) {
    case QueueStatus::State::Idle:
        return i18nc("printer queue state", "Idle");
    case QueueStatus::State::Printing:
        return i18nc("printer queue state", "Printing");
    case QueueStatus::State::Stopped:
        return i18nc("printer queue state", "Stopped");
    case QueueStatus::State::Unknown:
        break;
    }
    return i18nc("printer queue state", "Unknown");
}

} // namespace

PrinterStatusDialog::PrinterStatusDialog(QWidget *parent, bool askNow)
    : QDialog(parent)
{
    setWindowTitle(i18n("Printer Status"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18n("The label printers on this computer: their queues, and what network label printers (Altec, Zebra) say "
                                  "about themselves. Printers on USB cannot be asked; the system's printer settings manage the queues."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({i18n("Printer"), i18n("Queue"), i18n("Jobs"), i18n("The printer says")});
    m_table->horizontalHeader()->setSectionResizeMode(Says, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setWordWrap(true);
    layout->addWidget(m_table, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *again = buttons->addButton(i18n("Refresh"), QDialogButtonBox::ActionRole);
    again->setIcon(QIcon::fromTheme(u"view-refresh"_s));
    connect(again, &QPushButton::clicked, this, &PrinterStatusDialog::refresh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_timer = new QTimer(this);
    m_timer->setInterval(10 * 1000);
    connect(m_timer, &QTimer::timeout, this, &PrinterStatusDialog::refresh);
    resize(780, 320);
    if (askNow) {
        m_timer->start();
        refresh();
    }
}

PrinterStatusDialog::~PrinterStatusDialog()
{
    // Queries still running finish on their own; nothing reports back here.
    for (const QPointer<QThread> &thread : std::as_const(m_threads)) {
        if (thread) {
            thread->disconnect(this);
        }
    }
}

QList<PrinterInfo> PrinterStatusDialog::labelPrinters(const QList<PrinterInfo> &printers)
{
    QList<PrinterInfo> labels;
    for (const PrinterInfo &printer : printers) {
        if (printer.hasLabelDriver()) {
            labels << printer;
        }
    }
    return labels;
}

void PrinterStatusDialog::refresh()
{
    const QList<PrinterInfo> printers = labelPrinters(LabelPrinter::printers());
    QList<QueueStatus> queues;
    for (const PrinterInfo &printer : printers) {
        queues << LabelPrinter::queueStatus(printer.name);
    }
    showPrinters(printers, queues);
    askNetworkPrinters();
}

void PrinterStatusDialog::showPrinters(const QList<PrinterInfo> &printers, const QList<QueueStatus> &queues)
{
    m_printers = printers;
    m_queues = queues;
    m_table->setRowCount(int(printers.size()));
    for (int row = 0; row < printers.size(); ++row) {
        showRow(row);
    }
    m_table->resizeColumnsToContents();
    m_table->resizeRowsToContents();
}

void PrinterStatusDialog::askNetworkPrinters()
{
    for (const PrinterInfo &printer : std::as_const(m_printers)) {
        const auto address = printer.speaksZpl() ? Zpl::networkAddress(printer.deviceUri) : std::nullopt;
        if (!address) {
            continue;
        }
        // Each in its own thread: a printer that does not answer holds up nothing.
        const QString name = printer.name;
        auto answer = std::make_shared<std::optional<ZplPrinterInfo>>();
        QThread *thread = QThread::create([address, answer]() {
            *answer = Zpl::query(address->first, address->second, 3000);
        });
        m_threads.removeAll(nullptr);
        m_threads << thread;
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        connect(thread, &QThread::finished, this, [this, name, answer]() {
            m_answers.insert(name, *answer);
            for (int row = 0; row < m_printers.size(); ++row) {
                if (m_printers.at(row).name == name) {
                    showRow(row);
                }
            }
            m_table->resizeRowsToContents();
        });
        thread->start();
    }
}

void PrinterStatusDialog::showRow(int row)
{
    const PrinterInfo &printer = m_printers.at(row);
    const QueueStatus &queue = m_queues.at(row);
    QStringList problems = queue.problems();
    QString says;
    const bool network = printer.speaksZpl() && Zpl::networkAddress(printer.deviceUri);
    if (!network) {
        says = i18n("Cannot be asked (not a network ZPL printer)");
    } else if (!m_answers.contains(printer.name)) {
        says = i18n("Asking…");
    } else if (const auto &answer = m_answers.value(printer.name); !answer) {
        says = i18n("Did not answer");
        problems << i18n("The printer did not answer");
    } else {
        const QStringList own = answer->problems();
        problems += own;
        const QString labels = answer->continuous() ? i18n("continuous labels")
                                                    : i18nc("label length", "labels %1 mm long", QString::number(answer->labelSizeMm().height(), 'f', 1));
        says = own.isEmpty() ? i18nc("model, dpi, labels", "%1, %2 dpi, %3, ready", answer->model, qRound(answer->dpi()), labels)
                             : i18nc("model, problems", "%1: %2", answer->model, own.join(u", "_s));
    }
    QString queueText = stateText(queue);
    if (!queue.message.isEmpty()) {
        queueText += u" – "_s + queue.message;
    }
    const QStringList cells = {printer.description.isEmpty() ? printer.name : printer.description + u" ("_s + printer.name + u')',
                               queueText,
                               QString::number(queue.activeJobs),
                               says};
    const KColorScheme colours(QPalette::Active, KColorScheme::View);
    for (int column = 0; column < cells.size(); ++column) {
        auto *item = new QTableWidgetItem(cells.at(column));
        item->setToolTip(column == Printer ? printer.makeAndModel + u'\n' + printer.deviceUri : problems.join(u'\n'));
        if (!problems.isEmpty()) {
            item->setForeground(colours.foreground(KColorScheme::NegativeText));
        }
        if (column == Printer) {
            item->setIcon(QIcon::fromTheme(problems.isEmpty() ? u"emblem-ok-symbolic"_s : u"emblem-warning"_s));
        }
        m_table->setItem(row, column, item);
    }
}
