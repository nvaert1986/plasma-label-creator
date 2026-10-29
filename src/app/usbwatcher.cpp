/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "usbwatcher.h"

#include "labelprinter.h"

#include <QSocketNotifier>
#include <QUrl>
#include <QUrlQuery>

#include <libudev.h>

#include <optional>

using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr char DymoVendorId[] = "0922";

QString attribute(udev_device *device, const char *name)
{
    return QString::fromUtf8(udev_device_get_sysattr_value(device, name)).trimmed();
}

} // namespace

QString UsbLabelWriter::deviceUri() const
{
    // As CUPS's usb backend builds it from the printer's IEEE 1284 id:
    // usb://DYMO/LabelWriter%20450%20Turbo?serial=01010112345600
    // CUPS takes the model from the printer's IEEE 1284 id, without the
    // maker the USB product name starts with.
    QString model = product;
    if (model.startsWith(manufacturer + u' ', Qt::CaseInsensitive)) {
        model = model.mid(manufacturer.size() + 1);
    }
    QString uri = u"usb://"_s + QString::fromUtf8(QUrl::toPercentEncoding(manufacturer)) + u'/' + QString::fromUtf8(QUrl::toPercentEncoding(model));
    if (!serial.isEmpty()) {
        uri += u"?serial="_s + serial;
    }
    return uri;
}

UsbWatcher::UsbWatcher(QObject *parent)
    : QObject(parent)
    , m_udev(udev_new())
{
    if (!m_udev) {
        return;
    }

    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (m_monitor) {
        udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "usb", "usb_device");
        udev_monitor_enable_receiving(m_monitor);
        m_notifier = new QSocketNotifier(udev_monitor_get_fd(m_monitor), QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &UsbWatcher::readEvents);
    }

    // What is already plugged in.
    udev_enumerate *enumerate = udev_enumerate_new(m_udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "usb_device");
    udev_enumerate_add_match_sysattr(enumerate, "idVendor", DymoVendorId);
    udev_enumerate_scan_devices(enumerate);
    udev_list_entry *entry = nullptr;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate))
    {
        udev_device *device = udev_device_new_from_syspath(m_udev, udev_list_entry_get_name(entry));
        if (const auto printer = labelWriter(device)) {
            m_connected << *printer;
        }
        udev_device_unref(device);
    }
    udev_enumerate_unref(enumerate);
}

UsbWatcher::~UsbWatcher()
{
    if (m_monitor) {
        udev_monitor_unref(m_monitor);
    }
    if (m_udev) {
        udev_unref(m_udev);
    }
}

QList<UsbLabelWriter> UsbWatcher::connected() const
{
    return m_connected;
}

std::optional<UsbLabelWriter> UsbWatcher::labelWriter(udev_device *device)
{
    if (!device || attribute(device, "idVendor") != QLatin1String(DymoVendorId)) {
        return std::nullopt;
    }
    UsbLabelWriter printer;
    printer.sysPath = QString::fromUtf8(udev_device_get_syspath(device));
    printer.manufacturer = attribute(device, "manufacturer");
    // "DYMO LabelWriter 450  Turbo": the maker in front, and spaces doubled.
    printer.product = attribute(device, "product").simplified();
    printer.serial = attribute(device, "serial");
    // LabelWriters and LabelManagers; DYMO also makes scales.
    if (!printer.product.contains(u"LabelWriter"_s, Qt::CaseInsensitive) && !LabelCreator::LabelPrinter::isLabelManagerModel(printer.product)) {
        return std::nullopt;
    }
    if (printer.manufacturer.isEmpty()) {
        printer.manufacturer = u"DYMO"_s;
    }
    return printer;
}

void UsbWatcher::readEvents()
{
    while (udev_device *device = udev_monitor_receive_device(m_monitor)) {
        const QString action = QString::fromUtf8(udev_device_get_action(device));
        const QString sysPath = QString::fromUtf8(udev_device_get_syspath(device));
        if (action == u"add"_s) {
            if (const auto printer = labelWriter(device)) {
                m_connected << *printer;
                Q_EMIT printerAdded(*printer);
            }
        } else if (action == u"remove"_s) {
            // Removed devices no longer have their attributes; match by path.
            for (qsizetype i = 0; i < m_connected.size(); ++i) {
                if (m_connected.at(i).sysPath == sysPath) {
                    const UsbLabelWriter printer = m_connected.takeAt(i);
                    Q_EMIT printerRemoved(printer);
                    break;
                }
            }
        }
        udev_device_unref(device);
    }
}

bool UsbWatcher::matches(const QString &deviceUri, const UsbLabelWriter &printer)
{
    if (!deviceUri.startsWith(u"usb:"_s)) {
        return false;
    }
    const QUrl uri(deviceUri);
    const QString serial = QUrlQuery(uri).queryItemValue(u"serial"_s);
    if (!serial.isEmpty() && !printer.serial.isEmpty()) {
        return serial == printer.serial;
    }
    // Without serial numbers, the model has to do.
    return QUrl::fromPercentEncoding(uri.path().toUtf8()).contains(printer.product, Qt::CaseInsensitive);
}
