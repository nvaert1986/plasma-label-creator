/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labelprinter.h"
#include "objectfactory.h"

#include <QSet>
#include <QWidget>

class AddressBookPanel;
class QLineEdit;
class QListWidget;
class QTabWidget;

/**
 * The panel left of the label, laid out like DLS 8: label types, objects
 * and clip art to insert, and the address book, in tabs.
 */
class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);

    AddressBookPanel *addressBook() const;

    /** The label types to offer, those of the selected printer. */
    void setPaperSizes(const QList<LabelCreator::PaperSize> &paperSizes);
    /** Marks the label type in use. */
    void setCurrentPaper(const QString &paperName);

    void showLabelTypes();
    void showInsert();
    void showAddressBook();

Q_SIGNALS:
    void paperChosen(const LabelCreator::PaperSize &paper);
    void insertRequested(ObjectFactory::Kind kind);
    void clipartRequested(const QString &fileName);
    /** The user wants to remove one of their own label sizes. */
    void customSizeRemovalRequested(const QString &name);
    void customSizeEditRequested(const QString &name);

private:
    void filterPaperSizes(const QString &text);

    QTabWidget *m_tabs;
    QLineEdit *m_paperFilter;
    QListWidget *m_paperList;
    QListWidget *m_insertList;
    AddressBookPanel *m_addressBook;
    QList<LabelCreator::PaperSize> m_paperSizes;
    QString m_currentPaper;
    // Label types come in groups for some printers (Altec's, by product);
    // the open ones.
    QSet<QString> m_openGroups;
};
