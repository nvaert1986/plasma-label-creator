/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clipartpanel.h"

#include "dragsupport.h"

#include <KAcceleratorManager>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int PathRole = Qt::UserRole;
constexpr int ItemRole = Qt::UserRole + 1;

KConfigGroup settings()
{
    return KSharedConfig::openConfig()->group(u"Clip Art"_s);
}

/**
 * The images of one category. It grows to show them all instead of
 * scrolling, so the sections scroll together as one list.
 */
class ClipartGrid : public DragListWidget
{
public:
    ClipartGrid()
        : DragListWidget([](const QList<QListWidgetItem *> &items) {
            QStringList files;
            for (const QListWidgetItem *item : items) {
                files << item->data(PathRole).toString();
            }
            return DragSupport::imageMimeData(files);
        })
    {
        setViewMode(QListView::IconMode);
        setIconSize(QSize(40, 40));
        setGridSize(QSize(56, 56));
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setUniformItemSizes(true);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        // A static icon view turns dragging off; images are dragged onto the label.
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

    void fitHeight()
    {
        const int columns = std::max(1, viewport()->width() / gridSize().width());
        const int rows = (count() + columns - 1) / columns;
        setFixedHeight(rows * gridSize().height() + 2 * frameWidth() + 2);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        DragListWidget::resizeEvent(event);
        if (event->size().width() != event->oldSize().width()) {
            fitHeight();
        }
    }
};

} // namespace

ClipartPanel::ClipartPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *title = new QLabel(i18n("<b>Clip Art</b>"));
    auto *showAll = new QToolButton;
    showAll->setText(i18n("Show All"));
    showAll->setAutoRaise(true);
    auto *hideAll = new QToolButton;
    hideAll->setText(i18n("Hide All"));
    hideAll->setAutoRaise(true);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(showAll);
    titleRow->addWidget(hideAll);
    auto *hint = new QLabel(i18n("Drag onto the label, or double-click"));
    hint->setEnabled(false);

    auto *sections = new QWidget;
    m_sections = new QVBoxLayout(sections);
    m_sections->setContentsMargins(0, 0, 0, 0);
    m_sections->setSpacing(0);
    m_scroll = new QScrollArea;
    m_scroll->setWidget(sections);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setBackgroundRole(QPalette::Base);

    auto *add = new QToolButton;
    add->setIcon(QIcon::fromTheme(u"list-add"_s));
    add->setToolTip(i18n("Add images to My Clip Art"));
    m_remove = new QToolButton;
    m_remove->setIcon(QIcon::fromTheme(u"edit-delete"_s));
    m_remove->setToolTip(i18n("Remove the selected clip art"));
    m_remove->setEnabled(false);
    auto *more = new QToolButton;
    more->setIcon(QIcon::fromTheme(u"document-import"_s));
    more->setText(i18n("Import / Export"));
    more->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(QIcon::fromTheme(u"document-import"_s), i18n("Import DYMO Label Software's Clip Art…"), this, &ClipartPanel::importDlsProgram);
    menu->addAction(QIcon::fromTheme(u"document-import"_s), i18n("Import a DYMO Clip Art List (Clipart.xml)…"), this, &ClipartPanel::importDlsCatalog);
    menu->addSeparator();
    menu->addAction(QIcon::fromTheme(u"document-export"_s), i18n("Export Clip Art for DYMO Label Software…"), this, &ClipartPanel::exportLibrary);
    more->setMenu(menu);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(more);
    buttons->addStretch();
    buttons->addWidget(m_remove);
    buttons->addWidget(add);

    layout->addLayout(titleRow);
    layout->addWidget(hint);
    layout->addWidget(m_scroll, 1);
    layout->addLayout(buttons);

    connect(showAll, &QToolButton::clicked, this, [this]() {
        setAllExpanded(true);
    });
    connect(hideAll, &QToolButton::clicked, this, [this]() {
        setAllExpanded(false);
    });
    connect(add, &QToolButton::clicked, this, &ClipartPanel::addImages);
    connect(m_remove, &QToolButton::clicked, this, &ClipartPanel::removeSelected);

    reload();
}

void ClipartPanel::reload()
{
    m_library.load();
    fill();
}

void ClipartPanel::fill()
{
    while (QLayoutItem *item = m_sections->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_sectionList.clear();
    m_remove->setEnabled(false);

    const QList<ClipartItem> items = m_library.items();
    if (items.isEmpty()) {
        auto *empty = new QLabel(i18n("No clip art yet. Add images with +, or import DYMO Label Software's clip art."));
        empty->setWordWrap(true);
        empty->setMargin(6);
        m_sections->addWidget(empty);
        m_sections->addStretch();
        return;
    }

    // The library's categories in order, then any it does not list.
    QList<QPair<QString, QString>> categories;
    QSet<QString> known;
    for (const ClipartCategory &category : m_library.categories()) {
        categories.append({category.id, category.displayName()});
        known.insert(category.id);
    }
    for (const ClipartItem &item : items) {
        if (!known.contains(item.categoryId)) {
            categories.append({item.categoryId, i18n("Other")});
            known.insert(item.categoryId);
        }
    }

    const QStringList collapsed = settings().readEntry("Collapsed", QStringList());
    for (const auto &[id, name] : std::as_const(categories)) {
        auto *grid = new ClipartGrid;
        for (int i = 0; i < items.size(); ++i) {
            const ClipartItem &item = items.at(i);
            if (item.categoryId != id) {
                continue;
            }
            const QString path = m_library.path(item);
            auto *entry = new QListWidgetItem(QIcon(path), QString(), grid);
            entry->setData(PathRole, path);
            entry->setData(ItemRole, i);
            entry->setToolTip(QFileInfo(item.fileName).completeBaseName());
        }
        if (grid->count() == 0) {
            delete grid;
            continue;
        }

        auto *header = new QToolButton;
        header->setText(i18nc("clip art category, number of images", "%1 (%2)", name, grid->count()));
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setAutoRaise(true);
        header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);
        KAcceleratorManager::setNoAccel(header);
        // The arrow tells whether the section is open, as in a tree.
        header->setArrowType(collapsed.contains(id) ? Qt::RightArrow : Qt::DownArrow);
        grid->setVisible(!collapsed.contains(id));
        connect(header, &QToolButton::clicked, this, [this, header, grid]() {
            const bool expanded = header->arrowType() != Qt::DownArrow;
            header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            grid->setVisible(expanded);
            saveCollapsed();
        });

        connect(grid, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            Q_EMIT insertRequested(item->data(PathRole).toString());
        });
        // One selection across the sections, so Remove knows what it removes.
        connect(grid, &QListWidget::itemSelectionChanged, this, [this, grid]() {
            if (!grid->selectedItems().isEmpty()) {
                for (const Section &section : std::as_const(m_sectionList)) {
                    if (section.grid != grid) {
                        section.grid->clearSelection();
                    }
                }
            }
            m_remove->setEnabled(!selectedItems().isEmpty());
        });

        m_sections->addWidget(header);
        m_sections->addWidget(grid);
        m_sectionList.append({id, header, grid});
    }
    m_sections->addStretch();
}

void ClipartPanel::setAllExpanded(bool expanded)
{
    for (const Section &section : std::as_const(m_sectionList)) {
        section.header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        section.grid->setVisible(expanded);
    }
    saveCollapsed();
}

void ClipartPanel::saveCollapsed()
{
    QStringList collapsed;
    for (const Section &section : std::as_const(m_sectionList)) {
        if (section.header->arrowType() != Qt::DownArrow) {
            collapsed << section.categoryId;
        }
    }
    KConfigGroup group = settings();
    group.writeEntry("Collapsed", collapsed);
    group.sync();
}

QList<QListWidgetItem *> ClipartPanel::selectedItems() const
{
    QList<QListWidgetItem *> selected;
    for (const Section &section : m_sectionList) {
        selected << section.grid->selectedItems();
    }
    return selected;
}

void ClipartPanel::reportImport(int count, const QString &error)
{
    if (count == 0) {
        KMessageBox::error(this, error.isEmpty() ? i18n("Nothing was imported.") : error);
    } else {
        reload();
    }
}

void ClipartPanel::addImages()
{
    QStringList patterns;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
        patterns << u"*."_s + QString::fromLatin1(format);
    }
    const QStringList files = QFileDialog::getOpenFileNames(this, i18n("Add Clip Art"), QString(), i18n("Images (%1)", patterns.join(u' ')));
    if (files.isEmpty()) {
        return;
    }
    QString error;
    reportImport(m_library.importImages(files, ClipartLibrary::UserCategory, &error), error);
}

void ClipartPanel::removeSelected()
{
    const QList<QListWidgetItem *> selected = selectedItems();
    if (selected.isEmpty()
        || KMessageBox::warningContinueCancel(this, i18np("Remove this clip art?", "Remove these %1 clip art images?", selected.size()), i18n("Remove Clip Art"),
                                              KStandardGuiItem::remove())
            != KMessageBox::Continue) {
        return;
    }
    const QList<ClipartItem> items = m_library.items();
    for (QListWidgetItem *entry : selected) {
        m_library.remove(items.at(entry->data(ItemRole).toInt()));
    }
    reload();
}

void ClipartPanel::importDlsProgram()
{
    KMessageBox::information(this,
                             i18n("Choose <b>DYMO.DLS.dll</b> from the folder DYMO Label Software is installed in on Windows, usually "
                                  "C:\\Program Files (x86)\\DYMO\\DYMO Label Software. Plasma Label Creator copies the clip art from it into your library."),
                             i18n("Import DYMO Clip Art"),
                             u"ImportDlsClipartHint"_s);
    const QString file = QFileDialog::getOpenFileName(this, i18n("DYMO Label Software Program File"), QString(), i18n("DYMO.DLS.dll (*.dll)"));
    if (file.isEmpty()) {
        return;
    }
    QString error;
    reportImport(m_library.importDlsProgram(file, &error), error);
}

void ClipartPanel::importDlsCatalog()
{
    const QString file = QFileDialog::getOpenFileName(this, i18n("DYMO Clip Art List"), QString(), i18n("Clip art lists (Clipart.xml *.xml)"));
    if (file.isEmpty()) {
        return;
    }
    QString error;
    reportImport(m_library.importDlsCatalog(file, &error), error);
}

void ClipartPanel::exportLibrary()
{
    const QString directory = QFileDialog::getExistingDirectory(this, i18n("Export Clip Art to Folder"));
    if (directory.isEmpty()) {
        return;
    }
    QString error;
    if (!m_library.exportTo(directory, {}, &error)) {
        KMessageBox::error(this, error);
        return;
    }
    KMessageBox::information(this,
                             i18n("The clip art and a Clipart.xml listing it are in %1. In DYMO Label Software, add the images to My Clip Art.", directory));
}
