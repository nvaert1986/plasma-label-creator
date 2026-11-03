/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// An add-on for addontest: built as a plugin on its own, against the header only.

#include "plasmalabelcreatorplugin.h"

#include <QAction>
#include <QObject>

using namespace Qt::Literals::StringLiterals;

class FakeAddon : public QObject, public LabelCreator::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID LabelCreatorPlugin_iid FILE "fakeaddon.json")
    Q_INTERFACES(LabelCreator::Plugin)

public:
    void initialise(LabelCreator::PluginHost *host) override
    {
        m_host = host;
        m_open = new QAction(u"Open from the fake"_s, this);
        connect(m_open, &QAction::triggered, this, [this]() {
            m_host->openLabel(u"/fake/label.label"_s, u"fake:1"_s);
        });
    }
    QList<QAction *> fileActions() override
    {
        return {m_open};
    }
    QWidget *createStatusWidget(QWidget *) override
    {
        return nullptr;
    }
    bool owns(const QString &origin) const override
    {
        return origin.startsWith(u"fake:"_s);
    }
    QString originOf(const QString &fileName) override
    {
        return fileName.startsWith(u"/fake/"_s) ? u"fake:1"_s : QString();
    }
    QString title(const QString &origin) const override
    {
        return u"Fake label %1"_s.arg(origin.mid(5));
    }
    bool labelSaved(const QString &, const QString &fileName, bool, QString *errorString) override
    {
        if (fileName.isEmpty()) {
            *errorString = u"no file"_s;
            return false;
        }
        return true;
    }
    std::optional<int> takeCounter(const QString &, const QString &name, int start, int count, int increment, QString *) override
    {
        // Counters named "file:" are the app's own.
        if (name.startsWith(u"file:"_s)) {
            return std::nullopt;
        }
        const int first = m_counters.value(name, start);
        m_counters.insert(name, first + count * increment);
        return first;
    }
    void printed(const LabelCreator::PluginPrint &print) override
    {
        m_printed = print.labels;
    }

private:
    LabelCreator::PluginHost *m_host = nullptr;
    QAction *m_open = nullptr;
    QHash<QString, int> m_counters;
    int m_printed = 0;
};

#include "fakeaddon.moc"
