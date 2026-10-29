/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow.h"

#include "labelcreator_version.h"

#include <KAboutData>
#include <KActionCollection>
#include <KCrash>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>
#include <QMenuBar>
#include <QMenu>
#include <QUrl>

#include <cstdio>
#include <functional>

using namespace Qt::Literals::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasma-label-creator");

    KAboutData aboutData(u"plasma-label-creator"_s,
                         i18n("Plasma Label Creator"),
                         QStringLiteral(LABELCREATOR_VERSION_STRING),
                         i18n("Label designer for DYMO LabelWriter printers"),
                         KAboutLicense::GPL_V2,
                         i18n("© 2026 nvaert1986"));
    aboutData.addAuthor(u"nvaert1986"_s, i18n("Developer"), u"25772781+nvaert1986@users.noreply.github.com"_s);
    aboutData.setDesktopFileName(u"plasma-label-creator"_s);
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(u"document-print"_s));

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(u"files"_s, i18n("Label files to open."), u"[files...]"_s);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    auto *window = new MainWindow;
    window->show();
    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty()) {
        window->openFile(files.constFirst());
    }

    // For development: LABELCREATOR_SCREENSHOT=file.png saves a picture of the
    // window and quits, which works on the offscreen platform too.
    if (qEnvironmentVariableIsSet("LABELCREATOR_SCREENSHOT")) {
        window->showScreenshotTab(qEnvironmentVariableIntValue("LABELCREATOR_SCREENSHOT_TAB"));
        if (qEnvironmentVariableIsSet("LABELCREATOR_SCREENSHOT_SELECT")) {
            window->selectForScreenshot(qEnvironmentVariableIntValue("LABELCREATOR_SCREENSHOT_SELECT"));
        }
        // LABELCREATOR_SCREENSHOT_KIND=<row>:<file.png> also pictures the label kinds dialog.
        if (const QStringList kind = qEnvironmentVariable("LABELCREATOR_SCREENSHOT_KIND").split(u':'); kind.size() == 2) {
            QTimer::singleShot(600, window, [window, kind]() {
                window->labelKindForScreenshot(kind.at(0).toInt(), kind.at(1));
            });
        }
        if (qEnvironmentVariableIsSet("LABELCREATOR_SCREENSHOT_EDIT")) {
            QTimer::singleShot(500, window, [window]() {
                window->editTextForScreenshot(qEnvironmentVariableIntValue("LABELCREATOR_SCREENSHOT_EDIT"));
            });
        }
        // LABELCREATOR_SCREENSHOT_POPUP=<action>:<file.png> also opens that
        // action's drop-down menu and saves a picture of it.
        const QStringList popup = qEnvironmentVariable("LABELCREATOR_SCREENSHOT_POPUP").split(u':');
        if (popup.size() == 2) {
            QTimer::singleShot(1000, window, [window, popup]() {
                if (QAction *action = window->actionCollection()->action(popup.at(0)); action && action->menu()) {
                    action->menu()->popup(window->mapToGlobal(QPoint(10, 60)));
                    QTimer::singleShot(300, action->menu(), [menu = action->menu(), file = popup.at(1)]() {
                        menu->grab().save(file);
                    });
                }
            });
        }
        QTimer::singleShot(1500, window, [window]() {
            window->grab().save(qEnvironmentVariable("LABELCREATOR_SCREENSHOT"));
            // LABELCREATOR_SCREENSHOT_MENUS=1 also prints the menus, which a
            // picture of the window does not show.
            if (qEnvironmentVariableIsSet("LABELCREATOR_SCREENSHOT_MENUS")) {
                const std::function<void(const QList<QAction *> &, int)> dump = [&dump](const QList<QAction *> &actions, int depth) {
                    for (QAction *action : actions) {
                        const QString text = action->isSeparator() ? u"----"_s : action->text().remove(u'&');
                        std::printf("%s%s%s\n", qPrintable(QString(depth * 2, u' ')), qPrintable(text), action->isEnabled() ? "" : " (disabled)");
                        if (action->menu()) {
                            dump(action->menu()->actions(), depth + 1);
                        }
                    }
                };
                dump(window->menuBar()->actions(), 0);
            }
            QApplication::quit();
        });
    }

    return app.exec();
}
