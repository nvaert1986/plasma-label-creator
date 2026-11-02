/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "appsettings.h"

#include <KPageDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QLineEdit;
class QSpinBox;

/** Settings > Configure Plasma Label Creator: designing, printing, print-only mode, history. */
class SettingsDialog : public KPageDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings &settings, QWidget *parent = nullptr);

    AppSettings settings() const;
    void showSettings(const AppSettings &settings);

Q_SIGNALS:
    /** OK or Apply: the settings to use and keep. */
    void applied(const AppSettings &settings);

private:
    // Designing
    QFontComboBox *m_fontFamily;
    QDoubleSpinBox *m_fontSize;
    QDoubleSpinBox *m_nudge;
    QDoubleSpinBox *m_largeNudge;
    QDoubleSpinBox *m_turnStep;
    QComboBox *m_tape;
    QComboBox *m_units;
    // Printing
    QCheckBox *m_askPrinters;
    QSpinBox *m_statusSeconds;
    QCheckBox *m_warnNotReady;
    QSpinBox *m_copies;
    QDoubleSpinBox *m_acrossGap;
    QDoubleSpinBox *m_continuousLength;
    QSpinBox *m_counterWait;
    // Print-only mode
    QLineEdit *m_printOnlyFolder;
    QCheckBox *m_saveCounters;
    QCheckBox *m_lockPrintOnly;
    // History and versions
    QSpinBox *m_versionsKept;
    QCheckBox *m_versionsNextToLabel;
    QSpinBox *m_historyKept;
    QSpinBox *m_recentKept;
};
