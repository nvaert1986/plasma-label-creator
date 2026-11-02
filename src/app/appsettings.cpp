/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "appsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::Literals::StringLiterals;

namespace
{

KConfigGroup group()
{
    return KSharedConfig::openConfig()->group(u"Settings"_s);
}

} // namespace

AppSettings AppSettings::load()
{
    const AppSettings defaults;
    const KConfigGroup settings = group();
    AppSettings s;
    s.textFont.family = settings.readEntry("TextFontFamily", defaults.textFont.family);
    s.textFont.size = settings.readEntry("TextFontSize", defaults.textFont.size);
    s.nudgeMm = settings.readEntry("NudgeMm", defaults.nudgeMm);
    s.largeNudgeMm = settings.readEntry("LargeNudgeMm", defaults.largeNudgeMm);
    s.turnStep = settings.readEntry("TurnStep", defaults.turnStep);
    s.tapeMm = settings.readEntry("TapeMm", defaults.tapeMm);
    s.inches = settings.readEntry("Inches", defaults.inches);
    s.askPrinters = settings.readEntry("AskPrinters", defaults.askPrinters);
    s.statusSeconds = settings.readEntry("StatusSeconds", defaults.statusSeconds);
    s.warnNotReady = settings.readEntry("WarnNotReady", defaults.warnNotReady);
    s.copies = settings.readEntry("Copies", defaults.copies);
    s.acrossGapMm = settings.readEntry("AcrossGapMm", defaults.acrossGapMm);
    s.continuousLengthMm = settings.readEntry("ContinuousLengthMm", defaults.continuousLengthMm);
    s.counterWaitSeconds = settings.readEntry("CounterWaitSeconds", defaults.counterWaitSeconds);
    s.printOnlyFolder = settings.readEntry("PrintOnlyFolder", defaults.printOnlyFolder);
    s.saveCounters = settings.readEntry("SaveCounters", defaults.saveCounters);
    s.lockPrintOnly = settings.readEntry("LockPrintOnly", defaults.lockPrintOnly);
    s.versionsKept = settings.readEntry("VersionsKept", defaults.versionsKept);
    s.versionsNextToLabel = settings.readEntry("VersionsNextToLabel", defaults.versionsNextToLabel);
    s.historyKept = settings.readEntry("HistoryKept", defaults.historyKept);
    s.recentLayoutsKept = settings.readEntry("RecentLayoutsKept", defaults.recentLayoutsKept);
    return s;
}

void AppSettings::save() const
{
    // Only what differs from the defaults is written, so a later version's
    // better defaults reach everyone who did not choose otherwise.
    const AppSettings defaults;
    KConfigGroup settings = group();
    const auto write = [&settings](const char *key, const auto &value, const auto &standard) {
        if (settings.isEntryImmutable(key)) {
            return; // fixed by an administrator
        }
        if (value == standard) {
            settings.revertToDefault(key);
        } else {
            settings.writeEntry(key, value);
        }
    };
    write("TextFontFamily", textFont.family, defaults.textFont.family);
    write("TextFontSize", textFont.size, defaults.textFont.size);
    write("NudgeMm", nudgeMm, defaults.nudgeMm);
    write("LargeNudgeMm", largeNudgeMm, defaults.largeNudgeMm);
    write("TurnStep", turnStep, defaults.turnStep);
    write("TapeMm", tapeMm, defaults.tapeMm);
    write("Inches", inches, defaults.inches);
    write("AskPrinters", askPrinters, defaults.askPrinters);
    write("StatusSeconds", statusSeconds, defaults.statusSeconds);
    write("WarnNotReady", warnNotReady, defaults.warnNotReady);
    write("Copies", copies, defaults.copies);
    write("AcrossGapMm", acrossGapMm, defaults.acrossGapMm);
    write("ContinuousLengthMm", continuousLengthMm, defaults.continuousLengthMm);
    write("CounterWaitSeconds", counterWaitSeconds, defaults.counterWaitSeconds);
    write("PrintOnlyFolder", printOnlyFolder, defaults.printOnlyFolder);
    write("SaveCounters", saveCounters, defaults.saveCounters);
    write("LockPrintOnly", lockPrintOnly, defaults.lockPrintOnly);
    write("VersionsKept", versionsKept, defaults.versionsKept);
    write("VersionsNextToLabel", versionsNextToLabel, defaults.versionsNextToLabel);
    write("HistoryKept", historyKept, defaults.historyKept);
    write("RecentLayoutsKept", recentLayoutsKept, defaults.recentLayoutsKept);
    settings.sync();
}

bool AppSettings::isLocked(const char *key)
{
    return group().isEntryImmutable(key);
}
