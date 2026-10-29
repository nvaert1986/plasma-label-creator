/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "zplprinter.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThread>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

// Answers come framed as STX ... ETX, one or more per query.
QList<QByteArray> frames(const QByteArray &answer)
{
    QList<QByteArray> result;
    qsizetype start = answer.indexOf('\x02');
    while (start >= 0) {
        const qsizetype end = answer.indexOf('\x03', start + 1);
        if (end < 0) {
            break;
        }
        result << answer.mid(start + 1, end - start - 1);
        start = answer.indexOf('\x02', end + 1);
    }
    return result;
}

// The leading number of a setting such as "2400 DOTS"; 0 if there is none.
int leadingNumber(const QString &value)
{
    static const QRegularExpression number(u"^\\s*([+-]?\\d+)"_s);
    const QRegularExpressionMatch match = number.match(value);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

} // namespace

qreal ZplPrinterInfo::dpi() const
{
    // Printers report rounded dots per millimetre (8, 12, 23.6); the heads
    // are 203, 300 and 600 dpi.
    const qreal exact = dotsPerMm * 25.4;
    for (const qreal nominal : {203.0, 300.0, 600.0}) {
        if (qAbs(exact - nominal) <= nominal * 0.02) {
            return nominal;
        }
    }
    return exact;
}

int ZplPrinterInfo::printWidthDots() const
{
    return leadingNumber(configuration.value(u"PRINT WIDTH"_s));
}

QSizeF ZplPrinterInfo::labelSizeMm() const
{
    if (dotsPerMm <= 0) {
        return {};
    }
    return QSizeF(printWidthDots() / dotsPerMm, labelLengthDots / dotsPerMm);
}

bool ZplPrinterInfo::continuous() const
{
    return configuration.value(u"MEDIA TYPE"_s).startsWith(u"CONTINUOUS"_s);
}

QString ZplPrinterInfo::printMode() const
{
    return configuration.value(u"PRINT MODE"_s);
}

QStringList ZplPrinterInfo::problems() const
{
    QStringList list;
    if (headOpen) {
        list << u"head open"_s;
    }
    if (paperOut) {
        list << u"out of labels"_s;
    }
    if (ribbonOut && thermalTransfer) {
        list << u"out of ribbon"_s;
    }
    if (paused) {
        list << u"paused"_s;
    }
    return list;
}

namespace Zpl
{

bool parseHostIdentification(const QByteArray &answer, ZplPrinterInfo *info)
{
    // "ATP-600Pro,A2.11.AT6,23.6,80768KB"
    const QList<QByteArray> parts = frames(answer);
    if (parts.isEmpty()) {
        return false;
    }
    const QStringList fields = QString::fromLatin1(parts.constFirst()).split(u',');
    if (fields.size() < 3) {
        return false;
    }
    bool ok = false;
    const qreal dotsPerMm = fields.at(2).trimmed().toDouble(&ok);
    if (!ok || dotsPerMm <= 0) {
        return false;
    }
    info->model = fields.at(0).trimmed();
    info->firmware = fields.at(1).trimmed();
    info->dotsPerMm = dotsPerMm;
    return true;
}

bool parseHostStatus(const QByteArray &answer, ZplPrinterInfo *info)
{
    // Zebra's ZPL manual, ~HS: three strings of comma-separated fields.
    //   1: communication, paper out, pause, label length (dots), formats in
    //      buffer, buffer full, diagnostic mode, partial format, unused,
    //      corrupt RAM, under temperature, over temperature
    //   2: function settings, unused, head up, ribbon out, thermal transfer,
    //      print mode, print width mode, label waiting, labels remaining,
    //      format while printing, graphics stored
    //   3: password, static RAM
    const QList<QByteArray> parts = frames(answer);
    if (parts.size() < 2) {
        return false;
    }
    const QStringList first = QString::fromLatin1(parts.at(0)).split(u',');
    const QStringList second = QString::fromLatin1(parts.at(1)).split(u',');
    if (first.size() < 4 || second.size() < 5) {
        return false;
    }
    const auto flag = [](const QString &field) {
        return field.trimmed() == u"1"_s;
    };
    info->paperOut = flag(first.at(1));
    info->paused = flag(first.at(2));
    info->labelLengthDots = first.at(3).trimmed().toInt();
    info->headOpen = flag(second.at(2));
    info->ribbonOut = flag(second.at(3));
    info->thermalTransfer = flag(second.at(4));
    return true;
}

QMap<QString, QString> parseConfiguration(const QByteArray &answer)
{
    // "  2400 DOTS           PRINT WIDTH       ": the value in the first 22
    // columns, the name after it. Fixed columns rather than splitting on
    // spaces, since values have spaces too ("A2.11.AT6    <-").
    constexpr int NameColumn = 22;
    QMap<QString, QString> settings;
    QByteArray text = answer;
    text.replace('\x02', '\n').replace('\x03', '\n');
    for (const QByteArray &raw : text.split('\n')) {
        const QString line = QString::fromLatin1(raw).remove(u'\r');
        const QString name = line.mid(NameColumn).trimmed();
        const QString value = line.left(NameColumn).trimmed();
        if (!name.isEmpty() && !value.isEmpty()) {
            settings.insert(name, value);
        }
    }
    return settings;
}

std::optional<std::pair<QString, quint16>> networkAddress(const QString &deviceUri)
{
    const QUrl url(deviceUri);
    if (url.scheme() != u"socket"_s || url.host().isEmpty()) {
        return std::nullopt;
    }
    return std::pair<QString, quint16>(url.host(), quint16(url.port(9100)));
}

std::optional<ZplPrinterInfo> query(const QString &host, quint16 port, int timeoutMs, QString *errorString)
{
    const auto fail = [errorString](const QString &message) -> std::optional<ZplPrinterInfo> {
        if (errorString) {
            *errorString = message;
        }
        return std::nullopt;
    };
    // Waits in short slices, so that a thread asking can be told to stop
    // (QThread::requestInterruption) when the app closes.
    constexpr int Slice = 100; // milliseconds
    const auto interrupted = []() {
        return QThread::currentThread()->isInterruptionRequested();
    };
    QTcpSocket socket;
    socket.connectToHost(host, port);
    QElapsedTimer connecting;
    connecting.start();
    while (!socket.waitForConnected(Slice)) {
        if (socket.state() == QAbstractSocket::UnconnectedState || connecting.elapsed() >= timeoutMs || interrupted()) {
            return fail(u"Could not reach %1:%2: %3"_s.arg(host).arg(port).arg(socket.errorString()));
        }
    }

    // Sends one query and reads until its last frame has arrived.
    const auto ask = [&socket, timeoutMs, &interrupted](const QByteArray &command, int frameCount) {
        socket.write(command);
        socket.flush();
        QByteArray answer;
        QElapsedTimer timer;
        timer.start();
        while (answer.count('\x03') < frameCount && timer.elapsed() < timeoutMs && !interrupted()) {
            if (socket.waitForReadyRead(Slice)) {
                answer += socket.readAll();
            } else if (socket.state() != QAbstractSocket::ConnectedState) {
                break;
            }
        }
        return answer;
    };

    ZplPrinterInfo info;
    if (!parseHostIdentification(ask("~HI", 1), &info)) {
        return fail(u"%1 did not answer as a ZPL printer"_s.arg(host));
    }
    parseHostStatus(ask("~HS", 3), &info);
    info.configuration = parseConfiguration(ask("^XA^HH^XZ", 1));
    socket.disconnectFromHost();
    return info;
}

} // namespace Zpl

} // namespace LabelCreator
