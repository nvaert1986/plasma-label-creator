/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelversions.h"
#include "printhistory.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

/** The earlier versions of a label, with a picture of each, to restore one. */
class VersionsDialog : public QDialog
{
    Q_OBJECT

public:
    VersionsDialog(const QString &labelFile, const QList<LabelCreator::LabelVersions::Version> &versions, QWidget *parent = nullptr);

    /** The version chosen to restore; empty when none. */
    QString chosenVersion() const;

private:
    void showVersion(int row);

    QList<LabelCreator::LabelVersions::Version> m_versions;
    QListWidget *m_list;
    QLabel *m_picture;
    QPushButton *m_restore;
};

/** What was printed: searchable, to open a label as printed or print it again. */
class PrintHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action {
        None,
        Open, // the label as printed
        PrintAgain, // the same labels, with the same answers and rows
        Clear,
    };

    PrintHistoryDialog(const QList<LabelCreator::PrintRecord> &records, QWidget *parent = nullptr);

    Action action() const;
    /** The print chosen; meaningful for Open and PrintAgain. */
    LabelCreator::PrintRecord chosenRecord() const;

private:
    void filter(const QString &text);
    void finish(Action action);

    QList<LabelCreator::PrintRecord> m_records;
    QLineEdit *m_search;
    QTableWidget *m_table;
    QPushButton *m_open;
    QPushButton *m_again;
    Action m_action = Action::None;
};
