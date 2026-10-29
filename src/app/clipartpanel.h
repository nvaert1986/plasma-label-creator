/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clipartlibrary.h"

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

/**
 * The clip art library under the objects to insert, as in DLS 8: a section
 * per category that folds open and shut, images dragged onto the label.
 */
class ClipartPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ClipartPanel(QWidget *parent = nullptr);

Q_SIGNALS:
    /** The user wants the image at @p fileName on the label. */
    void insertRequested(const QString &fileName);

private:
    void reload();
    void fill();
    void addImages();
    void removeSelected();
    void importDlsProgram();
    void importDlsCatalog();
    void exportLibrary();
    void reportImport(int count, const QString &error);
    /** Opens or folds every category, as DLS's Show All and Hide All. */
    void setAllExpanded(bool expanded);
    void saveCollapsed();
    QList<QListWidgetItem *> selectedItems() const;

    struct Section {
        QString categoryId;
        QToolButton *header;
        QListWidget *grid;
    };

    LabelCreator::ClipartLibrary m_library;
    QScrollArea *m_scroll;
    QVBoxLayout *m_sections;
    QList<Section> m_sectionList;
    QToolButton *m_remove;
};
