/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "zplprinter.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QThread>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// What an Altec ATP-600 Pro (600 dpi, ZPL emulation) answered on 2026-10-29.
QByteArray answer(const QString &name)
{
    QFile file(QStringLiteral(LABELCREATOR_TEST_DATA "/zpl/atp600pro-") + name + u".bin"_s);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Answers the three queries the way that printer did, from its own thread
// so the blocking query() can talk to it.
class FakePrinter : public QObject
{
public:
    FakePrinter()
    {
        // A child, so it moves to the printer's thread along with it.
        m_server.setParent(this);
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                const QByteArray command = socket->readAll();
                if (command.contains("~HI")) {
                    socket->write(answer(u"hi"_s));
                }
                if (command.contains("~HS")) {
                    socket->write(answer(u"hs"_s));
                }
                if (command.contains("^HH")) {
                    socket->write(answer(u"hh"_s));
                }
            });
        });
    }

    quint16 port() const
    {
        return m_server.serverPort();
    }

private:
    QTcpServer m_server;
};

} // namespace

class ZplPrinterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsTheIdentity()
    {
        ZplPrinterInfo info;
        QVERIFY(Zpl::parseHostIdentification(answer(u"hi"_s), &info));
        QCOMPARE(info.model, u"ATP-600Pro"_s);
        QCOMPARE(info.firmware, u"A2.11.AT6"_s);
        QCOMPARE(info.dotsPerMm, 23.6);
        QCOMPARE(info.dpi(), 600.0); // 23.6 dots/mm is the 600 dpi head
        info.dotsPerMm = 8;
        QCOMPARE(info.dpi(), 203.0);
        info.dotsPerMm = 12;
        QCOMPARE(info.dpi(), 300.0);
        QVERIFY(!Zpl::parseHostIdentification("garbage", &info));
    }

    void readsTheStatus()
    {
        ZplPrinterInfo info;
        info.dotsPerMm = 23.6;
        QVERIFY(Zpl::parseHostStatus(answer(u"hs"_s), &info));
        QCOMPARE(info.labelLengthDots, 980);
        QVERIFY(!info.paperOut);
        QVERIFY(!info.paused);
        QVERIFY(!info.headOpen);
        QVERIFY(!info.ribbonOut);
        QVERIFY(info.thermalTransfer);
        QVERIFY(info.problems().isEmpty());

        // Head open and out of labels, as ~HS would flag them.
        QVERIFY(Zpl::parseHostStatus("\x02" "030,1,0,0980,000,0,0,0,000,0,0,0\x03\r\n\x02" "001,0,1,0,1,2,0,0,00000000,1,000\x03\r\n", &info));
        QVERIFY(info.paperOut);
        QVERIFY(info.headOpen);
        QCOMPARE(info.problems(), (QStringList{u"head open"_s, u"out of labels"_s}));
    }

    void readsTheConfiguration()
    {
        const QMap<QString, QString> settings = Zpl::parseConfiguration(answer(u"hh"_s));
        QCOMPARE(settings.value(u"PRINT WIDTH"_s), u"2400 DOTS"_s);
        QCOMPARE(settings.value(u"LABEL LENGTH"_s), u"0980"_s);
        QCOMPARE(settings.value(u"MEDIA TYPE"_s), u"NON-CONTINUOUS"_s);
        QCOMPARE(settings.value(u"SENSOR TYPE"_s), u"WEB"_s);
        QCOMPARE(settings.value(u"PRINT MODE"_s), u"TEAR OFF"_s);
        QCOMPARE(settings.value(u"PRINT METHOD"_s), u"THERMAL-TRANS."_s);
        QCOMPARE(settings.value(u"FIRMWARE"_s), u"A2.11.AT6    <-"_s);

        ZplPrinterInfo info;
        info.dotsPerMm = 23.6;
        info.labelLengthDots = 980;
        info.configuration = settings;
        QCOMPARE(info.printWidthDots(), 2400);
        QVERIFY(!info.continuous());
        QCOMPARE(info.printMode(), u"TEAR OFF"_s);
        const QSizeF size = info.labelSizeMm();
        QVERIFY(qAbs(size.width() - 101.7) < 0.1);
        QVERIFY(qAbs(size.height() - 41.5) < 0.1);
    }

    void findsTheNetworkAddress()
    {
        QCOMPARE(Zpl::networkAddress(u"socket://192.0.2.10:9100"_s)->first, u"192.0.2.10"_s);
        QCOMPARE(Zpl::networkAddress(u"socket://printer.example"_s)->second, quint16(9100));
        QCOMPARE(Zpl::networkAddress(u"socket://192.0.2.10:9101"_s)->second, quint16(9101));
        QVERIFY(!Zpl::networkAddress(u"usb://Zebra/ZD420?serial=1"_s));
        QVERIFY(!Zpl::networkAddress(u"smb://192.0.2.20/DYMO"_s));
    }

    void asksOverTheNetwork()
    {
        QThread thread;
        auto *printer = new FakePrinter;
        printer->moveToThread(&thread);
        connect(&thread, &QThread::finished, printer, &QObject::deleteLater);
        thread.start();

        QString error;
        const auto info = Zpl::query(u"127.0.0.1"_s, printer->port(), 2000, &error);
        QVERIFY2(info, qPrintable(error));
        QCOMPARE(info->model, u"ATP-600Pro"_s);
        QCOMPARE(info->labelLengthDots, 980);
        QCOMPARE(info->printWidthDots(), 2400);

        thread.quit();
        thread.wait();
    }

    void stopsWhenAsked()
    {
        // A printer that accepts the connection but never answers: the query
        // would wait 3 s per question; asked to stop, it ends at once.
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        const quint16 port = silent.serverPort();
        QElapsedTimer timer;
        timer.start();
        QThread *thread = QThread::create([port]() {
            Zpl::query(u"127.0.0.1"_s, port, 3000);
        });
        thread->start();
        QTest::qWait(300);
        thread->requestInterruption();
        QVERIFY(thread->wait(2000));
        QVERIFY(timer.elapsed() < 1500);
        delete thread;
    }

    void saysWhenNothingAnswers()
    {
        // A closed port on this machine: refused at once.
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost));
        const quint16 port = closed.serverPort();
        closed.close();
        QString error;
        QVERIFY(!Zpl::query(u"127.0.0.1"_s, port, 1000, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ZplPrinterTest)

#include "zplprintertest.moc"
