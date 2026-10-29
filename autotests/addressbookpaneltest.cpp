/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addressbook.h"
#include "addressbookpanel.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

const QString Alice = u"Alice Archer\nAcacia Lane 1\n1000 AA Amsterdam"_s;
const QString Bob = u"Bob Baker\nBirch Road 2\n2000 BB Breda"_s;
const QString Charlie = u"Charlie Cook\nCedar Street 3\n3000 CC Culemborg"_s;

QListWidgetItem *itemFor(QListWidget *list, const QString &name)
{
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->text().contains(name)) {
            return list->item(i);
        }
    }
    return nullptr;
}

QPushButton *button(QWidget *panel, const QString &text)
{
    for (QPushButton *candidate : panel->findChildren<QPushButton *>()) {
        if (candidate->text().remove(u'&') == text) {
            return candidate;
        }
    }
    return nullptr;
}

} // namespace

class AddressBookPanelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Never the user's own address book or settings.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QVERIFY(m_dir.isValid());
        m_file = m_dir.filePath(u"book.ABX"_s);
        AddressBook book;
        for (const QString &address : {Charlie, Alice, Bob}) {
            book.entries << AddressBook::fromAddress(address);
        }
        book.sort();
        QVERIFY(book.save(m_file));
    }

    void checksSurviveSearching()
    {
        AddressBookPanel panel;
        QVERIFY(panel.openFile(m_file));
        auto *list = panel.findChild<QListWidget *>();
        auto *search = panel.findChild<QLineEdit *>();
        QVERIFY(list && search);
        QCOMPARE(list->count(), 3);

        itemFor(list, u"Bob"_s)->setCheckState(Qt::Checked);
        QCOMPARE(panel.checkedAddresses(), QStringList{Bob});

        // The search hides Bob, but he stays checked and is still printed.
        search->setText(u"Alice"_s);
        QCOMPARE(list->count(), 1);
        QCOMPARE(panel.checkedAddresses(), QStringList{Bob});
        search->clear();
        QCOMPARE(list->count(), 3);
        QCOMPARE(itemFor(list, u"Bob"_s)->checkState(), Qt::Checked);
        QCOMPARE(itemFor(list, u"Alice"_s)->checkState(), Qt::Unchecked);
    }

    void checksStayWithTheirEntriesWhenSorting()
    {
        AddressBookPanel panel;
        QVERIFY(panel.openFile(m_file));
        auto *list = panel.findChild<QListWidget *>();
        auto *address = panel.findChild<QPlainTextEdit *>();
        QPushButton *save = button(&panel, u"Save"_s);
        QVERIFY(list && address && save);
        QSignalSpy checkedChanged(&panel, &AddressBookPanel::checkedChanged);

        itemFor(list, u"Charlie"_s)->setCheckState(Qt::Checked);
        QCOMPARE(checkedChanged.constLast().at(0).toInt(), 1);

        // A new entry sorts in front of the checked one.
        button(&panel, u"New"_s)->click();
        address->setPlainText(u"Aaron Abbot\nAsh Avenue 4\n4000 AA Assen"_s);
        QVERIFY(save->isEnabled());
        save->click();
        QCOMPARE(list->count(), 4);
        QCOMPARE(panel.checkedAddresses(), QStringList{Charlie});
        QCOMPARE(itemFor(list, u"Charlie"_s)->checkState(), Qt::Checked);

        // The new entry is still the one being edited, not Alice now at its old place.
        address->setPlainText(u"Aaron Abbot\nAsh Avenue 5\n4000 AA Assen"_s);
        save->click();
        const auto book = AddressBook::load(m_file);
        QVERIFY(book);
        QStringList addresses;
        for (const AddressEntry &entry : book->entries) {
            addresses << entry.address;
        }
        QCOMPARE(addresses.size(), 4);
        QVERIFY(addresses.contains(Alice));
        QVERIFY(addresses.contains(u"Aaron Abbot\nAsh Avenue 5\n4000 AA Assen"_s));
        QVERIFY(!addresses.contains(u"Aaron Abbot\nAsh Avenue 4\n4000 AA Assen"_s));
        QCOMPARE(panel.checkedAddresses(), QStringList{Charlie});
    }

    void checkAllMeansTheShownEntries()
    {
        AddressBookPanel panel;
        QVERIFY(panel.openFile(m_file));
        auto *search = panel.findChild<QLineEdit *>();
        auto *checkAll = panel.findChild<QCheckBox *>();
        QVERIFY(search && checkAll);
        search->setText(u"Bob"_s);
        checkAll->click();
        search->clear();
        QVERIFY(panel.checkedAddresses().contains(Bob));
        QVERIFY(!panel.checkedAddresses().contains(Alice));
    }

private:
    QTemporaryDir m_dir;
    QString m_file;
};

QTEST_MAIN(AddressBookPanelTest)

#include "addressbookpaneltest.moc"
