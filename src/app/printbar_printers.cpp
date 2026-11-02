/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printbar.h"

#include "alteclabels.h"
#include "customlabels.h"
#include "printersetup.h"
#include "usbwatcher.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QIcon>
#include <QInputDialog>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

// The print bar's printers: the list, the default printer, which LabelWriter
// a queue prints to, LabelWriters plugged in, and the label types.

namespace
{

// Which LabelWriter driver to use for queues without the DYMO driver.
KConfigGroup driverSettings()
{
    return KSharedConfig::openConfig()->group(u"Printer Drivers"_s);
}

} // namespace

void PrintBar::reloadPrinters()
{
    const QString current = printerName();
    m_printers = LabelPrinter::printers();
    // LabelWriters first, then every other queue, which can be told which
    // LabelWriter it is.
    std::stable_sort(m_printers.begin(), m_printers.end(), [](const PrinterInfo &a, const PrinterInfo &b) {
        return a.hasLabelDriver() && !b.hasLabelDriver();
    });

    const KConfigGroup drivers = driverSettings();
    const QString preferred = defaultPrinter();
    const QSignalBlocker blocker(m_printer);
    m_printer->clear();
    // Queues with the same description (the same printer on USB and shared
    // from Windows, say) say how they are connected.
    QHash<QString, int> titles;
    for (const PrinterInfo &printer : std::as_const(m_printers)) {
        ++titles[printer.description.isEmpty() ? printer.name : printer.description];
    }
    const auto connection = [](const PrinterInfo &printer) {
        const QString scheme = QUrl(printer.deviceUri).scheme();
        if (scheme == u"usb"_s) {
            return i18nc("printer connection", "USB");
        }
        if (scheme == u"smb"_s) {
            return i18nc("printer connection", "Windows share");
        }
        if (scheme == u"socket"_s || scheme == u"ipp"_s || scheme == u"ipps"_s || scheme == u"lpd"_s || scheme == u"dnssd"_s) {
            return i18nc("printer connection", "network");
        }
        return printer.name;
    };
    for (const PrinterInfo &printer : std::as_const(m_printers)) {
        QString title = printer.description.isEmpty() ? printer.name : printer.description;
        if (titles.value(title) > 1) {
            title = i18nc("printer, connection", "%1 (%2)", title, connection(printer));
        }
        // The default printer is marked with a star.
        const QIcon icon = QIcon::fromTheme(printer.name == preferred ? u"starred-symbolic"_s : u"printer"_s);
        if (printer.hasLabelDriver()) {
            m_printer->addItem(icon, title, printer.name);
            continue;
        }
        const QString driver = drivers.readEntry(printer.name, QString());
        QString model;
        for (const LabelWriterDriver &candidate : std::as_const(m_drivers)) {
            if (candidate.ppdName == driver) {
                model = candidate.modelName;
            }
        }
        m_printer->addItem(icon,
                           model.isEmpty() ? i18nc("printer name", "%1 (not set up as a LabelWriter)", title) : i18nc("printer name, model", "%1 (as %2)", title, model),
                           printer.name);
        m_printer->setItemData(m_printer->count() - 1, printer.deviceUri.isEmpty() ? printer.makeAndModel : printer.deviceUri, Qt::ToolTipRole);
    }
    const bool found = m_printer->count() > 0;
    if (!found) {
        m_printer->addItem(i18n("No printers found"));
    }
    m_printer->setEnabled(found);
    m_printerMenu->setEnabled(found);
    m_print->setEnabled(found);
    m_printer->setCurrentIndex(std::max(0, m_printer->findData(current)));
    reloadPaperSizes();
    updatePrinterMenu();
}

void PrintBar::selectStartupPrinter(const QString &lastUsed)
{
    setPrinterName(lastUsed);
    const QString preferred = defaultPrinter();
    if (hasPrinter(preferred)) {
        setPrinterName(preferred);
    }
    // Connected LabelWriters: selected when there is no default, else offered.
    for (const UsbLabelWriter &printer : m_usbWatcher->connected()) {
        handleUsbPrinter(printer);
    }
    m_previousPrinter = printerName();
    // Selecting the queue that was already current changed nothing, so
    // ask a network label printer about its labels here too.
    refreshStatus();
}

QString PrintBar::defaultPrinter() const
{
    return KSharedConfig::openConfig()->group(u"General"_s).readEntry("DefaultPrinter", QString());
}

bool PrintBar::hasPrinter(const QString &name) const
{
    return !name.isEmpty() && std::any_of(m_printers.cbegin(), m_printers.cend(), [&name](const PrinterInfo &printer) {
        return printer.name == name;
    });
}

void PrintBar::setDefaultPrinter(bool isDefault)
{
    const QString name = printerName();
    KConfigGroup general = KSharedConfig::openConfig()->group(u"General"_s);
    if (isDefault && !name.isEmpty()) {
        general.writeEntry("DefaultPrinter", name);
    } else if (!isDefault && defaultPrinter() == name) {
        general.deleteEntry("DefaultPrinter");
    } else {
        return;
    }
    general.sync();
    reloadPrinters(); // moves the star
}

void PrintBar::updatePrinterMenu()
{
    const QSignalBlocker blocker(m_defaultAction);
    m_defaultAction->setChecked(!printerName().isEmpty() && printerName() == defaultPrinter());
    m_defaultAction->setEnabled(!printerName().isEmpty());
    const PrinterInfo *printer = currentPrinter();
    // Queues with a label printer driver of their own need no model chosen.
    m_modelAction->setEnabled(printer && !printer->hasLabelDriver());
    m_detectAction->setVisible(printer && printer->speaksZpl() && Zpl::networkAddress(printer->deviceUri));
    m_offsetAction->setEnabled(printer);
    // DYMO's print quality, for DYMO's drivers only.
    m_graphics->setEnabled(printer && (printer->isLabelWriter() || !printerDriver().isEmpty()));
}

QString PrintBar::printerDriver() const
{
    const PrinterInfo *printer = currentPrinter();
    if (!printer || printer->hasLabelDriver()) {
        return {};
    }
    return driverSettings().readEntry(printer->name, QString());
}

const PrinterInfo *PrintBar::currentPrinter() const
{
    const QString name = printerName();
    for (const PrinterInfo &printer : m_printers) {
        if (printer.name == name) {
            return &printer;
        }
    }
    return nullptr;
}

bool PrintBar::choosePrinterModel()
{
    const QString name = printerName();
    PrinterInfo info;
    for (const PrinterInfo &printer : std::as_const(m_printers)) {
        if (printer.name == name) {
            info = printer;
        }
    }
    if (info.name.isEmpty()) {
        return false;
    }
    if (info.isLabelWriter()) {
        KMessageBox::information(this, i18n("%1 uses the DYMO driver for a %2 already.", name, info.makeAndModel));
        return true;
    }
    if (m_drivers.isEmpty()) {
        KMessageBox::error(this,
                           i18n("No DYMO LabelWriter drivers are installed. Install the DYMO CUPS drivers first: net-print/dymo-cups-drivers for the "
                                "LabelWriter 450 and older, net-print/dymo-lw5xx-cups-drivers for the 550 series."));
        return false;
    }

    QStringList models;
    for (const LabelWriterDriver &driver : std::as_const(m_drivers)) {
        models << driver.modelName;
    }
    // Suggest what the queue's description and address point at.
    int suggestion = 0;
    const QString assigned = driverSettings().readEntry(name, QString());
    const auto guess = LabelPrinter::guessDriver({info.makeAndModel, info.deviceUri, info.description, info.name}, m_drivers);
    for (int i = 0; i < m_drivers.size(); ++i) {
        if ((!assigned.isEmpty() && m_drivers.at(i).ppdName == assigned) || (assigned.isEmpty() && guess && m_drivers.at(i) == *guess)) {
            suggestion = i;
        }
    }
    bool ok = false;
    const QString model = QInputDialog::getItem(this,
                                                i18n("LabelWriter Model"),
                                                i18n("%1 does not use the DYMO driver. Which LabelWriter does it print to?\n"
                                                     "Labels are then prepared with that model's driver on this computer and sent to the printer as they are.",
                                                     info.description.isEmpty() ? name : info.description),
                                                models,
                                                suggestion,
                                                false,
                                                &ok);
    if (!ok) {
        return false;
    }
    const LabelWriterDriver &driver = m_drivers.at(models.indexOf(model));
    // The 550 series' driver has to talk to the printer while printing, which
    // a queue without the DYMO driver cannot pass on.
    if (LabelPrinter::needsDirectConnection(LabelPrinter::driverFilter(driver.ppdName))) {
        KMessageBox::error(this,
                           i18n("%1 cannot print through %2. Its DYMO driver reads the printer's status before every label, which only works "
                                "when the printer is set up with that driver itself: connect it over USB or the network and add it as a printer "
                                "with the \"%1\" driver.",
                                driver.modelName,
                                info.description.isEmpty() ? name : info.description),
                           i18n("LabelWriter Model"));
        return false;
    }
    KConfigGroup settings = driverSettings();
    settings.writeEntry(name, driver.ppdName);
    settings.sync();
    reloadPrinters();
    return true;
}

void PrintBar::handleUsbPrinter(const UsbLabelWriter &usb)
{
    // A queue for it already: select it, unless the user chose a default
    // printer, which then wins; offer to switch instead.
    for (const PrinterInfo &printer : std::as_const(m_printers)) {
        if (UsbWatcher::matches(printer.deviceUri, usb)) {
            const QString preferred = defaultPrinter();
            const bool keepDefault = hasPrinter(preferred) && preferred != printer.name;
            m_usbMessage->setProperty("sysPath", usb.sysPath);
            m_usbMessage->setMessageType(KMessageWidget::Positive);
            m_usbMessage->setText(i18n("%1 is connected.", usb.product));
            for (QAction *action : m_usbMessage->actions()) {
                m_usbMessage->removeAction(action);
            }
            if (keepDefault) {
                auto *use = new QAction(QIcon::fromTheme(u"printer"_s), i18n("Use It"), m_usbMessage);
                const QString name = printer.name;
                connect(use, &QAction::triggered, this, [this, name]() {
                    setPrinterName(name);
                    m_previousPrinter = name;
                    m_usbMessage->animatedHide();
                });
                m_usbMessage->addAction(use);
                m_usbMessage->animatedShow();
            } else {
                setPrinterName(printer.name);
                m_previousPrinter = printer.name;
                m_usbMessage->animatedShow();
                QTimer::singleShot(5000, m_usbMessage, &KMessageWidget::animatedHide);
            }
            return;
        }
    }

    // None yet: offer to set one up.
    m_usbMessage->setProperty("sysPath", usb.sysPath);
    for (QAction *action : m_usbMessage->actions()) {
        m_usbMessage->removeAction(action);
    }
    if (!LabelPrinter::guessDriver({usb.product}, m_drivers)) {
        m_usbMessage->setMessageType(KMessageWidget::Warning);
        m_usbMessage->setText(i18n("%1 is connected, but no driver for it is installed. The LabelWriter 550 series needs DYMO's LW5xx driver "
                                   "(net-print/dymo-lw5xx-cups-drivers), the 450 and older net-print/dymo-cups-drivers.",
                                   usb.product));
    } else {
        m_usbMessage->setMessageType(KMessageWidget::Information);
        m_usbMessage->setText(i18n("%1 is connected, but not set up as a printer yet.", usb.product));
        auto *setUp = new QAction(QIcon::fromTheme(u"list-add"_s), i18n("Set Up Printer"), m_usbMessage);
        connect(setUp, &QAction::triggered, this, [this, usb]() {
            setUpUsbPrinter(usb);
        });
        m_usbMessage->addAction(setUp);
    }
    m_usbMessage->animatedShow();
}

void PrintBar::setUpUsbPrinter(const UsbLabelWriter &usb)
{
    const auto driver = LabelPrinter::guessDriver({usb.product}, m_drivers);
    if (!driver) {
        return;
    }
    QStringList taken;
    for (const PrinterInfo &printer : std::as_const(m_printers)) {
        taken << printer.name;
    }
    const QString name = PrinterSetup::queueName(usb.product, taken);
    for (QAction *action : m_usbMessage->actions()) {
        action->setEnabled(false);
    }
    m_usbMessage->setText(i18n("Setting up %1…", usb.product));
    PrinterSetup::addQueue(this, name, usb.deviceUri(), driver->ppdName, driver->modelName, [this, name, usb](const QString &error) {
        if (!error.isEmpty()) {
            m_usbMessage->setMessageType(KMessageWidget::Error);
            m_usbMessage->setText(i18n("%1 could not be set up: %2", usb.product, error));
            for (QAction *action : m_usbMessage->actions()) {
                action->setEnabled(true);
            }
            return;
        }
        reloadPrinters();
        setPrinterName(name);
        m_usbMessage->setMessageType(KMessageWidget::Positive);
        m_usbMessage->setText(i18n("%1 is set up as printer %2.", usb.product, name));
        for (QAction *action : m_usbMessage->actions()) {
            m_usbMessage->removeAction(action);
        }
        QTimer::singleShot(5000, m_usbMessage, &KMessageWidget::animatedHide);
    });
}

void PrintBar::reloadPaperSizes()
{
    m_paperSizes.clear();
    m_acceptsCustomSizes = false;
    const PrinterInfo *queue = currentPrinter();
    const bool detected = m_zplInfo && m_zplPrinter == printerName();
    // An Altec prints on Altec's labels, not the generic sizes of the
    // driver it uses (CUPS's Zebra ZPL driver): offer those, the ones that
    // fit the loaded labels first.
    if (queue && queue->hasLabelDriver() && !queue->isLabelWriter()
        && AltecLabels::isAltec((detected ? m_zplInfo->model : QString()) + u' ' + queue->name + u' ' + queue->description)) {
        m_acceptsCustomSizes = true; // CUPS's Zebra ZPL driver takes any size
        const QList<PaperSize> custom = CustomLabels::all();
        if (detected && !m_zplInfo->continuous() && m_zplInfo->labelLengthDots > 0) {
            const QSizeF loaded = m_zplInfo->labelSizeMm();
            for (PaperSize paper : AltecLabels::fitting(custom + AltecLabels::all(), loaded.height())) {
                paper.group = i18n("Fit the loaded labels (%1 mm long)", QString::number(loaded.height(), 'f', 1));
                m_paperSizes << paper;
            }
        }
        m_paperSizes << custom << AltecLabels::all();
        Q_EMIT paperSizesChanged(m_paperSizes);
        return;
    }
    if (!printerName().isEmpty()) {
        LabelPrinter printer(printerName(), printerDriver());
        if (printer.load()) {
            // Drivers can list one label under several keywords; show it once.
            QSet<QString> seen;
            for (const PaperSize &paper : printer.paperSizes()) {
                if (!seen.contains(paper.name)) {
                    seen.insert(paper.name);
                    m_paperSizes.append(paper);
                }
            }
            // A LabelManager prints tape: one label type per tape width.
            if (queue && queue->isLabelManager()) {
                m_paperSizes = LabelPrinter::tapeTypes(m_paperSizes);
            }
            // Label printers that print any size also get the user's own.
            m_acceptsCustomSizes = printer.supportsCustomSizes() && printerDriver().isEmpty();
            if (m_acceptsCustomSizes) {
                m_paperSizes << CustomLabels::all();
            }
        }
    }
    Q_EMIT paperSizesChanged(m_paperSizes);
}

QString PrintBar::printerName() const
{
    return m_printer->currentData().toString();
}

void PrintBar::setPrinterName(const QString &name)
{
    const int index = m_printer->findData(name);
    if (index >= 0) {
        m_printer->setCurrentIndex(index);
    }
}

QList<PaperSize> PrintBar::paperSizes() const
{
    return m_paperSizes;
}

bool PrintBar::acceptsCustomSizes() const
{
    return m_acceptsCustomSizes;
}
