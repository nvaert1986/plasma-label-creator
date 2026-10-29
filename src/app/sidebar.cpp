/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sidebar.h"

#include "addressbookpanel.h"
#include "clipartpanel.h"
#include "customlabels.h"
#include "dragsupport.h"
#include "units.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QTimer>
#include <QToolButton>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QSet>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int PaperRole = Qt::UserRole;
constexpr int GroupRole = Qt::UserRole + 1;

constexpr int KindRole = Qt::UserRole;
constexpr int DescriptionRole = Qt::UserRole + 1;

/** Shows an object to insert as DLS does: its icon, its name in bold and what it is for. */
class ObjectDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static constexpr int IconSize = 32;
    static constexpr int Margin = 6;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        const int text = 2 * option.fontMetrics.lineSpacing();
        return QSize(IconSize + 3 * Margin, std::max(IconSize, text) + 2 * Margin);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // The selection and hover background from the style, then our own contents.
        QStyleOptionViewItem background = option;
        initStyleOption(&background, index);
        background.text.clear();
        background.icon = QIcon();
        option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);

        const bool selected = option.state & QStyle::State_Selected;
        const QPalette::ColorGroup group = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
        const QRect area = option.rect.adjusted(Margin, Margin, -Margin, -Margin);
        const QRect iconRect(area.left(), area.top() + (area.height() - IconSize) / 2, IconSize, IconSize);
        index.data(Qt::DecorationRole).value<QIcon>().paint(painter, iconRect, Qt::AlignCenter, group == QPalette::Normal ? QIcon::Normal : QIcon::Disabled);

        const QRect textRect = area.adjusted(IconSize + Margin, 0, 0, 0);
        const int lineSpacing = option.fontMetrics.lineSpacing();
        const int top = textRect.top() + (textRect.height() - 2 * lineSpacing) / 2;
        QFont bold = option.font;
        bold.setBold(true);
        painter->save();
        painter->setFont(bold);
        painter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        const QRect titleRect(textRect.left(), top, textRect.width(), lineSpacing);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(bold).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, titleRect.width()));
        painter->setFont(option.font);
        QColor faint = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        faint.setAlphaF(selected ? 0.85 : 0.65);
        painter->setPen(faint);
        const QRect descriptionRect(textRect.left(), top + lineSpacing, textRect.width(), lineSpacing);
        painter->drawText(descriptionRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          option.fontMetrics.elidedText(index.data(DescriptionRole).toString(), Qt::ElideRight, descriptionRect.width()));
        painter->restore();
    }
};

QString paperDescription(const PaperSize &paper)
{
    // Grouped sizes are named with their size already; under their own
    // product heading the size alone says enough.
    if (!paper.group.isEmpty()) {
        return paper.name.startsWith(paper.group + u' ') ? paper.name.mid(paper.group.size() + 1) : paper.name;
    }
    const QSizeF mm = paper.size * Units::TwipsPerPoint;
    return i18nc("label name, width x height", "%1 (%2 × %3 mm)", paper.name, qRound(Units::twipsToMm(mm.width())), qRound(Units::twipsToMm(mm.height())));
}

} // namespace

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget;
    layout->addWidget(m_tabs, 1);

    // Label types, from the printer's driver.
    auto *labels = new QWidget;
    auto *labelsLayout = new QVBoxLayout(labels);
    m_paperFilter = new QLineEdit;
    m_paperFilter->setPlaceholderText(i18n("Search label types…"));
    m_paperFilter->setClearButtonEnabled(true);
    m_paperList = new QListWidget;
    m_paperList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_paperList->setTextElideMode(Qt::ElideRight);
    m_paperList->setUniformItemSizes(false);
    labelsLayout->addWidget(m_paperFilter);
    labelsLayout->addWidget(m_paperList, 1);
    m_tabs->addTab(labels, QIcon::fromTheme(u"document-new"_s), i18n("Labels"));
    connect(m_paperFilter, &QLineEdit::textChanged, this, &Sidebar::filterPaperSizes);
    connect(m_paperList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item->data(GroupRole).isValid()) {
            return;
        }
        const int index = item->data(PaperRole).toInt();
        if (index >= 0 && index < m_paperSizes.size()) {
            Q_EMIT paperChosen(m_paperSizes.at(index));
        }
    });
    // The user's own label sizes can be removed from their context menu.
    m_paperList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_paperList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QListWidgetItem *item = m_paperList->itemAt(pos);
        const int index = item && !item->data(GroupRole).isValid() ? item->data(PaperRole).toInt() : -1;
        if (index < 0 || index >= m_paperSizes.size()) {
            return;
        }
        const QString name = m_paperSizes.at(index).name;
        const QList<PaperSize> custom = CustomLabels::all();
        if (std::none_of(custom.cbegin(), custom.cend(), [&name](const PaperSize &paper) {
                return paper.name == name;
            })) {
            return;
        }
        QMenu menu;
        menu.addAction(QIcon::fromTheme(u"document-edit"_s), i18n("Edit This Label Size…"), this, [this, name]() {
            Q_EMIT customSizeEditRequested(name);
        });
        menu.addAction(QIcon::fromTheme(u"edit-delete"_s), i18n("Remove This Label Size"), this, [this, name]() {
            Q_EMIT customSizeRemovalRequested(name);
        });
        menu.exec(m_paperList->viewport()->mapToGlobal(pos));
    });
    // A group heading opens or folds its label types.
    connect(m_paperList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QVariant group = item->data(GroupRole);
        if (group.isValid()) {
            if (!m_openGroups.remove(group.toString())) {
                m_openGroups.insert(group.toString());
            }
            filterPaperSizes(m_paperFilter->text());
        }
    });

    // Objects to insert, dragged onto the label or double-clicked.
    m_insertList = new DragListWidget([](const QList<QListWidgetItem *> &items) {
        return DragSupport::objectMimeData(ObjectFactory::Kind(items.constFirst()->data(KindRole).toInt()));
    });
    m_insertList->setItemDelegate(new ObjectDelegate(m_insertList));
    m_insertList->setSelectionMode(QAbstractItemView::SingleSelection);
    const auto addItem = [this](const QString &icon, const QString &text, const QString &description, ObjectFactory::Kind kind) {
        auto *item = new QListWidgetItem(QIcon::fromTheme(icon), text, m_insertList);
        item->setData(KindRole, int(kind));
        item->setData(DescriptionRole, description);
        item->setToolTip(description);
    };
    addItem(u"view-pim-contacts"_s, i18n("Address"), i18n("Smart text box for addresses"), ObjectFactory::Kind::Address);
    addItem(u"draw-text"_s, i18n("Text"), i18n("Text box for static text"), ObjectFactory::Kind::Text);
    addItem(u"accessories-calculator"_s, i18n("Counter"), i18n("Automatic counter"), ObjectFactory::Kind::Counter);
    addItem(u"view-calendar-day"_s, i18n("Date and Time"), i18n("Updates the date and time automatically"), ObjectFactory::Kind::DateTime);
    addItem(u"view-barcode"_s, i18n("Barcode"), i18n("Range of barcode types"), ObjectFactory::Kind::Barcode);
    addItem(u"insert-image"_s, i18n("Image"), i18n("Picture from your files"), ObjectFactory::Kind::Image);
    addItem(u"draw-rectangle"_s, i18n("Rectangle"), i18n("Box with an outline and fill"), ObjectFactory::Kind::Rectangle);
    addItem(u"draw-ellipse"_s, i18n("Ellipse"), i18n("Oval or circle"), ObjectFactory::Kind::Ellipse);
    addItem(u"draw-line"_s, i18n("Horizontal Line"), i18n("Line across the label"), ObjectFactory::Kind::HorizontalLine);
    addItem(u"draw-line"_s, i18n("Vertical Line"), i18n("Line from top to bottom"), ObjectFactory::Kind::VerticalLine);
    addItem(u"draw-text"_s, i18n("Curved Text"), i18n("Text along an arc or circle"), ObjectFactory::Kind::CurvedText);

    // Objects above, the clip art library below, as in DLS.
    auto *insertSplitter = new QSplitter(Qt::Vertical);
    auto *objects = new QWidget;
    auto *objectsLayout = new QVBoxLayout(objects);
    objectsLayout->setContentsMargins(0, 0, 0, 0);
    objectsLayout->addWidget(new QLabel(i18n("<b>Label Objects</b>")));
    auto *objectsHint = new QLabel(i18n("Drag onto the label, or double-click"));
    objectsHint->setEnabled(false);
    objectsLayout->addWidget(objectsHint);
    objectsLayout->addWidget(m_insertList, 1);
    auto *clipart = new ClipartPanel;
    insertSplitter->addWidget(objects);
    insertSplitter->addWidget(clipart);
    insertSplitter->setStretchFactor(0, 2);
    insertSplitter->setStretchFactor(1, 3);
    insertSplitter->setSizes({2000, 3000}); // scaled to the space there is
    connect(clipart, &ClipartPanel::insertRequested, this, &Sidebar::clipartRequested);
    m_tabs->addTab(insertSplitter, QIcon::fromTheme(u"insert-object"_s), i18n("Insert"));

    m_addressBook = new AddressBookPanel;
    m_tabs->addTab(m_addressBook, QIcon::fromTheme(u"view-pim-contacts"_s), i18n("Address Book"));
    connect(m_insertList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        Q_EMIT insertRequested(ObjectFactory::Kind(item->data(KindRole).toInt()));
    });
}

void Sidebar::setPaperSizes(const QList<PaperSize> &paperSizes)
{
    // A new set of groups starts with the first one open: the label types
    // that fit the loaded labels, when the printer said which those are.
    const auto groups = [](const QList<PaperSize> &sizes) {
        QStringList list;
        for (const PaperSize &paper : sizes) {
            if (!paper.group.isEmpty() && !list.contains(paper.group)) {
                list << paper.group;
            }
        }
        return list;
    };
    const QStringList newGroups = groups(paperSizes);
    if (newGroups != groups(m_paperSizes)) {
        // Groups still there stay as they were; the first one opens.
        QSet<QString> open;
        for (const QString &group : newGroups) {
            if (m_openGroups.contains(group)) {
                open.insert(group);
            }
        }
        if (!newGroups.isEmpty()) {
            open.insert(newGroups.constFirst());
        }
        m_openGroups = open;
    }
    m_paperSizes = paperSizes;
    filterPaperSizes(m_paperFilter->text());
}

void Sidebar::filterPaperSizes(const QString &text)
{
    m_paperList->clear();
    QString group;
    for (int i = 0; i < m_paperSizes.size(); ++i) {
        const PaperSize &paper = m_paperSizes.at(i);
        if (!text.isEmpty() && !paper.name.contains(text, Qt::CaseInsensitive)) {
            continue;
        }
        // A heading per group; searching shows every match, open or not.
        const bool open = !text.isEmpty() || m_openGroups.contains(paper.group);
        if (paper.group != group) {
            group = paper.group;
            if (!group.isEmpty()) {
                auto *heading = new QListWidgetItem(QIcon::fromTheme(open ? u"go-down"_s : u"go-next"_s), group, m_paperList);
                QFont font = heading->font();
                font.setBold(true);
                heading->setFont(font);
                heading->setData(GroupRole, group);
                heading->setFlags(Qt::ItemIsEnabled);
            }
        }
        if (!paper.group.isEmpty() && !open) {
            continue;
        }
        // Indented under a group heading.
        auto *item = new QListWidgetItem(paper.group.isEmpty() ? paperDescription(paper) : u"      "_s + paperDescription(paper), m_paperList);
        item->setToolTip(paper.name);
        item->setData(PaperRole, i);
        if (paper.name == m_currentPaper) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            m_paperList->setCurrentItem(item);
        }
    }
    // The list is new; keep the label type in use in view.
    if (QListWidgetItem *current = m_paperList->currentItem()) {
        m_paperList->scrollToItem(current, QAbstractItemView::PositionAtCenter);
    }
    if (m_paperSizes.isEmpty()) {
        auto *item = new QListWidgetItem(i18n("Select a label printer below to see its label types."), m_paperList);
        item->setFlags(Qt::NoItemFlags);
    }
}

void Sidebar::setCurrentPaper(const QString &paperName)
{
    if (m_currentPaper != paperName) {
        m_currentPaper = paperName;
        filterPaperSizes(m_paperFilter->text());
    }
}

void Sidebar::showLabelTypes()
{
    // To the label type in use: its group open, selected and in view, so
    // the arrow keys go through the types around it.
    m_tabs->setCurrentIndex(0);
    const auto current = std::find_if(m_paperSizes.cbegin(), m_paperSizes.cend(), [this](const PaperSize &paper) {
        return paper.name == m_currentPaper;
    });
    if (current == m_paperSizes.cend()) {
        m_paperFilter->setFocus();
        return;
    }
    if (!current->group.isEmpty()) {
        m_openGroups.insert(current->group);
    }
    if (!current->name.contains(m_paperFilter->text(), Qt::CaseInsensitive)) {
        const QSignalBlocker blocker(m_paperFilter);
        m_paperFilter->clear();
    }
    filterPaperSizes(m_paperFilter->text());
    for (int i = 0; i < m_paperList->count(); ++i) {
        QListWidgetItem *item = m_paperList->item(i);
        const int index = item->data(GroupRole).isValid() ? -1 : item->data(PaperRole).toInt();
        if (index >= 0 && index < m_paperSizes.size() && m_paperSizes.at(index).name == m_currentPaper) {
            m_paperList->setCurrentItem(item);
            m_paperList->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            break;
        }
    }
    m_paperList->setFocus();
}

void Sidebar::showInsert()
{
    m_tabs->setCurrentIndex(1);
}

void Sidebar::showAddressBook()
{
    m_tabs->setCurrentIndex(2);
}

AddressBookPanel *Sidebar::addressBook() const
{
    return m_addressBook;
}

