/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printbar.h"

#include "usbwatcher.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

// The bar itself, what to print and how many. The printers and their label
// types are in printbar_printers.cpp, their status in printbar_status.cpp.

PrintBar::PrintBar(QWidget *parent)
    : QWidget(parent)
{
    // Messages about printers being plugged in sit above the bar.
    m_usbMessage = new KMessageWidget;
    m_usbMessage->setWordWrap(true);
    m_usbMessage->setCloseButtonVisible(true);
    m_usbMessage->hide();

    // The printer, with a picture, as in DLS.
    auto *picture = new QLabel;
    picture->setPixmap(QIcon::fromTheme(u"printer"_s).pixmap(48, 48));
    m_printer = new QComboBox;
    m_printer->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_printer->setMinimumContentsLength(22);
    m_printerMenu = new QToolButton;
    m_printerMenu->setIcon(QIcon::fromTheme(u"configure"_s));
    m_printerMenu->setToolTip(i18n("Printer settings"));
    m_printerMenu->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_printerMenu);
    m_defaultAction = menu->addAction(QIcon::fromTheme(u"starred-symbolic"_s), i18n("Use as Default Printer"));
    m_defaultAction->setCheckable(true);
    m_defaultAction->setToolTip(i18n("Always start with this printer, even when another LabelWriter is plugged in"));
    m_modelAction = menu->addAction(QIcon::fromTheme(u"printer"_s), i18n("Which LabelWriter Is This…"));
    m_detectAction = menu->addAction(QIcon::fromTheme(u"view-refresh"_s), i18n("Detect the Loaded Labels Again"));
    m_detectAction->setToolTip(i18n("Ask the printer over the network which labels are loaded"));
    m_offsetAction = menu->addAction(QIcon::fromTheme(u"transform-move"_s), i18n("Print Offset…"));
    m_offsetAction->setToolTip(i18n("Move everything this printer prints a little, when labels come out shifted"));
    menu->addSeparator();
    m_statusAction = menu->addAction(QIcon::fromTheme(u"printer-symbolic"_s, QIcon::fromTheme(u"printer"_s)), i18n("Printer Status…"));
    m_statusAction->setToolTip(i18n("How every label printer is: its queue, jobs waiting, and what network printers say"));
    connect(m_statusAction, &QAction::triggered, this, &PrintBar::printerStatusRequested);
    m_printerMenu->setMenu(menu);
    m_graphics = new QCheckBox(i18n("Barcodes and graphics quality"));
    m_graphics->setToolTip(i18n("Prints slower but sharper, as DLS's \"Barcodes and graphics\" print quality."));
    auto *printerRow = new QHBoxLayout;
    printerRow->addWidget(m_printer, 1);
    printerRow->addWidget(m_printerMenu);
    // What the queue, and a network label printer, say about themselves:
    // asked when the printer is chosen and every half minute after.
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(30 * 1000);
    connect(m_statusTimer, &QTimer::timeout, this, &PrintBar::refreshStatus);
    m_statusTimer->start();
    m_printerStatus = new QLabel;
    m_printerStatus->setWordWrap(true);
    m_printerStatus->hide();
    auto *printerColumn = new QVBoxLayout;
    printerColumn->addLayout(printerRow);
    printerColumn->addWidget(m_printerStatus);
    printerColumn->addWidget(m_graphics);

    // What to print.
    m_printLabel = new QRadioButton(i18n("Only the label on the canvas"));
    m_printLabel->setChecked(true);
    m_printAddresses = new QRadioButton(i18n("All checked addresses in the address book"));
    m_printAddresses->setEnabled(false);
    m_printRows = new QRadioButton(i18n("Each row of the data source"));
    m_printRows->setEnabled(false);
    auto *whatColumn = new QVBoxLayout;
    whatColumn->addWidget(m_printLabel);
    whatColumn->addWidget(m_printAddresses);
    whatColumn->addWidget(m_printRows);

    // Copies and the Print button, large, on the right.
    m_copies = new QSpinBox;
    m_copies->setRange(1, 999);
    auto *copiesRow = new QHBoxLayout;
    copiesRow->addWidget(new QLabel(i18n("Copies:")));
    copiesRow->addWidget(m_copies);
    m_labelCount = new QLabel;
    m_labelCount->setAlignment(Qt::AlignRight);
    auto *copiesColumn = new QVBoxLayout;
    copiesColumn->addStretch();
    copiesColumn->addLayout(copiesRow);
    copiesColumn->addWidget(m_labelCount);
    m_print = new QPushButton(QIcon::fromTheme(u"document-print"_s), i18n("Print"));
    m_print->setIconSize(QSize(32, 32));
    m_print->setMinimumSize(140, 56);
    QFont big = m_print->font();
    big.setPointSizeF(big.pointSizeF() * 1.4);
    big.setBold(true);
    m_print->setFont(big);
    m_print->setDefault(true);

    auto *row = new QHBoxLayout;
    row->addWidget(picture, 0, Qt::AlignTop);
    row->addLayout(printerColumn);
    row->addSpacing(12);
    row->addLayout(whatColumn);
    row->addStretch(1);
    row->addLayout(copiesColumn);
    row->addWidget(m_print);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 8);
    layout->addWidget(m_usbMessage);
    layout->addLayout(row);

    connect(m_copies, &QSpinBox::valueChanged, this, &PrintBar::updateLabelCount);
    connect(m_printAddresses, &QRadioButton::toggled, this, &PrintBar::updateLabelCount);
    connect(m_printRows, &QRadioButton::toggled, this, &PrintBar::updateLabelCount);
    connect(m_print, &QPushButton::clicked, this, &PrintBar::printRequested);
    connect(m_printer, &QComboBox::currentIndexChanged, this, [this]() {
        reloadPaperSizes();
        m_printerMenu->setEnabled(!printerName().isEmpty());
        updatePrinterMenu();
        m_zplInfo.reset();
        refreshStatus();
        Q_EMIT printerChanged(printerName());
    });
    // Choosing a queue without the DYMO driver asks which LabelWriter it is.
    connect(m_printer, &QComboBox::activated, this, [this]() {
        const QString name = printerName();
        if (!name.isEmpty() && printerDriver().isEmpty()) {
            for (const PrinterInfo &printer : std::as_const(m_printers)) {
                if (printer.name == name && !printer.hasLabelDriver() && !choosePrinterModel()) {
                    setPrinterName(m_previousPrinter);
                    return;
                }
            }
        }
        m_previousPrinter = printerName();
        Q_EMIT printerChosen(printerName());
    });
    connect(m_modelAction, &QAction::triggered, this, &PrintBar::choosePrinterModel);
    connect(m_detectAction, &QAction::triggered, this, &PrintBar::detectLabels);
    connect(m_offsetAction, &QAction::triggered, this, &PrintBar::printOffsetRequested);
    connect(m_defaultAction, &QAction::toggled, this, &PrintBar::setDefaultPrinter);

    m_drivers = LabelPrinter::installedDrivers();
    reloadPrinters();
    m_previousPrinter = printerName();
    updatePrinterMenu();
    updateLabelCount();

    m_usbWatcher = new UsbWatcher(this);
    connect(m_usbWatcher, &UsbWatcher::printerAdded, this, [this](const UsbLabelWriter &printer) {
        // A queue for it may have been added in the system settings meanwhile.
        reloadPrinters();
        handleUsbPrinter(printer);
    });
    connect(m_usbWatcher, &UsbWatcher::printerRemoved, this, [this](const UsbLabelWriter &printer) {
        if (m_usbMessage->property("sysPath").toString() == printer.sysPath) {
            m_usbMessage->animatedHide();
        }
    });
}

PrintBar::~PrintBar()
{
    for (const QPointer<QThread> &thread : std::as_const(m_detectThreads)) {
        if (thread) {
            thread->requestInterruption();
            thread->wait();
        }
    }
}

void PrintBar::setDefaultCopies(int copies)
{
    m_copies->setValue(std::clamp(copies, m_copies->minimum(), m_copies->maximum()));
}

namespace
{

KConfigGroup offsetSettings()
{
    return KSharedConfig::openConfig()->group(u"Print Offsets"_s);
}

} // namespace

QPointF PrintBar::printOffsetMm() const
{
    const QList<qreal> values = offsetSettings().readEntry(printerName(), QList<qreal>());
    return values.size() == 2 ? QPointF(values.at(0), values.at(1)) : QPointF();
}

void PrintBar::setPrintOffsetMm(const QPointF &offset)
{
    KConfigGroup group = offsetSettings();
    if (offset.isNull()) {
        group.deleteEntry(printerName());
    } else {
        group.writeEntry(printerName(), QList<qreal>{offset.x(), offset.y()});
    }
    group.sync();
}

int PrintBar::copies() const
{
    return m_copies->value();
}

bool PrintBar::graphicsQuality() const
{
    return m_graphics->isChecked();
}

bool PrintBar::printAddresses() const
{
    return m_printAddresses->isChecked() && m_printAddresses->isEnabled();
}

bool PrintBar::printRows() const
{
    return m_printRows->isChecked() && m_printRows->isEnabled();
}

void PrintBar::setDataRows(bool available, int rows)
{
    m_dataRows = rows;
    if (!available && m_printRows->isChecked()) {
        m_printLabel->setChecked(true);
    }
    m_printRows->setEnabled(available);
    m_printRows->setText(rows < 0 ? i18n("Each row of the data source")
                                  : i18np("The ticked row of the data source", "Each of the %1 ticked rows of the data source", rows));
    updateLabelCount();
}

void PrintBar::choosePrintRows()
{
    if (m_printRows->isEnabled()) {
        m_printRows->setChecked(true);
    }
}

void PrintBar::setCheckedAddresses(int count)
{
    m_checkedAddresses = count;
    if (count == 0) {
        m_printLabel->setChecked(true);
    }
    m_printAddresses->setEnabled(count > 0);
    m_printAddresses->setText(count > 0 ? i18np("The checked address in the address book", "All %1 checked addresses in the address book", count)
                                        : i18n("All checked addresses in the address book"));
    updateLabelCount();
}

void PrintBar::updateLabelCount()
{
    if (printRows() && m_dataRows < 0) {
        m_labelCount->setText(i18np("1 label per row", "%1 labels per row", m_copies->value()));
        return;
    }
    const int labels = m_copies->value() * (printAddresses() ? m_checkedAddresses : printRows() ? m_dataRows : 1);
    m_labelCount->setText(i18np("1 label will be printed", "%1 labels will be printed", labels));
}
