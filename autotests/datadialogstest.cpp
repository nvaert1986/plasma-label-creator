/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "answersform.h"
#include "automationpage.h"
#include "datadialogs.h"
#include "gs1dialog.h"
#include "labelformat.h"
#include "historydialogs.h"
#include "exportdialog.h"
#include "layersdialog.h"
#include "settingsdialog.h"
#include "printerstatusdialog.h"
#include "printonlypanel.h"
#include "rolllayoutdialog.h"

#include <QDialogButtonBox>
#include <QRadioButton>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QStandardPaths>
#include <QFontComboBox>
#include <QGroupBox>
#include <QSignalSpy>
#include <QListWidget>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QDateEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// Saves a picture of @p widget when LABELCREATOR_TEST_GRAB names a folder.
void grab(QWidget *widget, const QString &name)
{
    if (qEnvironmentVariableIsSet("LABELCREATOR_TEST_GRAB")) {
        widget->grab().save(qEnvironmentVariable("LABELCREATOR_TEST_GRAB") + u'/' + name + u".png"_s);
    }
}

} // namespace

class DataDialogsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void answersMustPassTheirRules()
    {
        PrintQuestion batch;
        batch.name = u"Batch"_s;
        batch.prompt = u"Batch number"_s;
        batch.minimumLength = 4;
        batch.allowed = PrintQuestion::Allowed::Digits;
        PrintQuestion note;
        note.name = u"Note"_s;
        note.required = false;
        note.defaultValue = u"Fragile"_s;

        AnswersDialog dialog({batch, note}, {}, std::nullopt, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        const QList<QLineEdit *> fields = dialog.findChildren<QLineEdit *>();
        QCOMPARE(fields.size(), 2);
        QPushButton *print = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        // Empty and required: no printing yet.
        QVERIFY(!print->isEnabled());
        QTest::keyClicks(fields.at(0), u"12a"_s);
        QVERIFY(!print->isEnabled());
        fields.at(0)->clear();
        QTest::keyClicks(fields.at(0), u"1234"_s);
        QVERIFY(print->isEnabled());
        grab(&dialog, u"answers"_s);
        QCOMPARE(dialog.answers(), (QHash<QString, QString>{{u"Batch"_s, u"1234"_s}, {u"Note"_s, u"Fragile"_s}}));

        // Answers given before are offered again.
        AnswersDialog again({batch, note}, {{u"Batch"_s, u"5678"_s}}, std::nullopt, nullptr);
        QCOMPARE(again.answers().value(u"Batch"_s), u"5678"_s);
    }

    void formsHaveFieldsOfEveryKind()
    {
        const QLocale before;
        QLocale::setDefault(QLocale(u"nl_NL"_s));
        PrintQuestion serial;
        serial.name = u"Serial"_s;
        serial.lookupColumn = u"Serial"_s;
        PrintQuestion rack;
        rack.name = u"Rack"_s;
        rack.kind = PrintQuestion::Kind::Choice;
        rack.choicesColumn = u"Rack"_s;
        PrintQuestion size;
        size.name = u"Size"_s;
        size.kind = PrintQuestion::Kind::Choice;
        size.choices = {u"S"_s, u"M"_s, u"L"_s};
        size.defaultValue = u"M"_s;
        PrintQuestion weight;
        weight.name = u"Weight"_s;
        weight.kind = PrintQuestion::Kind::Number;
        weight.minimum = 0.5;
        weight.maximum = 20;
        weight.decimals = 1;
        weight.defaultValue = u"2,5"_s;
        PrintQuestion packed;
        packed.name = u"Packed"_s;
        packed.kind = PrintQuestion::Kind::Date;
        packed.dateFormat = u"yyyy-MM-dd"_s;
        PrintQuestion fragile;
        fragile.name = u"Fragile"_s;
        fragile.kind = PrintQuestion::Kind::Check;
        fragile.checkedText = u"FRAGILE"_s;
        fragile.uncheckedText = QString();
        fragile.required = false;

        DataTable data;
        data.columns = {u"Serial"_s, u"Rack"_s, u"Size"_s};
        data.rows = {{u"1001"_s, u"A"_s, u"L"_s}, {u"1002"_s, u"B"_s, u"S"_s}};
        AnswersForm form({serial, rack, size, weight, packed, fragile}, {}, data, nullptr);
        form.show();
        QVERIFY(QTest::qWaitForWindowExposed(&form));
        QCOMPARE(form.findChildren<QComboBox *>().at(0)->count(), 2); // racks from the data
        QHash<QString, QString> answers = form.answers();
        QCOMPARE(answers.value(u"Size"_s), u"M"_s);
        QCOMPARE(answers.value(u"Weight"_s), u"2,5"_s);
        QCOMPARE(answers.value(u"Packed"_s), QDate::currentDate().toString(u"yyyy-MM-dd"_s));
        QCOMPARE(answers.value(u"Fragile"_s), u"No"_s); // the default text when none is given
        QVERIFY(form.problem()); // no serial yet

        // A serial looked up fills the rack and size from its row.
        auto *serialField = form.findChildren<QLineEdit *>().constFirst();
        serialField->setText(u"1002"_s);
        Q_EMIT serialField->editingFinished();
        form.findChild<QCheckBox *>()->setChecked(true);
        grab(&form, u"form"_s);
        answers = form.answers();
        QCOMPARE(answers.value(u"Rack"_s), u"B"_s);
        QCOMPARE(answers.value(u"Size"_s), u"S"_s);
        QCOMPARE(answers.value(u"Fragile"_s), u"FRAGILE"_s);
        QVERIFY(!form.problem());
        QLocale::setDefault(before);
    }

    void questionKindsAreEdited()
    {
        PrintQuestion question;
        question.name = u"Size"_s;
        QuestionsDialog dialog({question}, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *kind = dialog.findChildren<QComboBox *>().at(0);
        QCOMPARE(kind->itemText(1), u"Drop-down list"_s);
        kind->setCurrentIndex(1);
        QCOMPARE(dialog.findChild<QStackedWidget *>()->currentIndex(), 1);
        dialog.findChild<QPlainTextEdit *>()->setPlainText(u"S\nM\n\nL"_s);
        grab(&dialog, u"question-kinds"_s);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.questions().constFirst().kind, PrintQuestion::Kind::Choice);
        QCOMPARE(dialog.questions().constFirst().choices, (QStringList{u"S"_s, u"M"_s, u"L"_s}));
    }

    void dataSourceLoadsAndTicksRows()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"assets.csv"_s);
        QFile csv(file);
        QVERIFY(csv.open(QIODevice::WriteOnly));
        csv.write("Name;Rack;Serial\nSRV-HV01;A;1001\nSRV-HV02;B;1002\nSRV-HV03;C;1003\n");
        csv.close();

        DataSource source;
        source.kind = DataSource::Kind::File;
        source.fileName = file;
        // Rows 0 and 2 ticked from before.
        DataSourceDialog dialog(source, DataReader::read(source), {0, 2}, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCOMPARE(dialog.source(), source);
        QCOMPARE(dialog.table()->rows.size(), 3);
        QCOMPARE(dialog.selectedRows(), (QList<int>{0, 2}));
        auto *rows = dialog.findChild<QTableWidget *>();
        QCOMPARE(rows->columnCount(), 4); // the tick, then the columns
        QCOMPARE(rows->horizontalHeaderItem(1)->text(), u"{Name}"_s);
        grab(&dialog, u"datasource"_s);

        // Loading again ticks every row.
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text().contains(u"Load"_s)) {
                button->click();
            }
        }
        QCOMPARE(dialog.selectedRows(), (QList<int>{0, 1, 2}));

        // None ticked before: none ticked now.
        DataSourceDialog none(source, DataReader::read(source), {}, nullptr);
        QCOMPARE(none.selectedRows(), QList<int>());
    }

    void layersAreEdited()
    {
        LayersDialog dialog({{u"Photo"_s, true, false, false}, {u"Old"_s, true, true, false}}, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *table = dialog.findChild<QTableWidget *>();
        QCOMPARE(table->rowCount(), 2);
        table->item(0, 0)->setText(u"Product photo"_s);
        table->item(0, 3)->setCheckState(Qt::Checked);
        table->setCurrentCell(1, 0);
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text().contains(u"Remove"_s)) {
                button->click();
            }
        }
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text().contains(u"Add"_s)) {
                button->click();
            }
        }
        grab(&dialog, u"layers"_s);
        const QList<Layer> layers = dialog.layers();
        QCOMPARE(layers.size(), 2);
        QCOMPARE(layers.at(0), (Layer{u"Product photo"_s, true, false, true}));
        QCOMPARE(layers.at(1).name, u"Layer 1"_s);
        QCOMPARE(dialog.renamed(), (QHash<QString, QString>{{u"Photo"_s, u"Product photo"_s}, {u"Old"_s, QString()}}));
    }

    void calculatedFieldsShowAnExample()
    {
        CalculatedFieldsDialog dialog({{u"Code"_s, u"{Prefix}-{Serial|pad:4}"_s}}, {{u"Prefix"_s, u"AB"_s}, {u"Serial"_s, u"12"_s}}, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *table = dialog.findChild<QTableWidget *>();
        QCOMPARE(table->item(0, 2)->text(), u"AB-0012"_s);
        // A second field builds on the first, and follows its changes.
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text().contains(u"Add"_s)) {
                button->click();
            }
        }
        table->item(1, 0)->setText(u"{Short}"_s);
        table->item(1, 1)->setText(u"{Code|right:4}"_s);
        QCOMPARE(table->item(1, 2)->text(), u"0012"_s);
        table->item(0, 1)->setText(u"{Prefix}{Serial|pad:5}"_s);
        QCOMPARE(table->item(1, 2)->text(), u"0012"_s);
        QCOMPARE(table->item(0, 2)->text(), u"AB00012"_s);
        grab(&dialog, u"calculated"_s);
        QCOMPARE(dialog.fields(), (QList<CalculatedField>{{u"Code"_s, u"{Prefix}{Serial|pad:5}"_s}, {u"Short"_s, u"{Code|right:4}"_s}}));
    }

    void gs1DataIsBuiltAndChecked()
    {
        Gs1Dialog dialog(u"[01]09501101530003[10]{Batch}"_s, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *table = dialog.findChild<QTableWidget *>();
        QCOMPARE(table->rowCount(), 2);
        QPushButton *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(ok->isEnabled());

        // An expiry date not written YYMMDD keeps OK off until it is.
        dialog.addElement({u"17"_s, u"31-12-26"_s});
        QVERIFY(!ok->isEnabled());
        auto *expiry = qobject_cast<QLineEdit *>(table->cellWidget(2, 1));
        expiry->setText(u"261231"_s);
        QVERIFY(ok->isEnabled());

        // A GTIN without its check digit gets it.
        qobject_cast<QLineEdit *>(table->cellWidget(0, 1))->setText(u"0950110153000"_s);
        QVERIFY(table->item(0, 2)->text().contains(u"check digit 3 added"_s));
        grab(&dialog, u"gs1"_s);
        QCOMPARE(dialog.data(), u"[01]09501101530003[10]{Batch}[17]261231"_s);
    }

    void rollLayoutIsChosen()
    {
        RollLayoutDialog dialog({}, 40, 14.4, true, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        const QList<QDoubleSpinBox *> spins = dialog.findChildren<QDoubleSpinBox *>();
        QCOMPARE(spins.size(), 2); // gap, length
        QVERIFY(!spins.at(0)->isEnabled() && !spins.at(1)->isEnabled());
        dialog.findChild<QSpinBox *>()->setValue(2);
        QVERIFY(spins.at(0)->isEnabled());
        spins.at(0)->setValue(3);
        grab(&dialog, u"roll"_s);
        QCOMPARE(dialog.roll(), (RollLayout{2, 3, false}));
        dialog.findChild<QCheckBox *>()->setChecked(true);
        QVERIFY(spins.at(1)->isEnabled());
        spins.at(1)->setValue(75);
        QCOMPARE(dialog.roll(), (RollLayout{2, 3, true}));
        QCOMPARE(dialog.lengthMm(), 75.0);
    }

    void printOnlyListsLabelsAndAsks()
    {
        QTemporaryDir dir;
        LabelDocument label;
        label.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
        TextObject text;
        text.name = u"TEXT"_s;
        text.spans = {{u"Batch {Batch}"_s, LabelFont(), Qt::black}};
        text.bounds = QRectF(100, 100, 3000, 600);
        label.objects << text;
        PrintQuestion batch;
        batch.name = u"Batch"_s;
        batch.minimumLength = 4;
        label.questions = {batch};
        QVERIFY(LabelWriter::writeFile(label, dir.filePath(u"batch.label"_s)));
        label.questions.clear();
        QVERIFY(LabelWriter::writeFile(label, dir.filePath(u"plain.label"_s)));
        QFile other(dir.filePath(u"notes.txt"_s));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();

        PrintOnlyPanel panel;
        panel.resize(340, 500);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.setFolder(dir.path());
        auto *labels = panel.findChild<QListWidget *>();
        QCOMPARE(labels->count(), 2); // the labels, not the notes
        QCOMPARE(labels->item(0)->text(), u"batch"_s);
        QSignalSpy chosen(&panel, &PrintOnlyPanel::labelChosen);
        Q_EMIT labels->itemClicked(labels->item(1));
        QCOMPARE(chosen.constFirst().constFirst().toString(), dir.filePath(u"plain.label"_s));

        // A label without questions has no form; one with them has.
        panel.setQuestions({}, {}, std::nullopt);
        QVERIFY(!panel.findChild<QGroupBox *>()->isVisible());
        panel.setQuestions({batch}, {{u"Batch"_s, u"12"_s}}, std::nullopt);
        QVERIFY(panel.findChild<QGroupBox *>()->isVisible());
        QVERIFY(panel.problem()); // too short
        QSignalSpy changed(&panel, &PrintOnlyPanel::answersChanged);
        panel.findChild<AnswersForm *>()->setAnswer(u"Batch"_s, u"1234"_s);
        QVERIFY(!changed.isEmpty());
        QVERIFY(!panel.problem());
        QCOMPARE(panel.answers().value(u"Batch"_s), u"1234"_s);
        grab(&panel, u"printonly-panel"_s);

        // Labels added to the folder show up.
        QVERIFY(LabelWriter::writeFile(label, dir.filePath(u"new.label"_s)));
        QTRY_COMPARE(labels->count(), 3);
    }

    void rowsOnlyLeavesTheSourceAlone()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"assets.csv"_s);
        QFile csv(file);
        QVERIFY(csv.open(QIODevice::WriteOnly));
        csv.write("Name\nA\nB\n");
        csv.close();
        DataSource source;
        source.kind = DataSource::Kind::File;
        source.fileName = file;
        DataSourceDialog dialog(source, DataReader::read(source), {1}, nullptr);
        dialog.setRowsOnly(true);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(!dialog.findChild<QLineEdit *>()->isVisible()); // the file name
        QCOMPARE(dialog.selectedRows(), QList<int>{1});
        QCOMPARE(dialog.source(), source);
    }

    void versionsAndHistoryAreShown()
    {
        QTemporaryDir dir;
        LabelDocument label;
        label.paperOutline.addRect(QRectF(0, 0, 1581, 5040));
        const QString older = dir.filePath(u"older.label"_s);
        const QString newer = dir.filePath(u"newer.label"_s);
        QVERIFY(LabelWriter::writeFile(label, older));
        QVERIFY(LabelWriter::writeFile(label, newer));
        const QDateTime now = QDateTime::currentDateTime();
        VersionsDialog versions(dir.filePath(u"rack.label"_s), {{newer, now}, {older, now.addDays(-1)}}, nullptr);
        versions.show();
        QVERIFY(QTest::qWaitForWindowExposed(&versions));
        QCOMPARE(versions.chosenVersion(), newer);
        versions.findChild<QListWidget *>()->setCurrentRow(1);
        QCOMPARE(versions.chosenVersion(), older);
        grab(&versions, u"versions"_s);

        PrintRecord first;
        first.when = now.addSecs(-60);
        first.title = u"rack.label"_s;
        first.printer = u"Altec"_s;
        first.labels = 3;
        first.rows = {{{u"Name"_s, u"SRV-HV01"_s}}};
        PrintRecord second = first;
        second.when = now;
        second.title = u"batch.label"_s;
        second.answers = {{u"Batch"_s, u"1234"_s}};
        second.rows.clear();
        PrintHistoryDialog history({second, first}, nullptr);
        history.show();
        QVERIFY(QTest::qWaitForWindowExposed(&history));
        auto *table = history.findChild<QTableWidget *>();
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 4)->text(), u"Batch: 1234"_s);
        // Searching also finds the data rows' values.
        history.findChild<QLineEdit *>()->setText(u"hv01"_s);
        QVERIFY(table->isRowHidden(0));
        QVERIFY(!table->isRowHidden(1));
        history.findChild<QLineEdit *>()->clear();
        grab(&history, u"history"_s);
        table->selectRow(1);
        for (QPushButton *button : history.findChildren<QPushButton *>()) {
            if (button->text().contains(u"Again"_s)) {
                button->click();
            }
        }
        QCOMPARE(history.action(), PrintHistoryDialog::Action::PrintAgain);
        QCOMPARE(history.chosenRecord(), first);
    }

    void printerStatusShowsQueuesAndProblems()
    {
        PrinterInfo altec;
        altec.name = u"AltecATP-600Pro"_s;
        altec.makeAndModel = u"Zebra ZPL Label Printer"_s;
        altec.deviceUri = u"socket://192.0.2.10:9100"_s;
        PrinterInfo usb = altec;
        usb.name = u"ATP-600_Pro-USB"_s;
        usb.deviceUri = u"usb://Altec/ATP-600%20Pro"_s;
        PrinterInfo office;
        office.name = u"Office"_s;
        office.makeAndModel = u"Brother MFC-L2700DW series"_s;
        // Only label printers.
        QCOMPARE(PrinterStatusDialog::labelPrinters({altec, office, usb}).size(), 2);

        PrinterStatusDialog dialog(nullptr, false);
        dialog.showPrinters({altec, usb},
                            {QueueStatus::fromOptions({{u"printer-state"_s, u"3"_s}}, 0),
                             QueueStatus::fromOptions({{u"printer-state"_s, u"5"_s}, {u"printer-state-reasons"_s, u"paused"_s}}, 2)});
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *table = dialog.findChild<QTableWidget *>();
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 1)->text(), u"Idle"_s);
        QCOMPARE(table->item(0, 3)->text(), u"Asking…"_s); // on the network
        QCOMPARE(table->item(1, 1)->text(), u"Stopped"_s);
        QCOMPARE(table->item(1, 2)->text(), u"2"_s);
        QVERIFY(table->item(1, 1)->toolTip().contains(u"paused"_s));
        QVERIFY(table->item(1, 3)->text().startsWith(u"Cannot be asked"_s)); // on USB
        grab(&dialog, u"printer-status"_s);
    }

    void settingsAreKeptAndShown()
    {
        QStandardPaths::setTestModeEnabled(true);
        KSharedConfig::openConfig()->deleteGroup(u"Settings"_s);
        QCOMPARE(AppSettings::load(), AppSettings()); // nothing set: the defaults

        AppSettings changed;
        changed.textFont = {u"DejaVu Sans"_s, 10};
        changed.turnStep = 5;
        changed.tapeMm = 19;
        changed.inches = true;
        changed.askPrinters = false;
        changed.acrossGapMm = 3.5;
        changed.lockPrintOnly = true;
        changed.versionsKept = 0;
        changed.historyKept = 100;
        changed.save();
        QCOMPARE(AppSettings::load(), changed);
        // Only what differs from the defaults is written.
        const KConfigGroup group = KSharedConfig::openConfig()->group(u"Settings"_s);
        QVERIFY(group.hasKey("TurnStep"));
        QVERIFY(!group.hasKey("Copies"));

        // The dialog shows them, restores the defaults, and hands back what is shown.
        SettingsDialog dialog(changed, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCOMPARE(dialog.settings().turnStep, 5.0);
        QCOMPARE(dialog.settings().tapeMm, 19);
        QVERIFY(dialog.settings().lockPrintOnly);
        grab(&dialog, u"settings"_s);
        // A font not installed here keeps its name, unless another is picked.
        AppSettings missing;
        missing.textFont.family = u"No Such Font Family"_s;
        SettingsDialog substitute(missing, nullptr);
        QCOMPARE(substitute.settings().textFont.family, u"No Such Font Family"_s);
        QSignalSpy applied(&dialog, &SettingsDialog::applied);
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(dialog.settings().turnStep, AppSettings().turnStep);
        QVERIFY(!dialog.settings().lockPrintOnly);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(applied.size(), 1);
        KSharedConfig::openConfig()->deleteGroup(u"Settings"_s);
    }

    void automationPageKeepsItsSettings()
    {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir dir;
        const QString file = dir.filePath(u"automation.json"_s);
        AutomationSettings settings;
        settings.folders << AutomationSettings::Folder{dir.path(), dir.filePath(u"box.label"_s), QString(), 2};
        settings.labelsFolder = dir.path();
        settings.httpPort = 9100;
        QVERIFY(settings.save(file));

        AutomationPage page(file);
        page.resize(680, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QCOMPARE(page.settings(), settings);
        auto *http = page.findChild<QGroupBox *>();
        for (QGroupBox *box : page.findChildren<QGroupBox *>()) {
            if (box->isCheckable()) {
                http = box;
            }
        }
        // Requests over HTTP: offered when built with Qt HTTP Server, and
        // never without a token.
        QCOMPARE(http->isEnabled(), AutomationPage::hasHttp());
        if (AutomationPage::hasHttp()) {
            http->setChecked(true);
            QVERIFY(page.settings().http);
            QVERIFY(page.settings().httpToken.size() >= 40);
        }
        grab(&page, u"automation"_s);
        QString error;
        QVERIFY2(page.save(&error), qPrintable(error));
        QCOMPARE(AutomationSettings::load(file), page.settings());
        // Not asked to start at log-in: no autostart entry.
        QVERIFY(!QFileInfo::exists(AutomationPage::autostartFile()));
    }

    void recentJobsAreListed()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath(u"jobs.jsonl"_s);
        RecentJobsDialog dialog(log, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *list = dialog.findChild<QTreeWidget *>();
        QCOMPARE(list->topLevelItemCount(), 0);
        AutomationJob printed{QDateTime::currentDateTime(), u"HTTP from 127.0.0.1"_s, u"box.label"_s, u"Zebra"_s, true, 3, 42, u"Sent 3 label(s)."_s};
        AutomationJob failed{QDateTime::currentDateTime(), u"Folder: /srv/jobs/rows.csv"_s, u"box.label"_s, u"Zebra"_s, false, 0, 0, u"Zebra reports: paused.\nmore"_s};
        QVERIFY(Automation::logJob(printed, log));
        QVERIFY(Automation::logJob(failed, log));
        // The service adds jobs while it is open: it follows.
        QTRY_COMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(list->topLevelItem(0)->text(5), u"Zebra reports: paused."_s);
        QCOMPARE(list->topLevelItem(0)->toolTip(5), u"Zebra reports: paused.\nmore"_s);
        QVERIFY(list->topLevelItem(1)->text(5).contains(u"42"_s));
        grab(&dialog, u"recent-jobs"_s);
    }

    void exportChoosesFormatAndOutline()
    {
        QTemporaryDir dir;
        ExportDialog dialog(dir.filePath(u"rack.pdf"_s), false, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(!dialog.png());
        QVERIFY(!dialog.outline());
        // PNG: the file name follows.
        dialog.findChildren<QRadioButton *>().at(1)->setChecked(true);
        QVERIFY(dialog.png());
        QCOMPARE(dialog.fileName(), dir.filePath(u"rack.png"_s));
        dialog.findChild<QCheckBox *>()->setChecked(true);
        QVERIFY(dialog.outline());
        grab(&dialog, u"export"_s);
        // No extension typed: the format's.
        dialog.findChild<QLineEdit *>()->setText(dir.filePath(u"proof"_s));
        QCOMPARE(dialog.fileName(), dir.filePath(u"proof.png"_s));
        // A dot in the name is not an extension; a name alone goes in the folder.
        dialog.findChild<QLineEdit *>()->setText(u"Kabel 2.5"_s);
        QCOMPARE(dialog.fileName(), dir.filePath(u"Kabel 2.5.png"_s));
        // An extension typed chooses the format, and the name stays as typed.
        QTest::keyClicks(dialog.findChild<QLineEdit *>(), u".pdf"_s);
        QVERIFY(!dialog.png());
        QCOMPARE(dialog.findChild<QLineEdit *>()->text(), u"Kabel 2.5.pdf"_s);
        QCOMPARE(dialog.fileName(), dir.filePath(u"Kabel 2.5.pdf"_s));
    }

    void questionsAreEdited()
    {
        PrintQuestion question;
        question.name = u"Batch"_s;
        QuestionsDialog dialog({question}, nullptr);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *name = dialog.findChildren<QLineEdit *>().at(0);
        name->setText(u"{Lot}"_s); // braces are dropped
        grab(&dialog, u"questions"_s);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.questions().size(), 1);
        QCOMPARE(dialog.questions().constFirst().name, u"Lot"_s);
    }
};

QTEST_MAIN(DataDialogsTest)

#include "datadialogstest.moc"
