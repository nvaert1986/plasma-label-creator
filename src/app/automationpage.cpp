/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "automationpage.h"

#include "labelcreator_config.h"
#include "labelprinter.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <csignal>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

const QString ServiceName = u"plasma-label-creator-automation"_s;

// A line edit with a Browse… button beside it.
QWidget *withBrowse(QLineEdit *edit, const std::function<void()> &browse)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);
    auto *button = new QPushButton(QIcon::fromTheme(u"document-open-folder"_s), i18n("Browse…"));
    QObject::connect(button, &QPushButton::clicked, edit, browse);
    layout->addWidget(button);
    return row;
}

// The printers, with @p none first for "no printer chosen".
void fillPrinters(QComboBox *combo, const QString &none, const QString &current)
{
    combo->clear();
    combo->addItem(none, QString());
    for (const PrinterInfo &printer : LabelPrinter::printers()) {
        combo->addItem(QIcon::fromTheme(u"printer"_s), printer.description.isEmpty() ? printer.name : printer.description, printer.name);
    }
    // A printer that is gone stays chosen, so it is not lost by looking.
    if (!current.isEmpty() && combo->findData(current) < 0) {
        combo->addItem(i18nc("printer that is not found", "%1 (not found)", current), current);
    }
    combo->setCurrentIndex(std::max(0, combo->findData(current)));
}

// A watched folder: where, which label, which printer, how many.
bool editFolder(QWidget *parent, AutomationSettings::Folder *folder)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(i18n("Watched Folder"));
    auto *form = new QFormLayout(&dialog);
    auto *path = new QLineEdit(folder->folder);
    form->addRow(i18n("Folder:"), withBrowse(path, [&dialog, path]() {
                     const QString chosen = QFileDialog::getExistingDirectory(&dialog, i18n("Watched Folder"), path->text());
                     if (!chosen.isEmpty()) {
                         path->setText(chosen);
                     }
                 }));
    auto *label = new QLineEdit(folder->label);
    form->addRow(i18n("Label:"), withBrowse(label, [&dialog, label]() {
                     const QString chosen = QFileDialog::getOpenFileName(&dialog, i18n("Label to Print"), label->text(), i18n("Label files (*.label)"));
                     if (!chosen.isEmpty()) {
                         label->setText(chosen);
                     }
                 }));
    auto *printer = new QComboBox;
    fillPrinters(printer, i18n("The default printer"), folder->printer);
    form->addRow(i18n("Printer:"), printer);
    auto *copies = new QSpinBox;
    copies->setRange(1, 999);
    copies->setValue(folder->copies);
    form->addRow(i18n("Copies:"), copies);
    auto *explain = new QLabel(i18n("A CSV file dropped in the folder prints a label for each row, with the columns named on its first line as fields. "
                                    "A JSON file is a print job as sent over HTTP. Printed jobs move to \"printed\", the others to \"failed\" with the reason."));
    explain->setWordWrap(true);
    form->addRow(explain);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (!QFileInfo(path->text().trimmed()).isDir()) {
            KMessageBox::error(&dialog, i18n("Choose a folder that exists."));
            return;
        }
        if (!QFileInfo(label->text().trimmed()).isFile()) {
            KMessageBox::error(&dialog, i18n("Choose the label to print."));
            return;
        }
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    dialog.resize(560, dialog.sizeHint().height());
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    folder->folder = QFileInfo(path->text().trimmed()).absoluteFilePath();
    folder->label = QFileInfo(label->text().trimmed()).absoluteFilePath();
    folder->printer = printer->currentData().toString();
    folder->copies = copies->value();
    return true;
}

} // namespace

AutomationPage::AutomationPage(const QString &settingsFile, QWidget *parent)
    : QWidget(parent)
    , m_settingsFile(settingsFile)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *intro = new QLabel(i18n("The automation service prints without anyone at the screen: jobs dropped in watched folders, and requests from other "
                                  "programs over HTTP. It runs in the background, also when Plasma Label Creator is closed."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    // The service itself.
    auto *serviceBox = new QGroupBox(i18n("Service"));
    auto *serviceForm = new QFormLayout(serviceBox);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_startStop = new QPushButton;
    connect(m_startStop, &QPushButton::clicked, this, [this]() {
        Automation::servicePid() > 0 ? stopService() : startService();
    });
    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_startStop);
    serviceForm->addRow(i18n("Status:"), statusRow);
    m_atLogin = new QCheckBox(i18n("Start the service when I log in"));
    serviceForm->addRow(QString(), m_atLogin);
    m_labelsFolder = new QLineEdit;
    m_labelsFolder->setPlaceholderText(i18n("No labels may be named"));
    m_labelsFolder->setToolTip(i18n("Jobs and requests may name the labels in this folder and its subfolders, such as \"Rack/patch panel\"; no others."));
    serviceForm->addRow(i18n("Labels folder:"), withBrowse(m_labelsFolder, [this]() {
                            const QString chosen = QFileDialog::getExistingDirectory(this, i18n("Labels Folder"), m_labelsFolder->text());
                            if (!chosen.isEmpty()) {
                                m_labelsFolder->setText(chosen);
                            }
                        }));
    m_printer = new QComboBox;
    serviceForm->addRow(i18n("Default printer:"), m_printer);
    m_saveCounters = new QCheckBox(i18n("Save counters in the label after printing"));
    m_saveCounters->setToolTip(i18n("So the next job counts on from where this one stopped."));
    serviceForm->addRow(QString(), m_saveCounters);
    layout->addWidget(serviceBox);

    // Watched folders.
    auto *foldersBox = new QGroupBox(i18n("Watched Folders"));
    auto *foldersLayout = new QHBoxLayout(foldersBox);
    m_folderList = new QTreeWidget;
    m_folderList->setHeaderLabels({i18n("Folder"), i18n("Label"), i18n("Printer"), i18n("Copies")});
    m_folderList->setRootIsDecorated(false);
    m_folderList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_folderList->setMinimumHeight(90);
    foldersLayout->addWidget(m_folderList, 1);
    auto *folderButtons = new QVBoxLayout;
    auto *add = new QPushButton(QIcon::fromTheme(u"list-add"_s), i18n("Add…"));
    m_editFolder = new QPushButton(QIcon::fromTheme(u"document-edit"_s), i18n("Edit…"));
    m_removeFolder = new QPushButton(QIcon::fromTheme(u"list-remove"_s), i18n("Remove"));
    folderButtons->addWidget(add);
    folderButtons->addWidget(m_editFolder);
    folderButtons->addWidget(m_removeFolder);
    folderButtons->addStretch();
    foldersLayout->addLayout(folderButtons);
    connect(add, &QPushButton::clicked, this, &AutomationPage::addFolder);
    connect(m_editFolder, &QPushButton::clicked, this, &AutomationPage::editFolder);
    connect(m_folderList, &QTreeWidget::itemDoubleClicked, this, &AutomationPage::editFolder);
    connect(m_removeFolder, &QPushButton::clicked, this, [this]() {
        const int row = m_folderList->indexOfTopLevelItem(m_folderList->currentItem());
        if (row >= 0) {
            m_folders.removeAt(row);
            showFolders();
        }
    });
    connect(m_folderList, &QTreeWidget::currentItemChanged, this, [this]() {
        m_editFolder->setEnabled(m_folderList->currentItem());
        m_removeFolder->setEnabled(m_folderList->currentItem());
    });
    layout->addWidget(foldersBox);

    // Requests over HTTP: only when built with Qt HTTP Server.
    m_http = new QGroupBox(i18n("Take print requests over HTTP"));
    m_http->setCheckable(true);
    auto *httpForm = new QFormLayout(m_http);
    m_thisComputer = new QRadioButton(i18n("From this computer only"));
    m_network = new QRadioButton(i18n("From other computers on the network too"));
    m_network->setToolTip(i18n("Anyone on the network who has the token can print. Keep the token secret."));
    m_thisComputer->setChecked(true);
    httpForm->addRow(i18n("Listen:"), m_thisComputer);
    httpForm->addRow(QString(), m_network);
    m_port = new QSpinBox;
    m_port->setRange(1024, 65535);
    httpForm->addRow(i18n("Port:"), m_port);
    m_token = new QLineEdit;
    m_token->setReadOnly(true);
    m_token->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    auto *showToken = new QPushButton(QIcon::fromTheme(u"view-visible"_s), QString());
    showToken->setCheckable(true);
    showToken->setToolTip(i18n("Show the token"));
    connect(showToken, &QPushButton::toggled, this, [this](bool shown) {
        m_token->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::PasswordEchoOnEdit);
    });
    auto *copyToken = new QPushButton(QIcon::fromTheme(u"edit-copy"_s), QString());
    copyToken->setToolTip(i18n("Copy the token"));
    connect(copyToken, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_token->text());
    });
    auto *newToken = new QPushButton(QIcon::fromTheme(u"view-refresh"_s), i18n("New Token"));
    newToken->setToolTip(i18n("Programs using the old token can no longer print."));
    connect(newToken, &QPushButton::clicked, this, [this]() {
        m_token->setText(AutomationSettings::newToken());
        updateExample();
    });
    auto *tokenRow = new QHBoxLayout;
    tokenRow->addWidget(m_token, 1);
    tokenRow->addWidget(showToken);
    tokenRow->addWidget(copyToken);
    tokenRow->addWidget(newToken);
    httpForm->addRow(i18n("Token:"), tokenRow);
    m_example = new QLabel;
    m_example->setWordWrap(true);
    m_example->setTextInteractionFlags(Qt::TextSelectableByMouse);
    httpForm->addRow(m_example);
    connect(m_http, &QGroupBox::toggled, this, [this](bool on) {
        // Never without a token.
        if (on && m_token->text().isEmpty()) {
            m_token->setText(AutomationSettings::newToken());
        }
        updateExample();
    });
    connect(m_port, &QSpinBox::valueChanged, this, &AutomationPage::updateExample);
    layout->addWidget(m_http);
    if (!hasHttp()) {
        m_http->setChecked(false);
        m_http->setEnabled(false);
        auto *missing = new QLabel(i18n("Print requests over HTTP need Qt HTTP Server, which was not installed when Plasma Label Creator was built "
                                        "(dev-qt/qthttpserver on Gentoo, qt6-httpserver-dev on Debian and Ubuntu, qt6-qthttpserver-devel on Fedora, "
                                        "qt6-httpserver on Arch). Install it and build Plasma Label Creator again to use them; watched folders work without it."));
        missing->setWordWrap(true);
        missing->setEnabled(false);
        layout->addWidget(missing);
    }
    layout->addStretch();

    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(2000);
    connect(m_statusTimer, &QTimer::timeout, this, &AutomationPage::updateStatus);
    m_statusTimer->start();

    QString error;
    showSettings(AutomationSettings::load(m_settingsFile, &error));
    if (!error.isEmpty()) {
        m_status->setText(error);
    }
}

bool AutomationPage::hasHttp()
{
    return LABELCREATOR_HTTP_SERVER;
}

QString AutomationPage::serviceProgram()
{
    const QString next = QStandardPaths::findExecutable(ServiceName, {QCoreApplication::applicationDirPath()});
    return next.isEmpty() ? QStandardPaths::findExecutable(ServiceName) : next;
}

QString AutomationPage::autostartFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/autostart/"_s + ServiceName + u".desktop"_s;
}

AutomationSettings AutomationPage::settings() const
{
    AutomationSettings settings;
    settings.folders = m_folders;
    settings.labelsFolder = m_labelsFolder->text().trimmed();
    settings.printer = m_printer->currentData().toString();
    settings.saveCounters = m_saveCounters->isChecked();
    settings.http = hasHttp() && m_http->isChecked();
    settings.httpNetwork = m_network->isChecked();
    settings.httpPort = m_port->value();
    settings.httpToken = m_token->text();
    return settings;
}

void AutomationPage::showSettings(const AutomationSettings &settings)
{
    m_folders = settings.folders;
    showFolders();
    m_labelsFolder->setText(settings.labelsFolder);
    fillPrinters(m_printer, i18n("None: jobs name their printer"), settings.printer);
    m_saveCounters->setChecked(settings.saveCounters);
    m_token->setText(settings.httpToken);
    m_http->setChecked(hasHttp() && settings.http);
    (settings.httpNetwork ? m_network : m_thisComputer)->setChecked(true);
    m_port->setValue(settings.httpPort);
    m_atLogin->setChecked(QFileInfo::exists(autostartFile()));
    updateExample();
    updateStatus();
}

bool AutomationPage::save(QString *errorString)
{
    const AutomationSettings current = settings();
    // Unchanged: not written, so a running service is left alone.
    if (current != AutomationSettings::load(m_settingsFile) || !QFileInfo::exists(m_settingsFile)) {
        if ((!current.folders.isEmpty() || current.http || QFileInfo::exists(m_settingsFile)) && !current.save(m_settingsFile, errorString)) {
            return false;
        }
    }
    // Started at log-in through an autostart entry, as any Plasma session does.
    const QString autostart = autostartFile();
    if (m_atLogin->isChecked() && !serviceProgram().isEmpty()) {
        QDir().mkpath(QFileInfo(autostart).absolutePath());
        QSaveFile file(autostart);
        if (!file.open(QIODevice::WriteOnly)) {
            if (errorString) {
                *errorString = file.errorString();
            }
            return false;
        }
        file.write("[Desktop Entry]\nType=Application\nName=Plasma Label Creator automation\nIcon=document-print\nNoDisplay=true\nExec=");
        file.write(serviceProgram().toUtf8());
        file.write("\nX-KDE-autostart-phase=2\n");
        if (!file.commit()) {
            if (errorString) {
                *errorString = file.errorString();
            }
            return false;
        }
    } else if (!m_atLogin->isChecked() && QFileInfo::exists(autostart)) {
        QFile::remove(autostart);
    }
    return true;
}

void AutomationPage::updateStatus()
{
    const qint64 pid = Automation::servicePid();
    const bool installed = !serviceProgram().isEmpty();
    if (pid > 0) {
        m_status->setText(i18n("Running (process %1).", pid));
    } else if (installed) {
        m_status->setText(i18n("Not running."));
    } else {
        m_status->setText(i18n("The service (%1) is not installed.", ServiceName));
    }
    m_startStop->setText(pid > 0 ? i18n("Stop") : i18n("Start"));
    m_startStop->setIcon(QIcon::fromTheme(pid > 0 ? u"media-playback-stop"_s : u"media-playback-start"_s));
    m_startStop->setEnabled(pid > 0 || installed);
    m_startStop->setToolTip(pid > 0 ? QString() : i18n("Starts it with the settings as last applied."));
    m_atLogin->setEnabled(installed);
}

void AutomationPage::startService()
{
    if (!QProcess::startDetached(serviceProgram(), {})) {
        KMessageBox::error(this, i18n("The automation service could not be started."));
    }
    QTimer::singleShot(1000, this, &AutomationPage::updateStatus);
}

void AutomationPage::stopService()
{
    // Asked to stop: it finishes the job it is printing first.
    if (const qint64 pid = Automation::servicePid(); pid > 0) {
        ::kill(pid_t(pid), SIGTERM);
    }
    QTimer::singleShot(1000, this, &AutomationPage::updateStatus);
}

void AutomationPage::addFolder()
{
    AutomationSettings::Folder folder;
    if (::editFolder(this, &folder)) {
        m_folders << folder;
        showFolders();
    }
}

void AutomationPage::editFolder()
{
    const int row = m_folderList->indexOfTopLevelItem(m_folderList->currentItem());
    if (row >= 0 && ::editFolder(this, &m_folders[row])) {
        showFolders();
        m_folderList->setCurrentItem(m_folderList->topLevelItem(row));
    }
}

void AutomationPage::showFolders()
{
    m_folderList->clear();
    for (const AutomationSettings::Folder &folder : std::as_const(m_folders)) {
        auto *item = new QTreeWidgetItem(m_folderList,
                                         {folder.folder,
                                          QFileInfo(folder.label).fileName(),
                                          folder.printer.isEmpty() ? i18n("The default printer") : folder.printer,
                                          QString::number(folder.copies)});
        item->setToolTip(1, folder.label);
    }
    m_editFolder->setEnabled(false);
    m_removeFolder->setEnabled(false);
}

void AutomationPage::updateExample()
{
    if (!hasHttp()) {
        m_example->clear();
        return;
    }
    m_example->setText(i18n("For example:\ncurl -H \"Authorization: Bearer <token>\" -H \"Content-Type: application/json\" -d '{\"label\": \"Rack/patch panel\", "
                            "\"values\": {\"Port\": \"A12\"}}' http://localhost:%1/v1/print",
                            m_port->value()));
}
