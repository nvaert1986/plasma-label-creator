/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "datadialogs.h"

#include <QDialogButtonBox>
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
