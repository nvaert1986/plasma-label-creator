/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addressbookpanel.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int IndexRole = Qt::UserRole;
constexpr int MaxRecentBooks = 10;

QString bookFilter()
{
    return i18n("DYMO address books (*.ABX *.abx)") + u";;"_s + i18n("All files (*)");
}

KConfigGroup settings()
{
    return KSharedConfig::openConfig()->group(u"Address Book"_s);
}

} // namespace

AddressBookPanel::AddressBookPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *top = new QGridLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(i18n("Search the address book"));
    m_search->setClearButtonEnabled(true);
    m_books = new QComboBox;
    top->addWidget(new QLabel(i18n("Search")), 0, 0);
    top->addWidget(new QLabel(i18n("Address book")), 0, 1);
    top->addWidget(m_search, 1, 0);
    top->addWidget(m_books, 1, 1);
    layout->addLayout(top);

    m_checkAll = new QCheckBox(i18n("All"));
    m_checkAll->setTristate(true);
    m_list = new QListWidget;
    m_list->setAlternatingRowColors(true);
    m_counts = new QLabel;
    m_counts->setAlignment(Qt::AlignRight);
    m_delete = new QPushButton(QIcon::fromTheme(u"edit-delete"_s), i18n("Delete"));
    m_delete->setToolTip(i18n("Delete the checked entries"));
    auto *listFooter = new QHBoxLayout;
    listFooter->addWidget(m_delete);
    listFooter->addStretch();
    listFooter->addWidget(m_counts);
    layout->addWidget(m_checkAll);
    layout->addWidget(m_list, 1);
    layout->addLayout(listFooter);

    auto *current = new QGridLayout;
    m_address = new QPlainTextEdit;
    m_address->setMaximumHeight(110);
    m_new = new QPushButton(i18n("New"));
    m_save = new QPushButton(i18n("Save"));
    m_cancel = new QPushButton(i18n("Cancel"));
    auto *buttons = new QVBoxLayout;
    buttons->addWidget(m_new);
    buttons->addWidget(m_save);
    buttons->addWidget(m_cancel);
    buttons->addStretch();
    m_fileAs = new QComboBox;
    m_fileAs->setEditable(true);
    current->addWidget(new QLabel(i18n("Current address:")), 0, 0, 1, 2);
    current->addWidget(m_address, 1, 0);
    current->addLayout(buttons, 1, 1);
    current->addWidget(new QLabel(i18n("File as:")), 2, 0, 1, 2);
    current->addWidget(m_fileAs, 3, 0);
    layout->addLayout(current);

    connect(m_search, &QLineEdit::textChanged, this, &AddressBookPanel::fill);
    connect(m_books, &QComboBox::activated, this, [this](int index) {
        const QString file = m_books->itemData(index).toString();
        if (file.isEmpty()) {
            open();
        } else if (file != m_fileName && !openFile(file)) {
            fill();
        }
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) {
            showEntry(item->data(IndexRole).toInt());
            Q_EMIT addressChosen(m_book.entries.at(m_current).address);
        }
    });
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int index = item->data(IndexRole).toInt();
        if (item->checkState() == Qt::Checked) {
            m_checked.insert(index);
        } else {
            m_checked.remove(index);
        }
        updateCounts();
    });
    // All, as in DLS, means all entries shown.
    connect(m_checkAll, &QCheckBox::clicked, this, [this]() {
        const Qt::CheckState state = m_checkAll->checkState() == Qt::Unchecked ? Qt::Unchecked : Qt::Checked;
        const QSignalBlocker blocker(m_list);
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *item = m_list->item(i);
            item->setCheckState(state);
            if (state == Qt::Checked) {
                m_checked.insert(item->data(IndexRole).toInt());
            } else {
                m_checked.remove(item->data(IndexRole).toInt());
            }
        }
        updateCounts();
    });
    connect(m_address, &QPlainTextEdit::textChanged, this, [this]() {
        const bool editing = m_current < 0 || m_address->toPlainText() != m_book.entries.value(m_current).address;
        m_save->setEnabled(editing && !m_address->toPlainText().trimmed().isEmpty());
        m_cancel->setEnabled(editing);
    });
    connect(m_fileAs, &QComboBox::editTextChanged, this, [this]() {
        if (m_current >= 0 && m_fileAs->currentText() != m_book.entries.at(m_current).fileAs) {
            m_save->setEnabled(true);
            m_cancel->setEnabled(true);
        }
    });
    connect(m_new, &QPushButton::clicked, this, &AddressBookPanel::newEntry);
    connect(m_save, &QPushButton::clicked, this, &AddressBookPanel::saveEntry);
    connect(m_cancel, &QPushButton::clicked, this, &AddressBookPanel::cancelEntry);
    connect(m_delete, &QPushButton::clicked, this, &AddressBookPanel::deleteChecked);

    const QString last = settings().readEntry("Current", QString());
    if (last.isEmpty() || !QFileInfo::exists(last) || !openFile(last)) {
        newBook();
    }
}

QString AddressBookPanel::fileName() const
{
    return m_fileName;
}

bool AddressBookPanel::isModified() const
{
    return m_modified;
}

void AddressBookPanel::setModified(bool modified)
{
    m_modified = modified;
}

void AddressBookPanel::rememberBook(const QString &fileName)
{
    QStringList recent = settings().readEntry("Recent", QStringList());
    if (!fileName.isEmpty()) {
        recent.removeAll(fileName);
        recent.prepend(fileName);
        while (recent.size() > MaxRecentBooks) {
            recent.removeLast();
        }
        settings().writeEntry("Recent", recent);
        settings().writeEntry("Current", fileName);
    }

    const QSignalBlocker blocker(m_books);
    m_books->clear();
    if (fileName.isEmpty()) {
        m_books->addItem(i18n("New address book"), QString());
    }
    for (const QString &file : std::as_const(recent)) {
        if (QFileInfo::exists(file)) {
            m_books->addItem(QFileInfo(file).completeBaseName(), file);
        }
    }
    m_books->addItem(QIcon::fromTheme(u"document-open"_s), i18n("Open…"), QString());
    m_books->setCurrentIndex(std::max(0, m_books->findData(fileName)));
}

bool AddressBookPanel::openFile(const QString &fileName)
{
    if (!maybeSave()) {
        return false;
    }
    QString error;
    auto book = AddressBook::load(fileName, &error);
    if (!book) {
        KMessageBox::error(this, i18n("Could not open the address book %1:\n%2", fileName, error));
        return false;
    }
    m_book = *book;
    m_fileName = fileName;
    m_checked.clear();
    setModified(false);
    rememberBook(fileName);
    fill();
    newEntry();
    return true;
}

void AddressBookPanel::newBook()
{
    if (!maybeSave()) {
        return;
    }
    m_book = AddressBook();
    m_fileName.clear();
    m_checked.clear();
    setModified(false);
    rememberBook(QString());
    fill();
    newEntry();
}

void AddressBookPanel::open()
{
    const QString file = QFileDialog::getOpenFileName(this, i18n("Open Address Book"), QString(), bookFilter());
    if (!file.isEmpty()) {
        openFile(file);
    } else {
        rememberBook(m_fileName.isEmpty() ? QString() : m_fileName);
    }
}

bool AddressBookPanel::save()
{
    if (m_fileName.isEmpty()) {
        return saveAs();
    }
    QString error;
    if (!m_book.save(m_fileName, &error)) {
        KMessageBox::error(this, i18n("Could not save the address book %1:\n%2", m_fileName, error));
        return false;
    }
    setModified(false);
    return true;
}

bool AddressBookPanel::saveAs()
{
    // The dialog adds .ABX itself, so it also asks before overwriting that file.
    QFileDialog dialog(this, i18n("Save Address Book"), m_fileName, bookFilter());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(u"ABX"_s);
    if (!m_fileName.isEmpty()) {
        dialog.selectFile(m_fileName);
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return false;
    }
    const QString file = dialog.selectedFiles().constFirst();
    m_fileName = file;
    if (!save()) {
        return false;
    }
    rememberBook(file);
    return true;
}

bool AddressBookPanel::maybeSave()
{
    if (!m_modified) {
        return true;
    }
    switch (KMessageBox::warningTwoActionsCancel(this,
                                                 i18n("The address book has been changed. Do you want to save it?"),
                                                 i18n("Unsaved Address Book"),
                                                 KStandardGuiItem::save(),
                                                 KStandardGuiItem::discard())) {
    case KMessageBox::PrimaryAction:
        return save();
    case KMessageBox::SecondaryAction:
        return true;
    default:
        return false;
    }
}

void AddressBookPanel::importCsv()
{
    const QString file = QFileDialog::getOpenFileName(this, i18n("Import Addresses"), QString(), i18n("CSV files (*.csv *.txt)") + u";;"_s + i18n("All files (*)"));
    if (file.isEmpty()) {
        return;
    }
    QString error;
    const QList<AddressEntry> entries = AddressBook::importCsv(file, &error);
    if (entries.isEmpty()) {
        KMessageBox::error(this, error.isEmpty() ? i18n("No addresses were found in %1.", file) : error);
        return;
    }
    m_book.entries << entries;
    sortEntries();
    setModified(true);
    fill();
    KMessageBox::information(this, i18np("Imported one address.", "Imported %1 addresses.", entries.size()));
}

QStringList AddressBookPanel::checkedAddresses() const
{
    // Also those the search hides: they were checked, and the count says so.
    QStringList addresses;
    for (int i = 0; i < m_book.entries.size(); ++i) {
        if (m_checked.contains(i)) {
            addresses << m_book.entries.at(i).address;
        }
    }
    return addresses;
}

void AddressBookPanel::sortEntries()
{
    // Equal entries are interchangeable, so matching by value is enough.
    QList<AddressEntry> checked;
    for (int index : std::as_const(m_checked)) {
        checked << m_book.entries.at(index);
    }
    const std::optional<AddressEntry> current = m_current >= 0 ? std::optional(m_book.entries.at(m_current)) : std::nullopt;
    m_book.sort();
    m_checked.clear();
    for (int i = 0; i < m_book.entries.size(); ++i) {
        const qsizetype found = checked.indexOf(m_book.entries.at(i));
        if (found >= 0) {
            checked.removeAt(found);
            m_checked.insert(i);
        }
    }
    m_current = current ? int(m_book.entries.indexOf(*current)) : -1;
}

void AddressBookPanel::fill()
{
    // Entries filed the same way get a second line to tell them apart.
    QHash<QString, int> names;
    for (const AddressEntry &entry : std::as_const(m_book.entries)) {
        ++names[entry.displayName()];
    }

    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const QString search = m_search->text().trimmed();
    for (int i = 0; i < m_book.entries.size(); ++i) {
        const AddressEntry &entry = m_book.entries.at(i);
        if (!search.isEmpty() && !entry.address.contains(search, Qt::CaseInsensitive) && !entry.fileAs.contains(search, Qt::CaseInsensitive)) {
            continue;
        }
        QString text = entry.displayName();
        if (names.value(text) > 1) {
            text += u"\n    "_s + entry.address.section(u'\n', 1, 1).trimmed();
        }
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(IndexRole, i);
        item->setCheckState(m_checked.contains(i) ? Qt::Checked : Qt::Unchecked);
        if (i == m_current) {
            m_list->setCurrentItem(item);
        }
    }
    updateCounts();
}

void AddressBookPanel::updateCounts()
{
    int shownChecked = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        shownChecked += m_list->item(i)->checkState() == Qt::Checked;
    }
    const int checked = int(m_checked.size());
    m_counts->setText(i18np("1 entry", "%1 entries", m_book.entries.size()) + u" | "_s + i18np("1 selected", "%1 selected", checked));
    m_delete->setEnabled(checked > 0);
    m_checkAll->setCheckState(shownChecked == 0 ? Qt::Unchecked : (shownChecked == m_list->count() ? Qt::Checked : Qt::PartiallyChecked));
    Q_EMIT checkedChanged(checked);
}

void AddressBookPanel::showEntry(int index)
{
    m_current = index;
    const AddressEntry entry = m_book.entries.value(index);
    {
        const QSignalBlocker addressBlocker(m_address);
        const QSignalBlocker fileAsBlocker(m_fileAs);
        m_address->setPlainText(entry.address);
        m_fileAs->clear();
        m_fileAs->addItems(entry.fileAsSuggestions());
        m_fileAs->setEditText(entry.fileAs);
    }
    m_save->setEnabled(false);
    m_cancel->setEnabled(false);
}

void AddressBookPanel::newEntry()
{
    m_current = -1;
    m_list->setCurrentItem(nullptr);
    const QSignalBlocker addressBlocker(m_address);
    const QSignalBlocker fileAsBlocker(m_fileAs);
    m_address->clear();
    m_fileAs->clear();
    m_save->setEnabled(false);
    m_cancel->setEnabled(false);
    m_address->setFocus();
}

void AddressBookPanel::saveEntry()
{
    const QString address = m_address->toPlainText().trimmed();
    if (address.isEmpty()) {
        return;
    }
    AddressEntry entry = m_current >= 0 ? m_book.entries.at(m_current) : AddressBook::fromAddress(address);
    entry.address = address;
    if (!m_fileAs->currentText().trimmed().isEmpty()) {
        entry.fileAs = m_fileAs->currentText().trimmed();
    }
    if (m_current >= 0) {
        m_book.entries[m_current] = entry;
    } else {
        m_book.entries << entry;
        m_current = int(m_book.entries.size() - 1);
    }
    sortEntries();
    setModified(true);
    // DLS saves the address book with every change.
    if (!m_fileName.isEmpty()) {
        save();
    }
    fill();
    showEntry(m_current);
    Q_EMIT addressChosen(entry.address);
}

void AddressBookPanel::cancelEntry()
{
    if (m_current >= 0) {
        showEntry(m_current);
    } else {
        newEntry();
    }
}

void AddressBookPanel::deleteChecked()
{
    QList<int> indexes(m_checked.cbegin(), m_checked.cend());
    if (indexes.isEmpty()
        || KMessageBox::warningContinueCancel(this,
                                              i18np("Delete the checked address?", "Delete the %1 checked addresses?", indexes.size()),
                                              i18n("Delete Addresses"),
                                              KStandardGuiItem::del())
            != KMessageBox::Continue) {
        return;
    }
    std::sort(indexes.begin(), indexes.end(), std::greater<>());
    for (int index : std::as_const(indexes)) {
        m_book.entries.removeAt(index);
    }
    m_checked.clear();
    setModified(true);
    if (!m_fileName.isEmpty()) {
        save();
    }
    newEntry();
    fill();
}
