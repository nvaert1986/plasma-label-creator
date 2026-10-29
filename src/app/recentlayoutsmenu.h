/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "recentlayouts.h"

#include <QMenu>

/**
 * The drop-down of DLS's "Recently used layouts" button: the labels printed
 * last, as thumbnails with their names, newest first.
 */
class RecentLayoutsMenu : public QMenu
{
    Q_OBJECT

public:
    explicit RecentLayoutsMenu(LabelCreator::RecentLayouts *layouts, QWidget *parent = nullptr);

Q_SIGNALS:
    void layoutChosen(const LabelCreator::RecentLayout &layout);

private:
    void fill();

    LabelCreator::RecentLayouts *m_layouts;
};
