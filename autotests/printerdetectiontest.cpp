/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelprinter.h"
#include "printersetup.h"
#include "usbwatcher.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;

namespace
{

UsbLabelWriter turbo(const QString &serial = u"01010112345600"_s)
{
    return {u"/sys/devices/usb1/1-2"_s, u"DYMO"_s, u"LabelWriter 450 Turbo"_s, serial};
}

} // namespace

class PrinterDetectionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deviceUriAsCupsBuildsIt()
    {
        QCOMPARE(turbo().deviceUri(), u"usb://DYMO/LabelWriter%20450%20Turbo?serial=01010112345600"_s);
        QCOMPARE(turbo(QString()).deviceUri(), u"usb://DYMO/LabelWriter%20450%20Turbo"_s);

        // As a LabelWriter 450 Turbo names itself over USB: the maker in
        // front (the watcher already folds its double space). CUPS's
        // address has the model alone.
        UsbLabelWriter real = turbo(u"02020212345600"_s);
        real.product = u"DYMO LabelWriter 450 Turbo"_s;
        QCOMPARE(real.deviceUri(), u"usb://DYMO/LabelWriter%20450%20Turbo?serial=02020212345600"_s);
    }

    void matchesQueues_data()
    {
        QTest::addColumn<QString>("deviceUri");
        QTest::addColumn<QString>("serial");
        QTest::addColumn<bool>("matches");
        QTest::newRow("same serial") << u"usb://DYMO/LabelWriter%20450%20Turbo?serial=01010112345600"_s << u"01010112345600"_s << true;
        QTest::newRow("other serial") << u"usb://DYMO/LabelWriter%20450%20Turbo?serial=99999"_s << u"01010112345600"_s << false;
        QTest::newRow("model only") << u"usb://DYMO/LabelWriter%20450%20Turbo"_s << u"01010112345600"_s << true;
        QTest::newRow("other model") << u"usb://DYMO/LabelWriter%20450"_s << QString() << false;
        QTest::newRow("network queue") << u"smb://192.0.2.1/DYMO"_s << u"01010112345600"_s << false;
    }

    void matchesQueues()
    {
        QFETCH(QString, deviceUri);
        QFETCH(QString, serial);
        QFETCH(bool, matches);
        QCOMPARE(UsbWatcher::matches(deviceUri, turbo(serial)), matches);
    }

    void recognisesLabelManagers()
    {
        // Tape printers as their queues and drivers name them.
        QVERIFY(LabelCreator::LabelPrinter::isLabelManagerModel(u"DYMO LabelMANAGER 450"_s));
        QVERIFY(LabelCreator::LabelPrinter::isLabelManagerModel(u"LabelManager 450D"_s));
        QVERIFY(LabelCreator::LabelPrinter::isLabelManagerModel(u"DYMO MobileLabeler"_s));
        QVERIFY(!LabelCreator::LabelPrinter::isLabelManagerModel(u"DYMO LabelWriter 450 Turbo"_s));
        LabelCreator::PrinterInfo queue;
        queue.makeAndModel = u"DYMO LabelMANAGER 450"_s;
        QVERIFY(queue.isLabelManager());
        QVERIFY(queue.hasLabelDriver()); // printed to directly, not asked which LabelWriter
        QVERIFY(!queue.isLabelWriter());
    }

    void queueNames()
    {
        QCOMPARE(PrinterSetup::queueName(u"LabelWriter 450 Turbo"_s, {}), u"DYMO-LabelWriter-450-Turbo"_s);
        QCOMPARE(PrinterSetup::queueName(u"DYMO LabelWriter 450"_s, {}), u"DYMO-LabelWriter-450"_s);
        QCOMPARE(PrinterSetup::queueName(u"LabelWriter 450 Turbo"_s, {u"DYMO-LabelWriter-450-Turbo"_s}), u"DYMO-LabelWriter-450-Turbo-2"_s);
    }

    void watcherStarts()
    {
        // Whatever is plugged in, starting the watcher must work without
        // special rights.
        UsbWatcher watcher;
        for (const UsbLabelWriter &printer : watcher.connected()) {
            QVERIFY(printer.product.contains(u"LabelWriter"_s) || printer.product.contains(u"LabelManager"_s, Qt::CaseInsensitive));
        }
    }
};

QTEST_GUILESS_MAIN(PrinterDetectionTest)

#include "printerdetectiontest.moc"
