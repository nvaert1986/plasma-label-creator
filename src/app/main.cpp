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
#include <QFileInfo>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <clocale>
#include <cstdio>
#include <functional>

using namespace Qt::Literals::StringLiterals;

namespace
{

// The language chosen under Settings > Configure Language. KXmlGui applies
// it once the application exists, but Plasma's platform theme already loads
// the frameworks' translations while the application is made, so their
// menu items (Configure Toolbars…) kept the desktop's language: apply it
// before.
void applyChosenLanguage()
{
    const QSettings overrides(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/klanguageoverridesrc"_s,
                              QSettings::IniFormat);
    const QByteArray chosen = overrides.value(u"Language/plasma-label-creator"_s).toByteArray();
    if (chosen.isEmpty()) {
        return;
    }
    const QByteArray desktop = qgetenv("LANGUAGE");
    qputenv("LANGUAGE", desktop.isEmpty() ? chosen : chosen + ':' + desktop);

    // gettext ignores LANGUAGE when the locale for messages is C or POSIX
    // (a desktop with LANG=C.UTF-8, say), so nothing would be translated:
    // then messages take another installed locale, the chosen language's
    // own if there is one. Dates and numbers keep the desktop's formats.
    if (!qEnvironmentVariableIsEmpty("LC_ALL")) {
        return;
    }
    const char *current = std::setlocale(LC_MESSAGES, "");
    const QByteArray messages = current ? QByteArray(current) : QByteArray("C");
    if (messages != "C" && messages != "POSIX" && !messages.startsWith("C.")) {
        return;
    }
    const QByteArray language = chosen.split(':').constFirst();
    const QByteArray own = language.contains('_') ? language : language + '_' + language.toUpper();
    for (const QByteArray &candidate : QByteArrayList{own + ".UTF-8", "en_US.UTF-8", "en_GB.UTF-8"}) {
        if (std::setlocale(LC_MESSAGES, candidate.constData())) {
            qputenv("LC_MESSAGES", candidate);
            return;
        }
    }
}

} // namespace

int main(int argc, char *argv[])
{
    applyChosenLanguage();
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasma-label-creator");
    // Started from the build folder, before installing: its translations
    // are in build/locale, next to build/bin.
    if (const QString built = QCoreApplication::applicationDirPath() + u"/../locale"_s; QFileInfo::exists(built)) {
        KLocalizedString::addDomainLocaleDir("plasma-label-creator", built);
    }

    KAboutData aboutData(u"plasma-label-creator"_s,
                         i18n("Plasma Label Creator"),
                         QStringLiteral(LABELCREATOR_VERSION_STRING),
                         i18n("Label designer for DYMO LabelWriter and LabelManager printers and ZPL label printers"),
                         KAboutLicense::GPL_V2,
                         i18n("© 2026 nvaert1986"));
    aboutData.addAuthor(u"nvaert1986"_s, i18n("Developer"), u"25772781+nvaert1986@users.noreply.github.com"_s);
    aboutData.setDesktopFileName(u"plasma-label-creator"_s);
    KAboutData::setApplicationData(aboutData);
    // Its own icon when installed (WITH_ICONS), else a generic one.
    QApplication::setWindowIcon(QIcon::fromTheme(u"io.github.nvaert1986.PlasmaLabelCreator"_s, QIcon::fromTheme(u"document-print"_s)));

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(u"files"_s, i18n("Label files to open."), u"[files...]"_s);
    const QCommandLineOption printOnly(u"print-only"_s, i18n("Start in print-only mode: choose a label from a folder and print it, without editing."));
    parser.addOption(printOnly);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    auto *window = new MainWindow;
    window->show();
    // A folder lists its labels in print-only mode; a file opens.
    const QStringList files = parser.positionalArguments();
    QString folder;
    if (!files.isEmpty() && QFileInfo(files.constFirst()).isDir()) {
        folder = QFileInfo(files.constFirst()).absoluteFilePath();
    } else if (!files.isEmpty()) {
        window->openFile(files.constFirst());
    }
    if (parser.isSet(printOnly)) {
        if (!folder.isEmpty()) {
            window->setPrintOnlyFolder(folder);
        }
        window->setPrintOnly(true);
    }

    // For development: LABELCREATOR_SCREENSHOT=file.png saves a picture of the
    // window and quits, which works on the offscreen platform too.
    if (qEnvironmentVariableIsSet("LABELCREATOR_SCREENSHOT")) {
        // LABELCREATOR_SCREENSHOT_SIZE=1920x1080 pictures the window at that size.
        if (const QStringList size = qEnvironmentVariable("LABELCREATOR_SCREENSHOT_SIZE").split(u'x'); size.size() == 2) {
            window->resize(size.at(0).toInt(), size.at(1).toInt());
        }
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
