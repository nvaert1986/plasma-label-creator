/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QByteArray>
#include <QMap>
#include <QSizeF>
#include <QString>

#include <optional>

namespace LabelCreator
{

/**
 * What a ZPL label printer on the network says about itself and the labels
 * in it. Zebra printers speak ZPL, and so do Altec's (TSC's) in their ZPL
 * emulation. All of it comes from queries that print nothing:
 * ~HI (identity), ~HS (status) and ^HH (configuration).
 */
struct ZplPrinterInfo {
    // ~HI
    QString model; // "ATP-600Pro"
    QString firmware; // "A2.11.AT6"
    qreal dotsPerMm = 0; // 8, 12 or 23.6 for 203, 300 or 600 dpi

    // ~HS
    bool paperOut = false;
    bool paused = false;
    bool headOpen = false;
    bool ribbonOut = false;
    bool thermalTransfer = false; // else direct thermal
    int labelLengthDots = 0; // as the printer measured or was told

    // ^HH, keyed by the printer's own names ("PRINT WIDTH": "2400 DOTS")
    QMap<QString, QString> configuration;

    qreal dpi() const;
    /** The print width the printer is set to, in dots; 0 if unknown. */
    int printWidthDots() const;
    /** The label length and print width in millimetres; empty when unknown. */
    QSizeF labelSizeMm() const;
    /** Continuous material rather than separate labels. */
    bool continuous() const;
    /** "TEAR OFF", "PEEL OFF", "CUTTER", ... */
    QString printMode() const;
    /** The problems that keep it from printing, for showing; empty when ready. */
    QStringList problems() const;
};

namespace Zpl
{

/** The answer to ~HI: model, firmware, dots per millimetre, memory. */
bool parseHostIdentification(const QByteArray &answer, ZplPrinterInfo *info);
/** The answer to ~HS: three framed strings of flags and numbers. */
bool parseHostStatus(const QByteArray &answer, ZplPrinterInfo *info);
/** The answer to ^HH: one setting per line, value first, name second. */
QMap<QString, QString> parseConfiguration(const QByteArray &answer);

/** The host and port of a socket://host:port device address; nullopt for others. */
std::optional<std::pair<QString, quint16>> networkAddress(const QString &deviceUri);

/**
 * Asks the printer at @p host for everything above over one TCP
 * connection. Blocks for up to about three times @p timeoutMs; run it off
 * the GUI thread.
 */
std::optional<ZplPrinterInfo> query(const QString &host, quint16 port = 9100, int timeoutMs = 3000, QString *errorString = nullptr);

} // namespace Zpl

} // namespace LabelCreator
