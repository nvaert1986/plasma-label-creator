/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QString>

/**
 * The settings of Settings > Configure Plasma Label Creator, kept in the
 * [Settings] group of plasma-label-creatorrc. An administrator can fix any
 * of them with KDE's Kiosk ([$i]); the dialog then shows them locked.
 */
struct AppSettings {
    // Designing
    LabelCreator::LabelFont textFont; // for new texts; Arial 12
    double nudgeMm = 0.35; // the arrow keys: one point
    double largeNudgeMm = 2.54; // with Shift: a tenth of an inch
    double turnStep = 15; // degrees, with Shift and Turn Left/Right
    int tapeMm = 12; // the tape a LabelManager label starts with
    bool inches = false; // rulers in inches

    // Printing
    bool askPrinters = true; // network ZPL printers, on their own
    int statusSeconds = 30;
    bool warnNotReady = true;
    int copies = 1;
    double acrossGapMm = 2;
    double continuousLengthMm = 50;
    int counterWaitSeconds = 10; // for a shared counter's lock

    // Print-only mode
    QString printOnlyFolder; // empty: the folder used last
    bool saveCounters = true;
    bool lockPrintOnly = false; // always print-only, no way back

    // History and versions
    int versionsKept = 20; // 0: none
    bool versionsNextToLabel = false;
    int historyKept = 5000; // 0: no history
    int recentLayoutsKept = 20;

    static AppSettings load();
    void save() const;
    /** Whether an administrator fixed setting @p key (KDE Kiosk). */
    static bool isLocked(const char *key);

    bool operator==(const AppSettings &) const = default;
};
