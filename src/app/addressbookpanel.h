/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "addressbook.h"

#include <QSet>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;

/**
 * The address book tab, laid out as in DLS 8: a searchable list of entries
 * with check boxes for printing, and the current address to edit below.
 */
class AddressBookPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AddressBookPanel(QWidget *parent = nullptr);

    QString fileName() const;
    bool isModified() const;

    /** The addresses checked for printing, in list order. */
    QStringList checkedAddresses() const;

    bool openFile(const QString &fileName);
    void newBook();
    void open();
    bool save();
    bool saveAs();
    void importCsv();
    /** Asks to save unsaved changes; false if the user cancelled. */
    bool maybeSave();

Q_SIGNALS:
    /** The user picked an address; DLS puts it on the label straight away. */
    void addressChosen(const QString &address);
    void checkedChanged(int count);

private:
    void fill();
    void showEntry(int index);
    void updateCounts();
    void newEntry();
    void saveEntry();
    void cancelEntry();
    void deleteChecked();
    void setModified(bool modified);
    void rememberBook(const QString &fileName);
    /** Sorts the book; check marks and the entry being edited stay with their entries. */
    void sortEntries();

    LabelCreator::AddressBook m_book;
    QString m_fileName;
    bool m_modified = false;
    int m_current = -1; // entry being edited, -1 for a new one
    // Checked entries, by index into m_book.entries. Kept here rather than in
    // the list, which only shows the entries matching the search.
    QSet<int> m_checked;

    QLineEdit *m_search;
    QComboBox *m_books;
    QCheckBox *m_checkAll;
    QListWidget *m_list;
    QLabel *m_counts;
    QPushButton *m_delete;
    QPlainTextEdit *m_address;
    QComboBox *m_fileAs;
    QPushButton *m_new;
    QPushButton *m_save;
    QPushButton *m_cancel;
};
