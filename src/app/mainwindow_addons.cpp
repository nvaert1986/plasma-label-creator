/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Add-ons: loading them, and what they may do with the window.

#include "addons.h"
#include "labeleditor.h"
#include "mainwindow.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QAction>
#include <QFileInfo>
#include <QPluginLoader>
#include <QStatusBar>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

void MainWindow::loadAddons()
{
    const QStringList off = Addons::switchedOff();
    QList<QAction *> fileActions;
    for (const KPluginMetaData &metaData : Addons::installed()) {
        if (off.contains(metaData.pluginId())) {
            continue;
        }
        // The loader keeps the library loaded for as long as the app runs.
        QPluginLoader loader(metaData.fileName());
        auto *addon = qobject_cast<Plugin *>(loader.instance());
        if (!addon) {
            qWarning("The add-on %s could not be loaded: %s",
                     qPrintable(metaData.fileName()),
                     qPrintable(loader.errorString().isEmpty() ? u"it is not a Plasma Label Creator add-on"_s : loader.errorString()));
            continue;
        }
        addon->initialise(this);
        m_addons << addon;
        fileActions << addon->fileActions();
    }
    if (!fileActions.isEmpty()) {
        auto *separator = new QAction(this);
        separator->setSeparator(true);
        fileActions.prepend(separator);
        unplugActionList(u"addon_file_actions"_s);
        plugActionList(u"addon_file_actions"_s, fileActions);
    }
}

Plugin *MainWindow::currentAddon() const
{
    const QString origin = currentOrigin();
    if (origin.isEmpty()) {
        return nullptr;
    }
    for (Plugin *addon : m_addons) {
        if (addon->owns(origin)) {
            return addon;
        }
    }
    return nullptr;
}

bool MainWindow::addonSaved(bool keepVersion, bool quiet)
{
    Plugin *addon = currentAddon();
    if (!addon) {
        return true;
    }
    QString error;
    if (addon->labelSaved(currentOrigin(), m_editor->fileName(), keepVersion, &error)) {
        return true;
    }
    if (quiet) {
        qWarning("The add-on could not keep the label: %s", qPrintable(error));
    } else {
        KMessageBox::error(this, i18n("The label was saved on this computer, but not where it came from:\n%1", error));
    }
    return false;
}

QWidget *MainWindow::dialogParent()
{
    return this;
}

bool MainWindow::openLabel(const QString &fileName, const QString &origin)
{
    if (!openFile(fileName)) {
        return false;
    }
    m_origin = origin;
    m_originFile = m_editor->fileName();
    updateCaption();
    return true;
}

QString MainWindow::currentFileName() const
{
    return m_editor->fileName();
}

QString MainWindow::currentOrigin() const
{
    // Saved elsewhere or replaced by another label: no longer the add-on's.
    return !m_originFile.isEmpty() && m_editor->fileName() == m_originFile ? m_origin : QString();
}

bool MainWindow::saveLabelAs(const QString &fileName, const QString &origin, QString *errorString)
{
    finishEditing();
    if (!m_editor->save(fileName, errorString)) {
        return false;
    }
    m_origin = origin;
    m_originFile = m_editor->fileName();
    updateCaption();
    return true;
}

void MainWindow::showMessage(const QString &text)
{
    statusBar()->showMessage(text, 10000);
}
