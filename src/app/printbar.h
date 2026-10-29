/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelprinter.h"
#include "zplprinter.h"

#include <QPointer>
#include <QWidget>

class KMessageWidget;
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QThread;
class QToolButton;
class UsbWatcher;
struct UsbLabelWriter;

/**
 * The print bar along the bottom of the window, as in DLS 8: the printer,
 * what to print, copies and a large Print button. It also keeps the list of
 * printers up to date, notices LabelWriters being plugged in and knows the
 * label types of the selected printer.
 */
class PrintBar : public QWidget
{
    Q_OBJECT

public:
    explicit PrintBar(QWidget *parent = nullptr);
    /** Stops asking printers about their labels, so no thread outlives the window. */
    ~PrintBar() override;

    QString printerName() const;
    void setPrinterName(const QString &name);
    /**
     * The LabelWriter driver to convert with for the selected queue, if that
     * queue does not have the DYMO driver itself; empty otherwise.
     */
    QString printerDriver() const;
    /**
     * Selects the printer to start with: the user's default printer if it
     * exists, else a LabelWriter connected over USB, else @p lastUsed.
     */
    void selectStartupPrinter(const QString &lastUsed);
    /** The printer the user chose as default in this app; empty if none. */
    QString defaultPrinter() const;

    /** The label types of the selected printer. */
    QList<LabelCreator::PaperSize> paperSizes() const;
    /**
     * How long the labels in the selected printer are along the feed, in
     * millimetres, if it was asked (ZPL printers on the network).
     */
    std::optional<qreal> loadedLabelLengthMm() const;
    /** Whether the selected printer is still being asked which labels are loaded. */
    bool isDetecting() const;
    /** Whether the selected printer prints label sizes it does not list (not a DYMO). */
    bool acceptsCustomSizes() const;
    /**
     * The selected printer's print offset in millimetres, in its own
     * directions: across the print head, then along the feed.
     */
    QPointF printOffsetMm() const;
    void setPrintOffsetMm(const QPointF &offset);

    int copies() const;
    bool graphicsQuality() const;
    /** Whether to print one label per checked address rather than the label as it is. */
    bool printAddresses() const;
    /** Whether to print a label for each ticked row of the data source. */
    bool printRows() const;
    /**
     * Offers printing from data: @p available when the label has a data
     * source; @p rows ticked rows, or -1 while they are not loaded yet.
     */
    void setDataRows(bool available, int rows);
    /** Chooses printing a label per row, when the data source allows it. */
    void choosePrintRows();
    /** How many addresses are checked in the address book. */
    void setCheckedAddresses(int count);
    /** Lists the label types of the selected printer again, as after adding a custom size. */
    void reloadPaperSizes();

Q_SIGNALS:
    void printRequested();
    void printerChanged(const QString &name);
    /** The user chose another printer in the list (not a change at start-up or from a plug-in). */
    void printerChosen(const QString &name);
    /** The user wants to set the selected printer's print offset. */
    void printOffsetRequested();
    void paperSizesChanged(const QList<LabelCreator::PaperSize> &paperSizes);

private:
    void reloadPrinters();
    /** Asks which LabelWriter the selected queue prints to; false if cancelled. */
    bool choosePrinterModel();
    void handleUsbPrinter(const UsbLabelWriter &printer);
    void setUpUsbPrinter(const UsbLabelWriter &printer);
    bool hasPrinter(const QString &name) const;
    void setDefaultPrinter(bool isDefault);
    void updatePrinterMenu();
    void updateLabelCount();
    /** The selected queue; nullptr when there is none. */
    const LabelCreator::PrinterInfo *currentPrinter() const;
    /**
     * Asks a ZPL printer on the network (Zebra, Altec) what it is and which
     * labels are loaded, in the background; the label list follows.
     */
    void detectLabels();
    void updatePrinterStatus(const QString &error);

    QComboBox *m_printer;
    QToolButton *m_printerMenu;
    QAction *m_modelAction;
    QAction *m_defaultAction;
    KMessageWidget *m_usbMessage;
    QRadioButton *m_printLabel;
    QRadioButton *m_printAddresses;
    QRadioButton *m_printRows;
    int m_dataRows = -1;
    QSpinBox *m_copies;
    QCheckBox *m_graphics;
    QPushButton *m_print;
    QLabel *m_labelCount;
    QLabel *m_printerStatus;
    QAction *m_detectAction;
    QAction *m_offsetAction;
    UsbWatcher *m_usbWatcher;

    QList<LabelCreator::PrinterInfo> m_printers;
    QList<LabelCreator::LabelWriterDriver> m_drivers;
    QList<LabelCreator::PaperSize> m_paperSizes;
    QString m_previousPrinter;
    int m_checkedAddresses = 0;
    // What the selected ZPL printer said about itself, if it was asked.
    std::optional<LabelCreator::ZplPrinterInfo> m_zplInfo;
    QString m_zplPrinter;
    QString m_detecting; // the printer being asked, if any
    QList<QPointer<QThread>> m_detectThreads;
    bool m_acceptsCustomSizes = false;
};
