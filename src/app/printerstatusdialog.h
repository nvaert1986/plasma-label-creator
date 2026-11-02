/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelprinter.h"
#include "zplprinter.h"

#include <QDialog>
#include <QHash>
#include <QPointer>

#include <optional>

class QTableWidget;
class QThread;
class QTimer;

/**
 * How every label printer is: its CUPS queue (state, jobs waiting, the
 * reasons it gives) and, for a ZPL printer on the network, what it says
 * itself (paper out, head open, paused, the labels loaded). Asked again
 * every ten seconds while open.
 */
class PrinterStatusDialog : public QDialog
{
    Q_OBJECT

public:
    /** @p askNow: ask CUPS and the printers straight away, and every ten seconds. */
    explicit PrinterStatusDialog(QWidget *parent = nullptr, bool askNow = true);
    ~PrinterStatusDialog() override;

    /** The label printers shown: the queues with a label printer's driver. */
    static QList<LabelCreator::PrinterInfo> labelPrinters(const QList<LabelCreator::PrinterInfo> &printers);

    /** Shows @p printers with their queues, as now; for tests, without asking CUPS. */
    void showPrinters(const QList<LabelCreator::PrinterInfo> &printers, const QList<LabelCreator::QueueStatus> &queues);

private:
    void refresh();
    void askNetworkPrinters();
    void showRow(int row);

    QTableWidget *m_table;
    QTimer *m_timer;
    QList<LabelCreator::PrinterInfo> m_printers;
    QList<LabelCreator::QueueStatus> m_queues;
    // What network ZPL printers said, by queue name; nullopt: no answer.
    QHash<QString, std::optional<LabelCreator::ZplPrinterInfo>> m_answers;
    QList<QPointer<QThread>> m_threads;
};
