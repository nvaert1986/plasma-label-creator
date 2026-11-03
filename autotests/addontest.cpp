/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addons.h"
#include "plasmalabelcreatorplugin.h"

#include <QAction>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QStandardPaths>
#include <QTest>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

// What an add-on gets: records what it asks for.
class FakeHost : public PluginHost
{
public:
    QWidget *dialogParent() override
    {
        return nullptr;
    }
    bool openLabel(const QString &fileName, const QString &origin) override
    {
        opened = fileName;
        openedOrigin = origin;
        return true;
    }
    QString currentFileName() const override
    {
        return opened;
    }
    QString currentOrigin() const override
    {
        return openedOrigin;
    }
    bool saveLabelAs(const QString &, const QString &, QString *) override
    {
        return true;
    }
    void showMessage(const QString &) override
    {
    }

    QString opened;
    QString openedOrigin;
};

class AddonTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // The fake add-on is built into plugins/plasma-label-creator here.
        QCoreApplication::addLibraryPath(QStringLiteral(ADDON_DIR));
    }

    void findsAndLoadsAnAddon()
    {
        const QList<KPluginMetaData> installed = Addons::installed();
        const auto found = std::find_if(installed.cbegin(), installed.cend(), [](const KPluginMetaData &metaData) {
            return metaData.pluginId() == u"fakeaddon"_s;
        });
        QVERIFY(found != installed.cend());
        QCOMPARE(found->name(), u"Fake add-on"_s);

        QPluginLoader loader(found->fileName());
        auto *addon = qobject_cast<Plugin *>(loader.instance());
        QVERIFY2(addon, qPrintable(loader.errorString()));

        FakeHost host;
        addon->initialise(&host);
        const QList<QAction *> actions = addon->fileActions();
        QCOMPARE(actions.size(), 1);
        actions.constFirst()->trigger();
        QCOMPARE(host.openedOrigin, u"fake:1"_s);
        QVERIFY(addon->owns(u"fake:1"_s));
        QVERIFY(!addon->owns(u"other:1"_s));
        QCOMPARE(addon->title(u"fake:7"_s), u"Fake label 7"_s);
        // Its own files, opened as files, are still its labels.
        QCOMPARE(addon->originOf(u"/fake/label.label"_s), u"fake:1"_s);
        QVERIFY(addon->originOf(u"/home/someone/label.label"_s).isEmpty());

        // Counters: taken for printing, peeked at for a preview, or left to the app.
        QString error;
        QCOMPARE(addon->takeCounter(u"fake:1"_s, u"serial"_s, 100, 5, 1, &error), std::optional<int>(100));
        QCOMPARE(addon->takeCounter(u"fake:1"_s, u"serial"_s, 1, 0, 1, &error), std::optional<int>(105));
        QCOMPARE(addon->takeCounter(u"fake:1"_s, u"file:/a/b"_s, 1, 5, 1, &error), std::nullopt);
        QVERIFY(error.isEmpty());

        QVERIFY(addon->labelSaved(u"fake:1"_s, u"/fake/label.label"_s, true, &error));
        QVERIFY(!addon->labelSaved(u"fake:1"_s, QString(), true, &error));
        QCOMPARE(error, u"no file"_s);
    }

    void remembersWhichAreSwitchedOff()
    {
        Addons::setSwitchedOff({u"fakeaddon"_s});
        QCOMPARE(Addons::switchedOff(), QStringList{u"fakeaddon"_s});
        Addons::setSwitchedOff({});
        QVERIFY(Addons::switchedOff().isEmpty());
    }
};

QTEST_MAIN(AddonTest)
#include "addontest.moc"
