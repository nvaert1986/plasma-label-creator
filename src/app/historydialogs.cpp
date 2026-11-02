/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "historydialogs.h"

#include "labelformat.h"
#include "labelrenderer.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int PictureSize = 320; // pixels, the longer side

QPixmap picture(const QString &fileName)
{
    LabelReader reader;
    const auto document = reader.readFile(fileName);
    if (!document) {
        return {};
    }
    const QSizeF canvas = document->canvasSize();
    const qreal scale = PictureSize / std::max(canvas.width(), canvas.height());
    QImage image((canvas * scale).toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.scale(scale, scale);
    LabelRenderer(LabelRenderer::Mode::Preview).paint(&painter, *document);
    painter.end();
    return QPixmap::fromImage(image);
}

// The answers and rows of a print, in a few words.
QString printedWith(const PrintRecord &record)
{
    QStringList parts;
    for (auto it = record.answers.constBegin(); it != record.answers.constEnd(); ++it) {
        parts << it.key() + u": "_s + it.value();
    }
    parts.sort();
    if (!record.rows.isEmpty()) {
        parts << i18np("1 data row", "%1 data rows", record.rows.size());
    }
    return parts.join(u", "_s);
}

} // namespace

VersionsDialog::VersionsDialog(const QString &labelFile, const QList<LabelVersions::Version> &versions, QWidget *parent)
    : QDialog(parent)
    , m_versions(versions)
{
    setWindowTitle(i18n("Versions of %1", QFileInfo(labelFile).fileName()));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18n("Each time the label is saved, the version it replaces is kept here. Restoring one puts it on the canvas; "
                                  "save to keep it, or undo."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *row = new QHBoxLayout;
    m_list = new QListWidget(this);
    const QLocale locale;
    for (const LabelVersions::Version &version : versions) {
        m_list->addItem(locale.toString(version.saved, QLocale::ShortFormat));
    }
    if (versions.isEmpty()) {
        m_list->addItem(i18n("No earlier versions yet"));
        m_list->item(0)->setFlags(Qt::NoItemFlags);
    }
    row->addWidget(m_list, 1);
    m_picture = new QLabel(this);
    m_picture->setMinimumSize(PictureSize, PictureSize / 2);
    m_picture->setAlignment(Qt::AlignCenter);
    row->addWidget(m_picture, 2);
    layout->addLayout(row, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_restore = buttons->addButton(i18n("Restore"), QDialogButtonBox::AcceptRole);
    m_restore->setIcon(QIcon::fromTheme(u"document-revert"_s));
    m_restore->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(m_list, &QListWidget::currentRowChanged, this, &VersionsDialog::showVersion);
    connect(m_list, &QListWidget::itemActivated, this, [this]() {
        if (m_restore->isEnabled()) {
            accept();
        }
    });
    if (!versions.isEmpty()) {
        m_list->setCurrentRow(0);
    }
    resize(640, 360);
}

void VersionsDialog::showVersion(int row)
{
    const bool valid = row >= 0 && row < m_versions.size();
    m_restore->setEnabled(valid);
    m_picture->setPixmap(valid ? picture(m_versions.at(row).fileName) : QPixmap());
}

QString VersionsDialog::chosenVersion() const
{
    const int row = m_list->currentRow();
    return row >= 0 && row < m_versions.size() ? m_versions.at(row).fileName : QString();
}

PrintHistoryDialog::PrintHistoryDialog(const QList<PrintRecord> &records, QWidget *parent)
    : QDialog(parent)
    , m_records(records)
{
    setWindowTitle(i18n("Print History"));
    auto *layout = new QVBoxLayout(this);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(i18n("Search labels, printers, answers and data…"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &PrintHistoryDialog::filter);
    layout->addWidget(m_search);

    m_table = new QTableWidget(int(records.size()), 5, this);
    m_table->setHorizontalHeaderLabels({i18n("Printed"), i18n("Label"), i18n("Printer"), i18n("Labels"), i18n("With")});
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QLocale locale;
    for (int row = 0; row < records.size(); ++row) {
        const PrintRecord &record = records.at(row);
        const QStringList cells = {locale.toString(record.when, QLocale::ShortFormat),
                                   record.title,
                                   record.printer,
                                   QString::number(record.labels),
                                   printedWith(record)};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            item->setToolTip(column == 1 ? record.source : cells.at(column));
            m_table->setItem(row, column, item);
        }
    }
    m_table->resizeColumnsToContents();
    layout->addWidget(m_table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_open = buttons->addButton(i18n("Open Label"), QDialogButtonBox::ActionRole);
    m_open->setIcon(QIcon::fromTheme(u"document-open"_s));
    m_open->setToolTip(i18n("The label as it was printed"));
    m_again = buttons->addButton(i18n("Print Again"), QDialogButtonBox::ActionRole);
    m_again->setIcon(QIcon::fromTheme(u"document-print"_s));
    m_again->setToolTip(i18n("The same labels, with the same answers and data rows, on the printer chosen now"));
    auto *clear = buttons->addButton(i18n("Clear History…"), QDialogButtonBox::ResetRole);
    clear->setIcon(QIcon::fromTheme(u"edit-clear-history"_s));
    clear->setEnabled(!records.isEmpty());
    connect(m_open, &QPushButton::clicked, this, [this]() {
        finish(Action::Open);
    });
    connect(m_again, &QPushButton::clicked, this, [this]() {
        finish(Action::PrintAgain);
    });
    connect(clear, &QPushButton::clicked, this, [this]() {
        if (KMessageBox::warningContinueCancel(this, i18n("Forget everything printed so far?"), i18n("Clear History"), KStandardGuiItem::clear())
            == KMessageBox::Continue) {
            finish(Action::Clear);
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    const auto updateButtons = [this]() {
        const bool chosen = m_table->currentRow() >= 0 && !m_table->isRowHidden(m_table->currentRow());
        m_open->setEnabled(chosen);
        m_again->setEnabled(chosen);
    };
    connect(m_table, &QTableWidget::currentCellChanged, this, updateButtons);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this]() {
        finish(Action::Open);
    });
    if (!records.isEmpty()) {
        m_table->selectRow(0);
    }
    updateButtons();
    resize(820, 440);
}

void PrintHistoryDialog::filter(const QString &text)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const PrintRecord &record = m_records.at(row);
        bool match = text.isEmpty();
        for (int column = 0; column < m_table->columnCount() && !match; ++column) {
            match = m_table->item(row, column)->text().contains(text, Qt::CaseInsensitive);
        }
        // The data rows' values count too.
        for (const auto &values : record.rows) {
            for (auto it = values.constBegin(); it != values.constEnd() && !match; ++it) {
                match = it.value().contains(text, Qt::CaseInsensitive);
            }
        }
        m_table->setRowHidden(row, !match);
    }
}

void PrintHistoryDialog::finish(Action action)
{
    m_action = action;
    accept();
}

PrintHistoryDialog::Action PrintHistoryDialog::action() const
{
    return m_action;
}

PrintRecord PrintHistoryDialog::chosenRecord() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_records.size() ? m_records.at(row) : PrintRecord();
}
