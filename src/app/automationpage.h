/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "automation.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTimer;
class QTreeWidget;

/**
 * Settings > Automation: the automation service, its watched folders, the
 * labels requests may name, and requests over HTTP, which are only offered
 * when it was built with Qt HTTP Server.
 */
class AutomationPage : public QWidget
{
    Q_OBJECT

public:
    explicit AutomationPage(const QString &settingsFile = LabelCreator::AutomationSettings::defaultFileName(), QWidget *parent = nullptr);

    LabelCreator::AutomationSettings settings() const;
    void showSettings(const LabelCreator::AutomationSettings &settings);
    /** Writes the settings (the service follows them) and the log-in start; false when it could not. */
    bool save(QString *errorString = nullptr);

    /** The service program next to the app, else on the PATH; empty when not installed. */
    static QString serviceProgram();
    /** The autostart entry that starts the service when the user logs in. */
    static QString autostartFile();
    /** Whether this build takes print requests over HTTP. */
    static bool hasHttp();

private:
    void updateStatus();
    void startService();
    void stopService();
    void addFolder();
    void editFolder();
    void showFolders();
    void updateExample();

    QString m_settingsFile;
    QList<LabelCreator::AutomationSettings::Folder> m_folders;
    QLabel *m_status;
    QPushButton *m_startStop;
    QCheckBox *m_atLogin;
    QLineEdit *m_labelsFolder;
    QComboBox *m_printer;
    QCheckBox *m_saveCounters;
    QTreeWidget *m_folderList;
    QPushButton *m_editFolder;
    QPushButton *m_removeFolder;
    QGroupBox *m_http;
    QRadioButton *m_thisComputer;
    QRadioButton *m_network;
    QSpinBox *m_port;
    QLineEdit *m_token;
    QLabel *m_example;
    QTimer *m_statusTimer;
};
