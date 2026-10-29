/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

class QSocketNotifier;
struct udev;
struct udev_device;
struct udev_monitor;

/** A DYMO LabelWriter connected over USB. */
struct UsbLabelWriter {
    QString sysPath; // identifies the device while it is connected
    QString manufacturer; // "DYMO"
    QString product; // "LabelWriter 450 Turbo"
    QString serial;

    /** The device URI CUPS's usb backend uses for it. */
    QString deviceUri() const;

    bool operator==(const UsbLabelWriter &) const = default;
};

/**
 * Watches for DYMO LabelWriters on USB, through udev, so they are noticed
 * when plugged in. Needs no special rights.
 */
class UsbWatcher : public QObject
{
    Q_OBJECT

public:
    explicit UsbWatcher(QObject *parent = nullptr);
    ~UsbWatcher() override;

    QList<UsbLabelWriter> connected() const;

    /** Whether the device URI of a CUPS queue points at @p printer. */
    static bool matches(const QString &deviceUri, const UsbLabelWriter &printer);

Q_SIGNALS:
    void printerAdded(const UsbLabelWriter &printer);
    void printerRemoved(const UsbLabelWriter &printer);

private:
    void readEvents();
    static std::optional<UsbLabelWriter> labelWriter(udev_device *device);

    udev *m_udev = nullptr;
    udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QList<UsbLabelWriter> m_connected;
};
