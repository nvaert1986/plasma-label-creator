/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

/**
 * @file
 * Add-ons for Plasma Label Creator: what the app offers an add-on and what
 * an add-on offers the app. An add-on is a Qt plugin in the
 * "plasma-label-creator" plugin folder (for instance
 * /usr/lib64/qt6/plugins/plasma-label-creator/) whose object implements
 * LabelCreator::Plugin. The app works the same without any add-on, and
 * every add-on can be switched off in its settings.
 *
 * Labels an add-on brings in are files on this computer, in a folder the
 * add-on keeps: the app opens, prints and saves them as any label file, and
 * finds their pictures, data files and other labels next to them as usual.
 * The add-on gives such a label an origin, its own key for it (a team's
 * label on a server, say), and the app tells the add-on when that label is
 * saved or printed, and asks it for the label's shared counters.
 *
 * Only Qt types cross the line between the app and an add-on, and nothing
 * here is linked: this header is all an add-on needs.
 */

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QtPlugin>

#include <optional>

class QAction;
class QWidget;

namespace LabelCreator
{

/** What the app offers an add-on. */
class PluginHost
{
public:
    virtual ~PluginHost() = default;

    /** The app's window, to show dialogs over. */
    virtual QWidget *dialogParent() = 0;

    /**
     * Opens the label file @p fileName as the current label, as one of the
     * add-on's with @p origin; asks first to save the label open now. False
     * when it was not opened.
     */
    virtual bool openLabel(const QString &fileName, const QString &origin) = 0;

    /** The current label's file; empty when it has never been saved. */
    virtual QString currentFileName() const = 0;
    /** The current label's origin; empty when it is no add-on's. */
    virtual QString currentOrigin() const = 0;

    /**
     * Saves the current label as @p fileName (in the add-on's folder) and
     * makes it one of the add-on's, with @p origin. Saving it again then
     * tells the add-on, as for a label it opened.
     */
    virtual bool saveLabelAs(const QString &fileName, const QString &origin, QString *errorString) = 0;

    /** A short message in the status bar. */
    virtual void showMessage(const QString &text) = 0;
};

/** A run of labels that was printed. */
struct PluginPrint {
    QString origin;
    QByteArray content; // the label file as it was printed, before its counters moved on
    QString title;
    QString printer;
    int labels = 0; // without header and trailer labels
    QHash<QString, QString> answers;
    QList<QHash<QString, QString>> rows;
};

/** What an add-on offers the app. */
class Plugin
{
public:
    virtual ~Plugin() = default;

    /** Called once, right after the add-on is loaded. */
    virtual void initialise(PluginHost *host) = 0;

    /** Actions for the File menu, such as opening and saving labels elsewhere. */
    virtual QList<QAction *> fileActions() = 0;

    /** Whether @p origin is one of this add-on's labels. */
    virtual bool owns(const QString &origin) const = 0;

    /** The current label's name in the window's title, such as "Name tag (Warehouse)". */
    virtual QString title(const QString &origin) const = 0;

    /**
     * The label @p origin was saved to @p fileName: send it on. With
     * @p keepVersion false only its counters moved on, after printing.
     * False and @p errorString when that failed (the file stays saved).
     */
    virtual bool labelSaved(const QString &origin, const QString &fileName, bool keepVersion, QString *errorString) = 0;

    /**
     * Shared counter @p name of label @p origin: takes @p count values for
     * printing, starting at @p start when the counter is new, and gives the
     * first. With @p count 0, only the next value, for a preview. Nullopt
     * and @p errorString when the counter could not be reached; nullopt
     * without an error when the add-on does not keep this counter (the app
     * then uses its own shared counter file).
     */
    virtual std::optional<int> takeCounter(const QString &origin, const QString &name, int start, int count, int increment, QString *errorString) = 0;

    /** A run of label @p print.origin was printed. */
    virtual void printed(const PluginPrint &print) = 0;
};

} // namespace LabelCreator

#define LabelCreatorPlugin_iid "org.kde.plasma-label-creator.Plugin/1"
Q_DECLARE_INTERFACE(LabelCreator::Plugin, LabelCreatorPlugin_iid)
