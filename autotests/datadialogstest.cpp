/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "datadialogs.h"
#include "gs1dialog.h"
#include "layersdialog.h"
#include "rolllayoutdialog.h"

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
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

        AnswersDialog dialog({batch, note}, {}, nullptr);
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
        AnswersDialog again({batch, note}, {{u"Batch"_s, u"5678"_s}}, nullptr);
        QCOMPARE(again.answers().value(u"Batch"_s), u"5678"_s);
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
