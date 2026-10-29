/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "recentlayoutsmenu.h"

#include <KLocalizedString>

#include <QListWidget>
#include <QLocale>
#include <QWidgetAction>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int IndexRole = Qt::UserRole;
// Thumbnails are drawn at up to 160 pixels; most labels are wider than tall.
constexpr QSize IconSize(128, 80);
constexpr int Columns = 3;
constexpr int VisibleRows = 3;

} // namespace

RecentLayoutsMenu::RecentLayoutsMenu(RecentLayouts *layouts, QWidget *parent)
    : QMenu(parent)
    , m_layouts(layouts)
{
    // Built when opened, so labels printed meanwhile are there.
    connect(this, &QMenu::aboutToShow, this, &RecentLayoutsMenu::fill);
}

void RecentLayoutsMenu::fill()
{
    clear();
    const QList<RecentLayout> layouts = m_layouts->layouts();
    if (layouts.isEmpty()) {
        addAction(i18n("Labels you print appear here"))->setEnabled(false);
        return;
    }

    auto *list = new QListWidget;
    list->setViewMode(QListView::IconMode);
    list->setIconSize(IconSize);
    list->setGridSize(QSize(IconSize.width() + 24, IconSize.height() + 2 * list->fontMetrics().lineSpacing() + 12));
    list->setMovement(QListView::Static);
    list->setResizeMode(QListView::Adjust);
    list->setWordWrap(true);
    list->setUniformItemSizes(true);
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    const QLocale locale;
    for (int i = 0; i < layouts.size(); ++i) {
        const RecentLayout &layout = layouts.at(i);
        auto *item = new QListWidgetItem(QIcon(layout.thumbnail), layout.title, list);
        item->setData(IndexRole, i);
        item->setToolTip(i18nc("layout name, when it was printed", "%1\nPrinted %2", layout.title, locale.toString(layout.used, QLocale::ShortFormat)));
    }
    const int rows = std::min(VisibleRows, int((layouts.size() + Columns - 1) / Columns));
    list->setFixedSize(std::min<int>(Columns, int(layouts.size())) * list->gridSize().width() + list->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 8,
                       rows * list->gridSize().height() + 8);
    connect(list, &QListWidget::itemClicked, this, [this, layouts](QListWidgetItem *item) {
        close();
        Q_EMIT layoutChosen(layouts.at(item->data(IndexRole).toInt()));
    });

    auto *action = new QWidgetAction(this);
    action->setDefaultWidget(list);
    addAction(action);
    addSeparator();
    addAction(QIcon::fromTheme(u"edit-clear-history"_s), i18n("Clear List"), this, [this]() {
        m_layouts->clear();
    });
}
