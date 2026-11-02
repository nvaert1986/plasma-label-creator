/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "answersform.h"
#include "datadialogs.h"
#include "gs1dialog.h"
#include "labelformat.h"
#include "layersdialog.h"
#include "printonlypanel.h"
#include "rolllayoutdialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QSignalSpy>
#include <QListWidget>
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
