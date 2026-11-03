/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "settingsdialog.h"

#include "addons.h"
#include "automationpage.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>

using namespace Qt::Literals::StringLiterals;

namespace
{

QDoubleSpinBox *numberBox(double minimum, double maximum, int decimals, const QString &suffix)
{
    auto *box = new QDoubleSpinBox;
    box->setRange(minimum, maximum);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    return box;
}

QSpinBox *countBox(int minimum, int maximum, const QString &suffix = {})
{
    auto *box = new QSpinBox;
    box->setRange(minimum, maximum);
    box->setSuffix(suffix);
    return box;
}

QLabel *note(const QString &text)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setEnabled(false);
    return label;
}

// A page of the dialog, with a form on it.
QFormLayout *newPage(KPageDialog *dialog, const QString &name, const QString &header, const QString &icon)
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    KPageWidgetItem *item = dialog->addPage(page, name);
    item->setHeader(header);
    item->setIcon(QIcon::fromTheme(icon));
    return form;
}

// Settings an administrator fixed with KDE's Kiosk: shown, not changed.
void lockIfFixed(QWidget *widget, const char *key)
{
    if (AppSettings::isLocked(key)) {
        widget->setEnabled(false);
        widget->setToolTip(i18n("Set by your administrator"));
    }
}

} // namespace

SettingsDialog::SettingsDialog(const AppSettings &settings, QWidget *parent, const QString &automationFile)
    : KPageDialog(parent)
{
    setWindowTitle(i18n("Configure Plasma Label Creator"));
    setFaceType(KPageDialog::List);
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    const QString mm = i18nc("millimetres", " mm");

    QFormLayout *designing = newPage(this, i18n("Designing"), i18n("New objects and moving them"), u"draw-freehand"_s);
    m_fontFamily = new QFontComboBox;
    m_fontSize = numberBox(4, 200, 1, i18nc("points", " pt"));
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(m_fontFamily, 1);
    fontRow->addWidget(m_fontSize);
    designing->addRow(i18n("Font for new text:"), fontRow);
    m_nudge = numberBox(0.01, 20, 2, mm);
    designing->addRow(i18n("Arrow keys move by:"), m_nudge);
    m_largeNudge = numberBox(0.1, 100, 2, mm);
    designing->addRow(i18n("With Shift:"), m_largeNudge);
    m_turnStep = numberBox(1, 90, 1, i18nc("degrees", "°"));
    m_turnStep->setToolTip(i18n("Turning with Shift held, and Turn Left / Turn Right"));
    designing->addRow(i18n("Turn in steps of:"), m_turnStep);
    m_tape = new QComboBox;
    for (const int width : {6, 9, 12, 19, 24}) {
        m_tape->addItem(i18nc("tape width", "%1 mm", width), width);
    }
    designing->addRow(i18n("Tape to start with:"), m_tape);
    m_units = new QComboBox;
    m_units->addItem(i18n("Millimetres"), false);
    m_units->addItem(i18n("Inches"), true);
    designing->addRow(i18n("Rulers in:"), m_units);

    QFormLayout *printing = newPage(this, i18n("Printing"), i18n("Printers, copies and rolls"), u"document-print"_s);
    m_askPrinters = new QCheckBox(i18n("Ask network label printers how they are"));
    m_askPrinters->setToolTip(i18n("Altec and Zebra printers on the network: labels loaded, paper out, head open, paused"));
    printing->addRow(QString(), m_askPrinters);
    m_statusSeconds = countBox(5, 600, i18nc("seconds", " s"));
    printing->addRow(i18n("Ask again every:"), m_statusSeconds);
    connect(m_askPrinters, &QCheckBox::toggled, m_statusSeconds, &QWidget::setEnabled);
    m_warnNotReady = new QCheckBox(i18n("Warn before printing when the printer is not ready"));
    printing->addRow(QString(), m_warnNotReady);
    m_copies = countBox(1, 999);
    printing->addRow(i18n("Copies to start with:"), m_copies);
    m_acrossGap = numberBox(0, 50, 1, mm);
    m_acrossGap->setToolTip(i18n("Altec does not give it: measure a roll with labels across"));
    printing->addRow(i18n("Gap between Altec labels across:"), m_acrossGap);
    m_continuousLength = numberBox(5, 1000, 1, mm);
    printing->addRow(i18n("Continuous rolls start at:"), m_continuousLength);
    m_counterWait = countBox(1, 300, i18nc("seconds", " s"));
    m_counterWait->setToolTip(i18n("How long to wait while another computer prints with the same shared counter"));
    printing->addRow(i18n("Wait for a shared counter:"), m_counterWait);

    QFormLayout *printOnly = newPage(this, i18n("Print-Only Mode"), i18n("For people who only print"), u"document-print-direct"_s);
    m_printOnlyFolder = new QLineEdit;
    m_printOnlyFolder->setPlaceholderText(i18n("The folder used last"));
    auto *browse = new QPushButton(QIcon::fromTheme(u"folder-open"_s), QString());
    browse->setToolTip(i18n("Choose the folder"));
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString folder = QFileDialog::getExistingDirectory(this, i18n("Folder With Labels"), m_printOnlyFolder->text());
        if (!folder.isEmpty()) {
            m_printOnlyFolder->setText(folder);
        }
    });
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(m_printOnlyFolder, 1);
    folderRow->addWidget(browse);
    printOnly->addRow(i18n("Labels from:"), folderRow);
    m_saveCounters = new QCheckBox(i18n("Save counters in the label after printing"));
    printOnly->addRow(QString(), m_saveCounters);
    m_lockPrintOnly = new QCheckBox(i18n("Always start in print-only mode, with no way back to designing"));
    printOnly->addRow(QString(), m_lockPrintOnly);
    printOnly->addRow(note(i18n("To keep people from changing these settings too, an administrator can fix them with KDE's Kiosk "
                                "(LockPrintOnly[$i]=true in the [Settings] group of plasma-label-creatorrc).")));

    QFormLayout *history = newPage(this, i18n("History"), i18n("Versions, print history and recent layouts"), u"view-history"_s);
    m_versionsKept = countBox(0, 500);
    m_versionsKept->setSpecialValueText(i18nc("no versions kept", "None"));
    history->addRow(i18n("Versions kept per label:"), m_versionsKept);
    m_versionsNextToLabel = new QCheckBox(i18n("Next to the label, in a .versions folder"));
    m_versionsNextToLabel->setToolTip(i18n("They then go along with the label, also on a network share, for everyone who opens it there"));
    history->addRow(QString(), m_versionsNextToLabel);
    m_historyKept = countBox(0, 100000);
    m_historyKept->setSpecialValueText(i18nc("no print history", "None"));
    history->addRow(i18n("Prints kept in the history:"), m_historyKept);
    m_recentKept = countBox(1, 100);
    history->addRow(i18n("Recently used layouts kept:"), m_recentKept);

    m_automation = new AutomationPage(automationFile);
    auto *scroll = new QScrollArea;
    scroll->setWidget(m_automation);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    KPageWidgetItem *automation = addPage(scroll, i18n("Automation"));
    automation->setHeader(i18n("Printing for other programs, without anyone at the screen"));
    automation->setIcon(QIcon::fromTheme(u"system-run"_s));

    // Add-ons: each installed one, on unless switched off here.
    QFormLayout *addons = newPage(this, i18n("Add-ons"), i18n("Extra features from add-ons installed on this computer"), u"preferences-plugin"_s);
    m_addons = new QListWidget;
    const QStringList off = Addons::switchedOff();
    for (const KPluginMetaData &metaData : Addons::installed()) {
        auto *item = new QListWidgetItem(metaData.description().isEmpty() ? metaData.name() : u"%1: %2"_s.arg(metaData.name(), metaData.description()), m_addons);
        item->setData(Qt::UserRole, metaData.pluginId());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(off.contains(metaData.pluginId()) ? Qt::Unchecked : Qt::Checked);
    }
    addons->addRow(m_addons);
    addons->addRow(note(m_addons->count() == 0 ? i18n("No add-ons are installed. Plasma Label Creator needs none.")
                                               : i18n("Switching an add-on on or off takes effect when Plasma Label Creator starts again.")));

    const std::pair<QWidget *, const char *> keys[] = {{m_fontFamily, "TextFontFamily"},
                                                      {m_fontSize, "TextFontSize"},
                                                      {m_nudge, "NudgeMm"},
                                                      {m_largeNudge, "LargeNudgeMm"},
                                                      {m_turnStep, "TurnStep"},
                                                      {m_tape, "TapeMm"},
                                                      {m_units, "Inches"},
                                                      {m_askPrinters, "AskPrinters"},
                                                      {m_statusSeconds, "StatusSeconds"},
                                                      {m_warnNotReady, "WarnNotReady"},
                                                      {m_copies, "Copies"},
                                                      {m_acrossGap, "AcrossGapMm"},
                                                      {m_continuousLength, "ContinuousLengthMm"},
                                                      {m_counterWait, "CounterWaitSeconds"},
                                                      {m_printOnlyFolder, "PrintOnlyFolder"},
                                                      {m_saveCounters, "SaveCounters"},
                                                      {m_lockPrintOnly, "LockPrintOnly"},
                                                      {m_versionsKept, "VersionsKept"},
                                                      {m_versionsNextToLabel, "VersionsNextToLabel"},
                                                      {m_historyKept, "HistoryKept"},
                                                      {m_recentKept, "RecentLayoutsKept"}};
    showSettings(settings);
    for (const auto &[widget, key] : keys) {
        lockIfFixed(widget, key);
    }

    connect(button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this]() {
        showSettings(AppSettings());
    });
    // The automation settings are the service's: Restore Defaults leaves its
    // folders alone.
    const auto saveAutomation = [this]() {
        QString error;
        if (!m_automation->save(&error)) {
            KMessageBox::error(this, i18n("The automation settings could not be saved:\n%1", error));
        }
        // The add-ons switched off; ones not installed now stay as they were.
        QStringList listed;
        QStringList off;
        for (int i = 0; i < m_addons->count(); ++i) {
            const QString id = m_addons->item(i)->data(Qt::UserRole).toString();
            listed << id;
            if (m_addons->item(i)->checkState() == Qt::Unchecked) {
                off << id;
            }
        }
        for (const QString &id : Addons::switchedOff()) {
            if (!listed.contains(id)) {
                off << id;
            }
        }
        Addons::setSwitchedOff(off);
    };
    connect(button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this, saveAutomation]() {
        saveAutomation();
        Q_EMIT applied(this->settings());
    });
    connect(this, &QDialog::accepted, this, [this, saveAutomation]() {
        saveAutomation();
        Q_EMIT applied(this->settings());
    });
    resize(720, 560);
}

void SettingsDialog::showSettings(const AppSettings &settings)
{
    m_fontFamily->setCurrentFont(QFont(settings.textFont.family));
    m_fontFamilySet = settings.textFont.family;
    m_fontFamilyShown = m_fontFamily->currentFont().family();
    m_fontSize->setValue(settings.textFont.size);
    m_nudge->setValue(settings.nudgeMm);
    m_largeNudge->setValue(settings.largeNudgeMm);
    m_turnStep->setValue(settings.turnStep);
    m_tape->setCurrentIndex(std::max(0, m_tape->findData(settings.tapeMm)));
    m_units->setCurrentIndex(settings.inches ? 1 : 0);
    m_askPrinters->setChecked(settings.askPrinters);
    m_statusSeconds->setValue(settings.statusSeconds);
    m_statusSeconds->setEnabled(settings.askPrinters && !AppSettings::isLocked("StatusSeconds"));
    m_warnNotReady->setChecked(settings.warnNotReady);
    m_copies->setValue(settings.copies);
    m_acrossGap->setValue(settings.acrossGapMm);
    m_continuousLength->setValue(settings.continuousLengthMm);
    m_counterWait->setValue(settings.counterWaitSeconds);
    m_printOnlyFolder->setText(settings.printOnlyFolder);
    m_saveCounters->setChecked(settings.saveCounters);
    m_lockPrintOnly->setChecked(settings.lockPrintOnly);
    m_versionsKept->setValue(settings.versionsKept);
    m_versionsNextToLabel->setChecked(settings.versionsNextToLabel);
    m_historyKept->setValue(settings.historyKept);
    m_recentKept->setValue(settings.recentLayoutsKept);
}

AppSettings SettingsDialog::settings() const
{
    AppSettings s;
    // A font not installed here shows as another in the list: unless a
    // family was picked, the one set stays.
    const QString shown = m_fontFamily->currentFont().family();
    s.textFont.family = shown == m_fontFamilyShown ? m_fontFamilySet : shown;
    s.textFont.size = m_fontSize->value();
    s.nudgeMm = m_nudge->value();
    s.largeNudgeMm = m_largeNudge->value();
    s.turnStep = m_turnStep->value();
    s.tapeMm = m_tape->currentData().toInt();
    s.inches = m_units->currentData().toBool();
    s.askPrinters = m_askPrinters->isChecked();
    s.statusSeconds = m_statusSeconds->value();
    s.warnNotReady = m_warnNotReady->isChecked();
    s.copies = m_copies->value();
    s.acrossGapMm = m_acrossGap->value();
    s.continuousLengthMm = m_continuousLength->value();
    s.counterWaitSeconds = m_counterWait->value();
    s.printOnlyFolder = m_printOnlyFolder->text().trimmed();
    s.saveCounters = m_saveCounters->isChecked();
    s.lockPrintOnly = m_lockPrintOnly->isChecked();
    s.versionsKept = m_versionsKept->value();
    s.versionsNextToLabel = m_versionsNextToLabel->isChecked();
    s.historyKept = m_historyKept->value();
    s.recentLayoutsKept = m_recentKept->value();
    return s;
}
